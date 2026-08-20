// CUDA rcssemptynet — standalone env source for --cu builds.
// Separate from rcssemptynet.h (CPU). One device Env per match; trainer IO
// is per-agent. Obs is always bf16; pufferl casts to train precision if needed.
#ifndef PUFFER_RCSSEMPTYNET_GPU_CU
#define PUFFER_RCSSEMPTYNET_GPU_CU

#define PUF_BACKEND PUF_GPU
#define PUF_GPU_ENV 1

#define TIME_REFEREE_ENABLED 0
#define BALL_STUCK_REFEREE_ENABLED 0
#define OFFSIDE_REFEREE_ENABLED 0
#define FREE_KICK_REFEREE_ENABLED 0
#define TOUCH_REFEREE_ENABLED 1
#define CATCH_REFEREE_ENABLED 1
#define FOUL_REFEREE_ENABLED 1
#define PENALTY_REFEREE_ENABLED 0

#define LEFT_PLAYERS_COUNT 2
#define RIGHT_PLAYERS_COUNT 0
#define PUF_AGENTS_PER_ENV (LEFT_PLAYERS_COUNT + RIGHT_PLAYERS_COUNT)

#define HALF_TICKS 3000

#include <cuda_bf16.h>
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef __nv_bfloat16 obs_t;
#include "pufferenv.h"
#include "rcsscommonv2.h"
#include "render.h"

typedef struct {
    float goal;
    float fault;
    float stagnate;
    float step;
} Reward;

#define REWARD_STRIDE ((int)(sizeof(Reward) / sizeof(float)))
#define OBS_TASK_STRIDE 1
#define OBS_SIZE (OBS_SELF_INDEX_STRIDE + OBS_TIME_STRIDE + OBS_TASK_STRIDE + OBS_BALL_STRIDE + OBS_SELF_STAMINA + REWARD_STRIDE \
    + NUM_PLAYERS * OBS_PLAYER_STRIDE)

struct Log {
    float perf;
    float score;
    float episode_return;
    float episode_length;
    float difficulty;
    float success_rate;
    float left_goal;
    float right_goal;
    float fault_left;
    float fault_right;
    float stagnate;
    float timeout;
    float n;
};

struct Env {
    Log log;
    Agent agents[NUM_PLAYERS];
    int tag;
    int boundary_reached;
    int num_agents;
    unsigned int rng;

    Reward reward_struct[NUM_PLAYERS];
    Reward returns[NUM_PLAYERS];
    int players_tick[NUM_PLAYERS];

    Stadium stadium;
    int side;

    int tick;
    int tick_since_last_goal;
    int starting_horizon_ticks;
    int early_terminal;

    float goal_reward;
    float fault_reward;
    float no_goal_timeout;
    float no_goal_reward;
    float step_reward;

    int episode_left_goal;
    int episode_right_goal;
    int episode_fault_left;
    int episode_fault_right;
    int episode_stagnate;
    int episode_timeout;

    float success_rate;
    float difficulty;
    float success_update_rate;
    float target_success_rate;
    float difficulty_rate;
};

#define RCSS_BLOCK 256
static int rcss_grid(int n) {
    return (n + RCSS_BLOCK - 1) / RCSS_BLOCK;
}

static struct {
    Env* envs;
    int n;
    obs_t* observations;
    float* actions;
    float* rewards;
    float* terminals;
    cudaStream_t stream;
} g_gpu;

void puf_log(Log* log, Dict* out) {
    dict_set(out, "perf", log->perf);
    dict_set(out, "score", log->score);
    dict_set(out, "episode_return", log->episode_return);
    dict_set(out, "episode_length", log->episode_length);
    dict_set(out, "difficulty", log->difficulty);
    dict_set(out, "success_rate", log->success_rate);
    dict_set(out, "left_goal", log->left_goal);
    dict_set(out, "right_goal", log->right_goal);
    dict_set(out, "fault_left", log->fault_left);
    dict_set(out, "fault_right", log->fault_right);
    dict_set(out, "stagnate", log->stagnate);
    dict_set(out, "timeout", log->timeout);
    dict_set(out, "n", log->n);
}

