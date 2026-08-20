/* RcssEmptyNet: empty-net scoring curriculum for PufferLib Ocean.
 */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float obs_t;
#include "pufferenv.h"
#include "rcsscommon.h"

#define NUM_PLAYERS 3
#define NUM_IMAGINARY_OPPONENTS 3
#define HALF_TICKS 3000
#define PUF_STEPS_PER_SEC 10

typedef struct {
    float goal;
    float fault;
    float stagnate;
    float step;
} Reward;

#define REWARD_STRIDE ((int)(sizeof(Reward) / sizeof(float)))
#define OBS_SIZE (OBS_BALL_STRIDE + OBS_SELF_STAMINA + REWARD_STRIDE \
    + NUM_PLAYERS * OBS_PLAYER_STRIDE + NUM_IMAGINARY_OPPONENTS * OBS_PLAYER_STRIDE)

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

    Reward* reward_struct;
    Reward* returns;
    int* players_tick;

    MyStadium* stadium;
    MyPlayer** players;
    MyPlayer* filler_left;
    int side;
    bool synch_mode;

    bool boostrap_done;
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

    bool debugging_enabled;
    FILE* log_file;
    int use_monitor;
};
typedef Env RcssEmptyNet;

void puf_close(RcssEmptyNet* env);

static void configure_stadium_paths(RcssEmptyNet* env) {
    // stadium_set_sense_body_enabled(env->stadium, true);
    // stadium_set_visual_enabled(env->stadium, true);
    // stadium_set_ball_compute_path(env->stadium, MY_BALL_PATH_HIGH);
    // stadium_set_player_compute_path(env->stadium, MY_PLAYER_PATH_HIGH);
    // landmark_noisy_dist_table_init();
    // stadium_set_flag_compute_path(env->stadium, MY_FLAG_PATH_V2);
    (void)env;
}

void bootstrap(RcssEmptyNet* env) {
    env->filler_left = NULL;

    // RCSS assigns the first initialized team name to LEFT. If RL plays RIGHT,
    // claim LEFT with a non-RL filler so t2 becomes the physical RIGHT side.
    if (env->side == SIDE_RIGHT) {
        env->filler_left = stadium_doInitPlayer(env->stadium, "t1", 18.0, false);
        if (!env->filler_left) {
            fprintf(stderr, "RcssEmptyNet: failed to init LEFT filler for RIGHT side\n");
            exit(1);
        }
    }

    const char* team = (env->side == SIDE_LEFT) ? "t1" : "t2";
    for (int a = 0; a < env->num_agents; a++) {
        env->players[a] = stadium_doInitPlayer(env->stadium, team, 18.0, false);
        if (!env->players[a]) {
            fprintf(stderr, "RcssEmptyNet: failed to init player %d on side %d\n",
                    a, env->side);
            exit(1);
        }
    }

    stadium_doStep(env->stadium);
    stadium_doStep(env->stadium);
    if (env->num_agents > 0 && env->players[0]) {
        player_turn(env->players[0], 1);
    }
    stadium_changePlayMode(env->stadium, MY_PM_PLAY_ON);
    stadium_doStep(env->stadium);
}

void init(RcssEmptyNet* env) {
    env->log_file = NULL;
    env->boostrap_done = false;
    env->filler_left = NULL;
    env->players = NULL;
    env->success_rate = 0.0f;
    if (env->use_monitor < 0) env->use_monitor = 0;
    if (env->starting_horizon_ticks <= 0) env->starting_horizon_ticks = 2000;
    if (env->success_update_rate <= 0.0f) env->success_update_rate = 0.02f;
    if (env->difficulty_rate < 0.0f) env->difficulty_rate = 0.05f;
    if (env->difficulty < 0.0f) env->difficulty = 0.0f;
    if (env->difficulty > 1.0f) env->difficulty = 1.0f;

    env->returns = (Reward*)calloc(env->num_agents, sizeof(Reward));
    env->reward_struct = (Reward*)calloc(env->num_agents, sizeof(Reward));
    env->players_tick = (int*)calloc(env->num_agents, sizeof(int));
    env->players = (MyPlayer**)calloc((size_t)env->num_agents, sizeof(MyPlayer*));

    init_rcss(env->rng, env->use_monitor, env->synch_mode, &env->stadium);
    configure_stadium_paths(env);
}