__device__ __forceinline__ void gpu_store_obs(obs_t* dst, float v) {
    *dst = __float2bfloat16(v);
}

__device__ static inline float gpu_sign(float x) {
    return (0.0f < x) - (x < 0.0f);
}

__device__ static inline int gpu_agent_row(int match, int a) {
    return match * NUM_PLAYERS + a;
}

__device__ static void gpu_spawn_box(float difficulty, int side, float* x0, float* x1, float* y_half) {
    const float easy_near = 44.0f;
    const float easy_far = PITCH_LENGTH * 0.5f - 1.5f;
    const float easy_y = 5.0f;
    const float hard_x0 = -PITCH_LENGTH * 0.5f + 1.0f;
    const float hard_x1 = PITCH_LENGTH * 0.5f - 2.0f;
    const float hard_y = PITCH_WIDTH * 0.5f - 1.0f;
    float p = difficulty;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    if (side == SIDE_LEFT) {
        *x0 = easy_near + p * (hard_x0 - easy_near);
        *x1 = easy_far + p * (hard_x1 - easy_far);
    } else {
        *x0 = -easy_far + p * (hard_x0 - (-easy_far));
        *x1 = -easy_near + p * (hard_x1 - (-easy_near));
    }
    *y_half = easy_y + p * (hard_y - easy_y);
}

__device__ static void gpu_place_ball(Env* env, float* ball_x, float* ball_y, float x0, float x1, float y_half) {
    *ball_x = drand(&env->stadium.seed, x0, x1);
    *ball_y = drand(&env->stadium.seed, -y_half, y_half);
    stadium_moveBall(&env->stadium, *ball_x, *ball_y, 0, 0);
}

__device__ static void gpu_place_player(Env* env, int player_index, float ball_x, float ball_y) {
    const float xmin = -PITCH_LENGTH * 0.5f + 1.0f;
    const float xmax = PITCH_LENGTH * 0.5f - 1.0f;
    const float ymax = PITCH_WIDTH * 0.5f - 1.0f;
    float d = env->difficulty;
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;
    float rad = 3.0f + d * (40.0f - 3.0f);
    float p_x = ball_x, p_y = ball_y;
    for (int attempt = 0; attempt < 24; attempt++) {
        float ang = drand(&env->stadium.seed, -M_PIf, M_PIf);
        p_x = ball_x + rad * cosf(ang);
        p_y = ball_y + rad * sinf(ang);
        if (p_x >= xmin && p_x <= xmax && fabsf(p_y) <= ymax)
            break;
        if (attempt == 23) {
            p_x = fminf(xmax, fmaxf(xmin, p_x));
            p_y = fminf(ymax, fmaxf(-ymax, p_y));
        }
    }
    float body_ang = atan2f(ball_y - p_y, ball_x - p_x) * (180.0f / M_PIf);
    body_ang = normalize_angle_deg(body_ang + drand(&env->stadium.seed, -25.0f, 25.0f));
    stadium_movePlayer(&env->stadium, player_index, p_x, p_y, body_ang, 0, 0);
}

__device__ static void gpu_ensure_play_on(Env* env) {
    stadium_step(&env->stadium);
    if (env->stadium.playmode != PM_PlayOn) {
        changePlayMode(&env->stadium, PM_PlayOn);
        stadium_step(&env->stadium);
    }
}

__device__ static void gpu_spawn(Env* env) {
    float x0, x1, y_half;
    gpu_spawn_box(env->difficulty, env->side, &x0, &x1, &y_half);
    float ball_x, ball_y;
    gpu_place_ball(env, &ball_x, &ball_y, x0, x1, y_half);
    for (int a = 0; a < env->num_agents; a++)
        gpu_place_player(env, a, ball_x, ball_y);
    gpu_ensure_play_on(env);
}