static void spawn_box(float difficulty, int side, float* x0, float* x1, float* y_half) {
    const float easy_near = 38.0f; // |x| of near edge of easy box
    const float easy_far = 50.0f;  // |x| of far edge of easy box
    const float easy_y = 7.0f;
    const float hard_x0 = -(float)PITCH_LENGTH * 0.5f + 1.0f; // ~-51.5
    const float hard_x1 = (float)PITCH_LENGTH * 0.5f - 2.0f;  // ~50.5
    const float hard_y = (float)PITCH_WIDTH * 0.5f - 1.0f;    // ~33
    float p = difficulty;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    if (side == SIDE_LEFT) {
        // Easy near RIGHT goal (attack +x).
        *x0 = easy_near + p * (hard_x0 - easy_near);
        *x1 = easy_far + p * (hard_x1 - easy_far);
    } else {
        // Easy near LEFT goal (attack −x).
        *x0 = -easy_far + p * (hard_x0 - (-easy_far));
        *x1 = -easy_near + p * (hard_x1 - (-easy_near));
    }
    *y_half = easy_y + p * (hard_y - easy_y);
}

static void place_ball(RcssEmptyNet* env, float* ball_x, float* ball_y, float x0, float x1, float y_half) {
    *ball_x = randf(&env->rng, x0, x1);
    *ball_y = randf(&env->rng, -y_half, y_half);
    stadium_moveBall(env->stadium, *ball_x, *ball_y, 0, 0);
}

static void place_player(RcssEmptyNet* env, int player_index, float ball_x, float ball_y, float x0, float x1, float y_half) {
    float p_x = randf(&env->rng, x0, x1);
    float p_y = randf(&env->rng, -y_half, y_half);
    float body_ang = atan2f(ball_y - p_y, ball_x - p_x) * (180.0f / PI_F);
    body_ang = normalize_angle(body_ang + randf(&env->rng, -25.0f, 25.0f));
    stadium_movePlayer(env->stadium, env->players[player_index], p_x, p_y, body_ang, 0, 0);
}

static void ensure_play_on(RcssEmptyNet* env) {
    stadium_doStep(env->stadium);
    if (stadium_playmode(env->stadium) != MY_PM_PLAY_ON) {
        stadium_changePlayMode(env->stadium, MY_PM_PLAY_ON);
        stadium_doStep(env->stadium);
    }
}

static void park_filler(RcssEmptyNet* env) {
    if (env->filler_left)
        stadium_movePlayer(env->stadium, env->filler_left, -55.0, -35.0, 0, 0, 0);
}

static void clear_observations(RcssEmptyNet* env) {
    for (int a = 0; a < env->num_agents; a++)
        memset(env->agents[a].observations, 0, (size_t)OBS_SIZE * sizeof(obs_t));
}

static float goal_event_reward(RcssEmptyNet* env, int scored_side) {
    if (scored_side == SIDE_LEFT)
        env->episode_left_goal++;
    else
        env->episode_right_goal++;

    const int we_scored = (scored_side == env->side);
    float r = we_scored ? env->goal_reward : -env->goal_reward;
    if (we_scored) {
        r /= (env->tick_since_last_goal / 10.0);
        env->tick_since_last_goal = 0;
    }
    return r;
}

static float fault_event_reward(RcssEmptyNet* env, int fault_side) {
    if (fault_side == SIDE_LEFT)
        env->episode_fault_left++;
    else
        env->episode_fault_right++;
    return (fault_side == env->side) ? env->fault_reward : 0.0f;
}

static void apply_agent_rewards(RcssEmptyNet* env, Reward r, int terminal) {
    for (int a = 0; a < env->num_agents; a++) {
        env->reward_struct[a] = r;
        env->returns[a].goal += r.goal;
        env->returns[a].fault += r.fault;
        env->returns[a].stagnate += r.stagnate;
        env->returns[a].step += r.step;
        env->agents[a].rewards[0] = r.goal + r.fault + r.stagnate + r.step;
        env->agents[a].terminals[0] = terminal ? 1.0f : 0.0f;
    }
}