__device__ static void gpu_reset_episode(Env* env) {
    for (int a = 0; a < env->num_agents; a++) {
        env->players_tick[a] = 0;
        env->returns[a].goal = 0.0f;
        env->returns[a].fault = 0.0f;
        env->returns[a].stagnate = 0.0f;
        env->returns[a].step = 0.0f;
    }
    env->tick = 0;
    env->tick_since_last_goal = 0;
    env->episode_left_goal = 0;
    env->episode_right_goal = 0;
    env->episode_fault_left = 0;
    env->episode_fault_right = 0;
    env->episode_stagnate = 0;
    env->episode_timeout = 0;

    stadium_reset(&env->stadium);
    changePlayMode(&env->stadium, PM_PlayOn);
    gpu_spawn(env);
}

__device__ static void gpu_compute_observations(Env* env, obs_t* observations, int match) {
    for (int a = 0; a < env->num_agents; a++) {
        float tmp[OBS_SIZE];
        int idx = 0;
        tmp[idx++] = (float)env->tick / (float)env->starting_horizon_ticks;
        tmp[idx++] = ballKickable(&env->stadium, a) ? 1.0f : 0.0f;
        pack_observations_common(&env->stadium, a, env->num_agents, env->side, tmp, &idx);
        tmp[idx++] = gpu_sign(env->reward_struct[a].goal);
        tmp[idx++] = gpu_sign(env->reward_struct[a].fault);
        tmp[idx++] = gpu_sign(env->reward_struct[a].stagnate);
        tmp[idx++] = gpu_sign(env->reward_struct[a].step);
        obs_t* obs = observations + (long)gpu_agent_row(match, a) * OBS_SIZE;
        for (int i = 0; i < OBS_SIZE; i++)
            gpu_store_obs(&obs[i], tmp[i]);
    }
}

__device__ static float gpu_remaining_time_frac(const Env* env) {
    float elapsed = (float)env->tick / (float)env->starting_horizon_ticks;
    if (elapsed > 1.0f) elapsed = 1.0f;
    return 1.0f - elapsed;
}

__device__ static int gpu_goal_diff(const Env* env) {
    int goals_for = (env->side == SIDE_LEFT)
        ? env->episode_left_goal : env->episode_right_goal;
    int goals_against = (env->side == SIDE_LEFT)
        ? env->episode_right_goal : env->episode_left_goal;
    return goals_for - goals_against;
}

__device__ static int gpu_episode_success(const Env* env) {
    return gpu_goal_diff(env) > 0;
}

__device__ static float gpu_player_return(const Reward* ret) {
    return ret->goal + ret->fault + ret->stagnate + ret->step;
}

__device__ static float gpu_score_early_terminal(const Env* env) {
    if (gpu_episode_success(env))
        return 1.0f;
    if (env->episode_timeout)
        return 0.0f;
    return -1.0f;
}

__device__ static float gpu_perf_early_terminal(const Env* env) {
    if (!gpu_episode_success(env))
        return 0.0f;
    return gpu_remaining_time_frac(env);
}

__device__ static int gpu_fixed_horizon_ticks(const Env* env) {
    int start = env->starting_horizon_ticks;
    if (start < 1) start = 1;
    if (start >= HALF_TICKS) return start;
    float difficulty = env->difficulty;
    if (difficulty < 0.0f) difficulty = 0.0f;
    if (difficulty > 1.0f) difficulty = 1.0f;
    return start + (int)(difficulty * (float)(HALF_TICKS - start) + 0.5f);
}

__device__ static float gpu_score_fixed_horizon(const Env* env) {
    return (float)gpu_goal_diff(env);
}

__device__ static float gpu_perf_fixed_horizon(const Env* env) {
    int goals_for = (env->side == SIDE_LEFT)
        ? env->episode_left_goal : env->episode_right_goal;
    if (goals_for <= 0)
        return 0.0f;
    int horizon = gpu_fixed_horizon_ticks(env);
    if (horizon < 1) horizon = 1;
    float start = (float)env->starting_horizon_ticks;
    if (start < 1.0f) start = 1.0f;
    return (float)goals_for * start / (float)horizon;
}

__device__ static float gpu_goal_event_reward(Env* env, int scored_side) {
    if (scored_side == SIDE_LEFT)
        env->episode_left_goal++;
    else
        env->episode_right_goal++;
    const int we_scored = (scored_side == env->side);
    return we_scored ? env->goal_reward : -env->goal_reward;
}

__device__ static float gpu_fault_event_reward(Env* env, int fault_side) {
    if (fault_side == SIDE_LEFT)
        env->episode_fault_left++;
    else
        env->episode_fault_right++;
    return (fault_side == env->side) ? env->fault_reward : 0.0f;
}

__device__ static void gpu_apply_agent_rewards(Env* env, Reward r, int terminal,
        float* rewards, float* terminals, int match) {
    float total = r.goal + r.fault + r.stagnate + r.step;
    for (int a = 0; a < env->num_agents; a++) {
        env->reward_struct[a] = r;
        env->returns[a].goal += r.goal;
        env->returns[a].fault += r.fault;
        env->returns[a].stagnate += r.stagnate;
        env->returns[a].step += r.step;
        int row = gpu_agent_row(match, a);
        rewards[row] = total;
        terminals[row] = terminal ? 1.0f : 0.0f;
    }
}

__device__ static void gpu_add_player_log(Env* env, int player_index) {
    Reward* ret = &env->returns[player_index];
    Log* log = &env->log;
    log->episode_return += gpu_player_return(ret);
    log->episode_length += (float)env->players_tick[player_index];
    log->left_goal += (float)env->episode_left_goal;
    log->right_goal += (float)env->episode_right_goal;
    log->fault_left += (float)env->episode_fault_left;
    log->fault_right += (float)env->episode_fault_right;
    log->stagnate += (float)env->episode_stagnate;
    log->timeout += (float)env->episode_timeout;
    log->perf += env->early_terminal
        ? gpu_perf_early_terminal(env)
        : gpu_perf_fixed_horizon(env);
    log->score += env->early_terminal
        ? gpu_score_early_terminal(env)
        : gpu_score_fixed_horizon(env);
    log->difficulty += env->difficulty;
    log->success_rate += env->success_rate;
    log->n += 1.0f;
}

__device__ static void gpu_add_log(Env* env) {
    for (int i = 0; i < env->num_agents; i++)
        gpu_add_player_log(env, i);
}

__device__ static void gpu_update_difficulty(Env* env) {
    float rate = env->success_update_rate;
    env->success_rate = (1.0f - rate) * env->success_rate
        + rate * (float)gpu_episode_success(env);
    if (env->difficulty_rate > 0.0f) {
        env->difficulty += env->difficulty_rate
            * (env->success_rate - env->target_success_rate);
        if (env->difficulty < 0.0f) env->difficulty = 0.0f;
        if (env->difficulty > 1.0f) env->difficulty = 1.0f;
    }
}

__device__ static void gpu_handle_early_terminal(Env* env, PlayMode pm,
        float* rewards, float* terminals, int match) {
    Reward r = {0};
    r.step = env->step_reward;
    int terminal = 0;

    if (is_goal(pm)) {
        r.goal = gpu_goal_event_reward(env, goal_side(pm));
        if (r.goal > 0.0f)
            r.goal *= gpu_remaining_time_frac(env);
        terminal = 1;
    } else if (is_fault(pm)) {
        r.fault = gpu_fault_event_reward(env, side_at_fault(pm));
        terminal = 1;
    } else if (env->tick >= env->starting_horizon_ticks) {
        r.stagnate = env->no_goal_reward;
        env->episode_timeout = 1;
        terminal = 1;
    }

    gpu_apply_agent_rewards(env, r, terminal, rewards, terminals, match);
}