static float player_return(const Reward* ret) {
    return ret->goal + ret->fault + ret->stagnate + ret->step;
}

static int goal_diff(RcssEmptyNet* env) {
    int goals_for = (env->side == SIDE_LEFT)
        ? env->episode_left_goal : env->episode_right_goal;
    int goals_against = (env->side == SIDE_LEFT)
        ? env->episode_right_goal : env->episode_left_goal;
    return goals_for - goals_against;
}

static int episode_success(RcssEmptyNet* env) {
    return goal_diff(env) > 0;
}

#pragma region early_terminal

static int handle_early_terminal(RcssEmptyNet* env, MyPlayMode pm) {
    Reward r = {0};
    r.step = env->step_reward;
    int terminal = 0;

    if (is_goal(pm)) {
        r.goal = goal_event_reward(env, goal_side(pm));
        terminal = 1;
    } else if (is_fault(pm)) {
        r.fault = fault_event_reward(env, side_at_fault(pm));
        terminal = 1;
    } else if (env->tick >= env->starting_horizon_ticks) {
        r.stagnate = env->no_goal_reward;
        env->episode_timeout = 1;
        terminal = 1;
    }

    apply_agent_rewards(env, r, terminal);
    return 0;
}

static float score_early_terminal(RcssEmptyNet* env) {
    if (episode_success(env))
        return 1.0f;
    if (env->episode_timeout)
        return 0.0f;
    return -1.0f;
}

// Faster goal → higher. No goal → 0. Range (0, 1].
static float perf_early_terminal(RcssEmptyNet* env) {
    if (!episode_success(env))
        return 0.0f;
    int timeout = env->starting_horizon_ticks;
    if (timeout < 1) timeout = 1;
    float elapsed = (float)env->tick / (float)timeout;
    if (elapsed > 1.0f) elapsed = 1.0f;
    return 1.0f - elapsed;
}

#pragma endregion

#pragma region fixed_horizon

static int fixed_horizon_ticks(RcssEmptyNet* env) {
    int start = env->starting_horizon_ticks;
    if (start < 1) start = 1;
    if (start >= HALF_TICKS) return start;
    float difficulty = env->difficulty;
    if (difficulty < 0.0f) difficulty = 0.0f;
    if (difficulty > 1.0f) difficulty = 1.0f;
    return start + (int)(difficulty * (float)(HALF_TICKS - start) + 0.5f);
}