__device__ static void gpu_handle_fixed_horizon(Env* env, PlayMode pm,
        float* rewards, float* terminals, int match) {
    const int horizon = gpu_fixed_horizon_ticks(env);
    const int terminal = (env->tick >= horizon);
    Reward r = {0};
    int soft = 0;

    if (is_goal(pm)) {
        r.goal = gpu_goal_event_reward(env, goal_side(pm));
        soft = !terminal;
    } else if (is_fault(pm)) {
        r.fault = gpu_fault_event_reward(env, side_at_fault(pm));
        soft = !terminal;
    } else if (!terminal
               && is_play_on(pm)
               && env->no_goal_timeout > 0.0f
               && env->tick_since_last_goal >= (int)env->no_goal_timeout) {
        r.stagnate = env->no_goal_reward;
        env->tick_since_last_goal = 0;
        env->episode_stagnate++;
        soft = 1;
    }

    gpu_apply_agent_rewards(env, r, terminal, rewards, terminals, match);
    if (soft) {
        changePlayMode(&env->stadium, PM_PlayOn);
        gpu_spawn(env);
    }
}

__device__ static void gpu_handle_rewards_and_terminals(Env* env,
        float* rewards, float* terminals, int match) {
    PlayMode pm = env->stadium.playmode;
    if (env->early_terminal)
        gpu_handle_early_terminal(env, pm, rewards, terminals, match);
    else
        gpu_handle_fixed_horizon(env, pm, rewards, terminals, match);

    if (terminals[gpu_agent_row(match, 0)] == 1.0f) {
        gpu_add_log(env);
        gpu_update_difficulty(env);
        gpu_reset_episode(env);
    }
}

__device__ static void gpu_step_match(Env* env, const float* actions,
        obs_t* observations, float* rewards, float* terminals, int match) {
    for (int a = 0; a < env->num_agents; a++)
        env->players_tick[a] += 1;

    for (int a = 0; a < env->num_agents; a++) {
        env->reward_struct[a].goal = 0.0f;
        env->reward_struct[a].fault = 0.0f;
        env->reward_struct[a].stagnate = 0.0f;
        env->reward_struct[a].step = 0.0f;
    }

    env->tick += 1;
    env->tick_since_last_goal += 1;

    for (int a = 0; a < env->num_agents; a++) {
        const float* act = actions + (long)gpu_agent_row(match, a) * NUM_ATNS;
        act_one(&env->stadium, a, act);
    }

    stadium_step(&env->stadium);
    gpu_handle_rewards_and_terminals(env, rewards, terminals, match);
    gpu_compute_observations(env, observations, match);
}

__global__ void gpu_rcss_reset_kernel(Env* envs, obs_t* observations,
        float* rewards, float* terminals, int num_matches) {
    int match = blockIdx.x * blockDim.x + threadIdx.x;
    if (match >= num_matches)
        return;
    Env* env = &envs[match];
    gpu_reset_episode(env);
    for (int a = 0; a < env->num_agents; a++) {
        int row = gpu_agent_row(match, a);
        rewards[row] = 0.0f;
        terminals[row] = 0.0f;
        env->reward_struct[a].goal = 0.0f;
        env->reward_struct[a].fault = 0.0f;
        env->reward_struct[a].stagnate = 0.0f;
        env->reward_struct[a].step = 0.0f;
    }
    gpu_compute_observations(env, observations, match);
}

__global__ void gpu_rcss_step_kernel(Env* __restrict__ envs,
        const float* __restrict__ actions, obs_t* __restrict__ observations,
        float* __restrict__ rewards, float* __restrict__ terminals,
        int num_matches) {
    int match = blockIdx.x * blockDim.x + threadIdx.x;
    if (match >= num_matches)
        return;
    gpu_step_match(&envs[match], actions, observations, rewards, terminals, match);
}