static int handle_fixed_horizon(RcssEmptyNet* env, MyPlayMode pm) {
    const int horizon = fixed_horizon_ticks(env);
    const int terminal = (env->tick >= horizon);
    Reward r = {0};
    int soft = 0;

    if (is_goal(pm)) {
        r.goal = goal_event_reward(env, goal_side(pm));
        soft = !terminal;
    } else if (is_fault(pm)) {
        r.fault = fault_event_reward(env, side_at_fault(pm));
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

    apply_agent_rewards(env, r, terminal);
    if (soft) {
        stadium_changePlayMode(env->stadium, MY_PM_PLAY_ON);
        park_filler(env);
        float x0, x1, y_half;
        spawn_box(env->difficulty, env->side, &x0, &x1, &y_half);
        float ball_x, ball_y;
        place_ball(env, &ball_x, &ball_y, x0, x1, y_half);
        for (int a = 0; a < env->num_agents; a++)
            place_player(env, a, ball_x, ball_y, x0, x1, y_half);
        ensure_play_on(env);
    }
    return 0;
}

static float score_fixed_horizon(RcssEmptyNet* env) {
    return (float)goal_diff(env);
}

// Own-goal rate: 1.0 ≈ one goal every starting_horizon_ticks of budget.
// Longer curriculum episodes need more goals for the same perf.
static float perf_fixed_horizon(RcssEmptyNet* env) {
    int goals_for = (env->side == SIDE_LEFT)
        ? env->episode_left_goal : env->episode_right_goal;
    if (goals_for <= 0)
        return 0.0f;
    int horizon = fixed_horizon_ticks(env);
    if (horizon < 1) horizon = 1;
    float start = (float)env->starting_horizon_ticks;
    if (start < 1.0f) start = 1.0f;
    return (float)goals_for * start / (float)horizon;
}

#pragma endregion

void add_player_log(RcssEmptyNet* env, int player_index) {
    Reward* ret = &env->returns[player_index];
    Log* log = &env->log;

    log->episode_return += player_return(ret);
    log->episode_length += (float)env->players_tick[player_index];
    log->left_goal += (float)env->episode_left_goal;
    log->right_goal += (float)env->episode_right_goal;
    log->fault_left += (float)env->episode_fault_left;
    log->fault_right += (float)env->episode_fault_right;
    log->stagnate += (float)env->episode_stagnate;
    log->timeout += (float)env->episode_timeout;
    log->perf += env->early_terminal
        ? perf_early_terminal(env)
        : perf_fixed_horizon(env);
    log->score += env->early_terminal
        ? score_early_terminal(env)
        : score_fixed_horizon(env);
    log->difficulty += env->difficulty;
    log->success_rate += env->success_rate;
    log->n += 1.0f;
}

static void update_difficulty(RcssEmptyNet* env) {
    float rate = env->success_update_rate;
    env->success_rate = (1.0f - rate) * env->success_rate
        + rate * (float)episode_success(env);
    if (env->difficulty_rate > 0.0f
            && env->success_rate > env->target_success_rate) {
        env->difficulty += env->difficulty_rate
            * (env->success_rate - env->target_success_rate);
        if (env->difficulty > 1.0f) env->difficulty = 1.0f;
    }
}

void add_log(RcssEmptyNet* env) {
    for (int i = 0; i < env->num_agents; i++)
        add_player_log(env, i);
}

void handle_rewards_and_terminals(RcssEmptyNet* env) {
    MyPlayMode pm = stadium_playmode(env->stadium);
    if (env->early_terminal)
        handle_early_terminal(env, pm);
    else
        handle_fixed_horizon(env, pm);

    if (env->agents[0].terminals[0] == 1.0f) {
        add_log(env);
        update_difficulty(env);
        stadium_recoveryPlayers(env->stadium);
        stadium_changePlayMode(env->stadium, MY_PM_PLAY_ON);
        park_filler(env);
        float x0, x1, y_half;
        spawn_box(env->difficulty, env->side, &x0, &x1, &y_half);
        float ball_x, ball_y;
        place_ball(env, &ball_x, &ball_y, x0, x1, y_half);
        for (int a = 0; a < env->num_agents; a++)
            place_player(env, a, ball_x, ball_y, x0, x1, y_half);
        ensure_play_on(env);
        for (int i = 0; i < env->num_agents; i++)
            memset(&env->returns[i], 0, sizeof(Reward));
        clear_observations(env);
        memset(env->players_tick, 0, env->num_agents * sizeof(int));
        memset(env->reward_struct, 0, env->num_agents * sizeof(Reward));
        env->tick = 0;
        env->tick_since_last_goal = 0;
        env->episode_left_goal = 0;
        env->episode_right_goal = 0;
        env->episode_fault_left = 0;
        env->episode_fault_right = 0;
        env->episode_stagnate = 0;
        env->episode_timeout = 0;
    }
}

static float norm_reward(float value, float scale) {
    float abs_scale = fabsf(scale);
    return abs_scale > 0.0f ? value / abs_scale : 0.0f;
}

static void pack_reward_obs(obs_t* obs, int idx, const Reward* r, const RcssEmptyNet* env) {
    obs[idx++] = norm_reward(r->goal, env->goal_reward);
    obs[idx++] = norm_reward(r->fault, env->fault_reward);
    obs[idx++] = norm_reward(r->stagnate, env->no_goal_reward);
    obs[idx++] = norm_reward(r->step, env->step_reward);
}

static void compute_observations(RcssEmptyNet* env) {
    float s = env->side == SIDE_LEFT ? 1.0f : -1.0f;
    int order[NUM_PLAYERS];
    for (int i = 0; i < env->num_agents; i++)
        order[i] = i;
    sort_players_by_position(env->players, order, env->num_agents, s);

    for (int a = 0; a < env->num_agents; a++) {
        obs_t* obs = env->agents[a].observations;
        int idx = pack_observations_common(
            obs, a, env->num_agents, env->players, env->stadium, env->side, order);
        pack_reward_obs(obs, idx, &env->reward_struct[a], env);
    }
}

void puf_reset(RcssEmptyNet* env) {
    if (!env->boostrap_done) {
        bootstrap(env);
        env->boostrap_done = true;
    }

    if (!stadium_isAlive(env->stadium)) {
        printf("server is dead!\n");
        puf_close(env);
        exit(1);
    }

    clear_observations(env);
    memset(env->players_tick, 0, env->num_agents * sizeof(int));
    memset(env->reward_struct, 0, env->num_agents * sizeof(Reward));
    env->tick = 0;
    env->tick_since_last_goal = 0;
    env->episode_left_goal = 0;
    env->episode_right_goal = 0;
    env->episode_fault_left = 0;
    env->episode_fault_right = 0;
    env->episode_stagnate = 0;
    env->episode_timeout = 0;

    stadium_recoveryPlayers(env->stadium);

    stadium_changePlayMode(env->stadium, MY_PM_PLAY_ON);
    park_filler(env);
    float x0, x1, y_half;
    spawn_box(env->difficulty, env->side, &x0, &x1, &y_half);
    float ball_x, ball_y;
    place_ball(env, &ball_x, &ball_y, x0, x1, y_half);
    for (int a = 0; a < env->num_agents; a++)
        place_player(env, a, ball_x, ball_y, x0, x1, y_half);
    ensure_play_on(env);
    compute_observations(env);
}

void puf_step(RcssEmptyNet* env) {
    if (!stadium_isAlive(env->stadium)) {
        printf("server is dead!\n");
        puf_close(env);
        exit(1);
    }

    for (int player_index = 0; player_index < env->num_agents; player_index++) {
        env->players_tick[player_index] += 1;
        memset(&env->reward_struct[player_index], 0, sizeof(Reward));
    }

    env->tick += 1;
    env->tick_since_last_goal += 1;

    for (int a = 0; a < env->num_agents; a++)
        act_one(env->players[a], env->agents[a].actions);

    stadium_doStep(env->stadium);

    handle_rewards_and_terminals(env);

    compute_observations(env);
}

void puf_render(RcssEmptyNet* env) {
    (void)env;
    if (IsKeyDown(KEY_ESCAPE)) {
        exit(0);
    }
}

void puf_close(RcssEmptyNet* env) {
    if (IsWindowReady()) {
        CloseWindow();
    }
    if (env->stadium) {
        stadium_doQuit(env->stadium);
        stadium_destroy(env->stadium);
        env->stadium = NULL;
    }

    static bool server_param_cleared = false;
    if (!server_param_cleared) {
        serverparam_clear();
        server_param_cleared = true;
    }

    free(env->players);
    env->players = NULL;
    env->filler_left = NULL;

    if (env->log_file) {
        fclose(env->log_file);
        env->log_file = NULL;
    }
    free(env->returns);
    free(env->reward_struct);
    free(env->players_tick);
    env->returns = NULL;
    env->reward_struct = NULL;
    env->players_tick = NULL;
}

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

void puf_init(Env* env, Dict* kwargs) {
    env->num_agents = NUM_PLAYERS;
    env->side = (int)dict_get(kwargs, "side");
    env->use_monitor = (int)dict_get(kwargs, "use_monitor");
    env->starting_horizon_ticks = (int)dict_get(kwargs, "starting_horizon_ticks");
    env->synch_mode = dict_get(kwargs, "synch_mode") != 0.0;
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
    for (int i = 0; i < env->num_agents; i++) {
        env->agents[i].policy = 0;
        env->agents[i].action_mask = NULL;
    }
    init(env);
}