static void gpu_fill_env(Env* env, Dict* kwargs, unsigned int rng) {
    memset(env, 0, sizeof(*env));
    env->num_agents = NUM_PLAYERS;
    env->rng = rng;
    env->side = LEFT_PLAYERS_COUNT > 0 ? SIDE_LEFT : SIDE_RIGHT;
    env->starting_horizon_ticks = (int)dict_get(kwargs, "starting_horizon_ticks");
    env->goal_reward = (float)dict_get(kwargs, "goal_reward");
    env->fault_reward = (float)dict_get(kwargs, "fault_reward");
    env->no_goal_timeout = (float)dict_get(kwargs, "no_goal_timeout");
    env->no_goal_reward = (float)dict_get(kwargs, "no_goal_reward");
    env->early_terminal = (int)dict_get(kwargs, "early_terminal");
    env->step_reward = (float)dict_get(kwargs, "step_reward");
    env->difficulty = (float)dict_get(kwargs, "difficulty");
    env->success_update_rate = (float)dict_get(kwargs, "success_update_rate");
    env->target_success_rate = (float)dict_get(kwargs, "target_success_rate");
    env->difficulty_rate = (float)dict_get(kwargs, "difficulty_rate");
    env->success_rate = 0.0f;
    stadium_init(&env->stadium, env->rng);
}

Env* puf_vec_create(int n, Dict* env_kwargs,
        obs_t* observations, float* actions, float* rewards, float* terminals) {
    Env* host_envs = (Env*)calloc((size_t)n, sizeof(Env));
    for (int i = 0; i < n; i++)
        gpu_fill_env(&host_envs[i], env_kwargs, (unsigned int)(i + 1));

    Env* envs = NULL;
    cudaMalloc((void**)&envs, (size_t)n * sizeof(Env));
    cudaMemcpy(envs, host_envs, (size_t)n * sizeof(Env), cudaMemcpyHostToDevice);
    free(host_envs);

    g_gpu.envs = envs;
    g_gpu.n = n;
    g_gpu.observations = observations;
    g_gpu.actions = actions;
    g_gpu.rewards = rewards;
    g_gpu.terminals = terminals;
    g_gpu.stream = 0;
    return envs;
}

void puf_bind_stream(cudaStream_t stream) {
    g_gpu.stream = stream;
}

void puf_init(Env* env, Dict* kwargs) {
    (void)env;
    (void)kwargs;
}

void puf_reset(Env* env) {
    (void)env;
    gpu_rcss_reset_kernel<<<rcss_grid(g_gpu.n), RCSS_BLOCK>>>(
        g_gpu.envs, g_gpu.observations, g_gpu.rewards, g_gpu.terminals, g_gpu.n);
}

void puf_step(Env* env) {
    (void)env;
    gpu_rcss_step_kernel<<<rcss_grid(g_gpu.n), RCSS_BLOCK, 0, g_gpu.stream>>>(
        g_gpu.envs, g_gpu.actions, g_gpu.observations, g_gpu.rewards, g_gpu.terminals,
        g_gpu.n);
}

void puf_close(Env* env) {
    (void)env;
    if (IsWindowReady())
        CloseWindow();
    cudaFree(g_gpu.envs);
    g_gpu.envs = NULL;
}

void puf_render(Env* env) {
    (void)env;
    if (!g_gpu.envs || g_gpu.n < 1)
        return;
    if (g_gpu.stream)
        cudaStreamSynchronize(g_gpu.stream);
    Env h;
    cudaMemcpy(&h, g_gpu.envs, sizeof(Env), cudaMemcpyDeviceToHost);
    if (!IsWindowReady()) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(960, 680, "PufferLib RCSS (GPU)");
        SetTargetFPS(60);
    }
    if (IsKeyDown(KEY_ESCAPE))
        exit(0);
    render(&h.stadium);
    puf_web_vsync();
}

#endif
