/* RcssEmptyNet: empty-net scoring curriculum for PufferLib Ocean.
 */

#define TIME_REFEREE_ENABLED 0
#define BALL_STUCK_REFEREE_ENABLED 0
#define OFFSIDE_REFEREE_ENABLED 0
#define FREE_KICK_REFEREE_ENABLED 0
#define TOUCH_REFEREE_ENABLED 1
#define CATCH_REFEREE_ENABLED 1
#define FOUL_REFEREE_ENABLED 1
#define PENALTY_REFEREE_ENABLED 0

#define NUM_PLAYERS 2

#define NUM_IMAGINARY_OPPONENTS 2
#define HALF_TICKS 3000
#define PUF_STEPS_PER_SEC 30

typedef float obs_t;

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pufferenv.h"
#include "rcsscommonv2.h"
#include "render.h"
#include "table.h"

typedef struct {
    float goal;
    float fault;
    float stagnate;
    float step;
} Reward;

// #define REWARD_STRIDE ((int)(sizeof(Reward) / sizeof(float)))
#define OBS_TASK_STRIDE 1
#define OBS_TIME_STRIDE 1
#define OBS_SIZE (OBS_COMMON_STRIDE + OBS_TASK_STRIDE + OBS_TIME_STRIDE) /* + NUM_IMAGINARY_OPPONENTS * OBS_PLAYER_STRIDE */

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

typedef struct {
    float players_return[NUM_PLAYERS];
    int players_tick[NUM_PLAYERS];
    int tick;
    int episode_fault_left;
    int episode_fault_right;
    int episode_stagnate;
    int episode_timeout;
} EpisodeStats;

typedef struct {
    Side primary_side;
    int starting_horizon_ticks;
    int max_ticks;
    int early_terminal;
    float goal_reward;
    float fault_reward;
    float no_goal_timeout;
    float no_goal_reward;
    float step_reward;
    float difficulty;
    float success_update_rate;
    float target_success_rate;
    float difficulty_rate;
} EpisodeSettings;

struct Env {
    Agent agents[NUM_PLAYERS];
    Stadium stadium;
    unsigned int rng;
    int num_agents;
    Log log;
    int tag;

    EpisodeStats episode_stats;
    EpisodeSettings episode_settings;

    int tick_since_last_goal;
    float success_rate;
};

typedef Env RcssEmptyNet;

static void configure_stadium_paths(RcssEmptyNet* env) {
    // stadium_set_sense_body_enabled(&env->stadium, true);
    // stadium_set_visual_enabled(&env->stadium, true);
    // stadium_set_ball_compute_path(&env->stadium, MY_BALL_PATH_HIGH);
    // stadium_set_player_compute_path(&env->stadium, MY_PLAYER_PATH_HIGH);
    // landmark_noisy_dist_table_init();
    // stadium_set_flag_compute_path(&env->stadium, MY_FLAG_PATH_V2);
    (void)env;
}

static void spawn_box(float difficulty, int side, float* x0, float* x1, float* y_half) {
    const float easy_near = 44.0f; // |x| of near edge of easy box
    const float easy_far = PITCH_LENGTH * 0.5f - 1.5f;  // |x| of far edge of easy box
    const float easy_y = 5.0f;
    const float hard_x0 = -PITCH_LENGTH * 0.5f + 1.0f; // ~-51.5
    const float hard_x1 = PITCH_LENGTH * 0.5f - 2.0f;  // ~50.5
    const float hard_y = PITCH_WIDTH * 0.5f - 1.0f;    // ~33
    float p = difficulty;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    if (side == LEFT) {
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
    *ball_x = drand(&env->stadium.seed, x0, x1);
    *ball_y = drand(&env->stadium.seed, -y_half, y_half);
    stadium_moveBall(&env->stadium, *ball_x, *ball_y, 0, 0);
}

static void place_player(RcssEmptyNet* env, int player_index, float ball_x, float ball_y, float x0, float x1, float y_half) {
    (void)x0; (void)x1; (void)y_half;
    const float xmin = -PITCH_LENGTH * 0.5f + 1.0f;
    const float xmax =  PITCH_LENGTH * 0.5f - 1.0f;
    const float ymax =  PITCH_WIDTH * 0.5f - 1.0f;
    float d = env->episode_settings.difficulty;
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;
    float r = 3.0f + d * (40.0f - 3.0f);
    float p_x = ball_x, p_y = ball_y;
    for (int attempt = 0; attempt < 24; attempt++) {
        float ang = drand(&env->stadium.seed, -M_PIf, M_PIf);
        p_x = ball_x + r * cosf(ang);
        p_y = ball_y + r * sinf(ang);
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

static void ensure_play_on(RcssEmptyNet* env) {
    stadium_step(&env->stadium);
    if (env->stadium.playmode != PM_PlayOn) {
        changePlayMode(&env->stadium, PM_PlayOn, NEUTRAL);
        stadium_step(&env->stadium);
    }
}

static void clear_observations(RcssEmptyNet* env) {
    for (int a = 0; a < env->num_agents; a++)
        memset(env->agents[a].observations, 0, (size_t)OBS_SIZE * sizeof(obs_t));
}

static void apply_agent_rewards(RcssEmptyNet* env, Reward *team_left_reward, Reward *team_right_reward, int terminal) {
    for (int a = 0; a < env->num_agents; a++) {
        Reward *r;
        if (env->stadium.players.side[a] == LEFT)
            r = team_left_reward;
        else
            r = team_right_reward;
        
        float total_reward = r->goal + r->fault + r->stagnate + r->step;
        env->episode_stats.players_return[a] += total_reward;
        env->agents[a].rewards[0] = total_reward;
        env->agents[a].terminals[0] = terminal;
    }
}

static float remaining_time_frac(const RcssEmptyNet* env) {
    float elapsed = (float)env->episode_stats.tick / (float)env->episode_settings.starting_horizon_ticks;
    if (elapsed > 1.0f) elapsed = 1.0f;
    return 1.0f - elapsed;
}

static int is_winner(RcssEmptyNet* env, Side side) {
    return side * (env->stadium.team_left_points - env->stadium.team_right_points) > 0;
}

#pragma region early_terminal

static int handle_early_terminal(RcssEmptyNet* env) {
    Reward team_left_reward = {0};
    Reward team_right_reward = {0};
    team_left_reward.step = env->episode_settings.step_reward;
    team_right_reward.step = env->episode_settings.step_reward;
    int terminal = 0;
    Side scored_side = env->stadium.playmode == PM_AfterGoal ? env->stadium.playmode_side : NEUTRAL;
    Side faulted_side = side_at_fault(&env->stadium);
    if (scored_side != NEUTRAL) {
        team_left_reward.goal = scored_side == LEFT ? env->episode_settings.goal_reward : -env->episode_settings.goal_reward;
        team_left_reward.goal *= remaining_time_frac(env);
        team_right_reward.goal = -team_left_reward.goal;
        terminal = 1;
    } else if (faulted_side == LEFT) {
        env->episode_stats.episode_fault_left++;
        team_left_reward.fault = env->episode_settings.fault_reward;
        terminal = 1;
    } else if (faulted_side == RIGHT) {
        env->episode_stats.episode_fault_right++;
        team_right_reward.fault = env->episode_settings.fault_reward;
        terminal = 1;
    } else if (env->episode_stats.tick >= env->episode_settings.starting_horizon_ticks) {
        team_left_reward.stagnate = env->episode_settings.no_goal_reward;
        team_right_reward.stagnate = env->episode_settings.no_goal_reward;
        env->episode_stats.episode_timeout = 1;
        terminal = 1;
    }

    apply_agent_rewards(env, &team_left_reward, &team_right_reward, terminal);
    return 0;
}

static float score_early_terminal(RcssEmptyNet* env, Side side) {
    return is_winner(env, side);
}

// Faster goal → higher. No goal → 0. Range (0, 1].
static float perf_early_terminal(RcssEmptyNet* env, Side side) {
    return score_early_terminal(env, side) * remaining_time_frac(env);
}

#pragma endregion

#pragma region fixed_horizon

static void update_max_ticks(RcssEmptyNet* env) {
    int base_ticks = env->episode_settings.starting_horizon_ticks;
    float difficulty = env->episode_settings.difficulty;
    env->episode_settings.max_ticks = base_ticks + (int)(difficulty * (float)(HALF_TICKS - base_ticks) + 0.5f);
}

static int handle_fixed_horizon(RcssEmptyNet* env) {
    const int terminal = (env->episode_stats.tick >= env->episode_settings.max_ticks);
    Reward team_left_reward = {0};
    Reward team_right_reward = {0};
    int soft = 0;
    Side scored_side = env->stadium.playmode == PM_AfterGoal ? env->stadium.playmode_side : NEUTRAL;
    Side faulted_side = side_at_fault(&env->stadium);
    if (scored_side != NEUTRAL) {
        team_left_reward.goal = scored_side == LEFT ? env->episode_settings.goal_reward : -env->episode_settings.goal_reward;
        team_left_reward.goal *= remaining_time_frac(env);
        team_right_reward.goal = -team_left_reward.goal;
        soft = !terminal;
    } else if (faulted_side == LEFT) {
        env->episode_stats.episode_fault_left++;
        team_left_reward.fault = env->episode_settings.fault_reward;
        soft = !terminal;
    } else if (faulted_side == RIGHT) {
        env->episode_stats.episode_fault_right++;
        team_right_reward.fault = env->episode_settings.fault_reward;
        soft = !terminal;
    } else if (!terminal
        && env->stadium.playmode == PM_PlayOn
        && env->episode_settings.no_goal_timeout > 0.0f
        && env->tick_since_last_goal >= (int)env->episode_settings.no_goal_timeout) {
        team_left_reward.stagnate = env->episode_settings.no_goal_reward;
        team_right_reward.stagnate = env->episode_settings.no_goal_reward;
        env->tick_since_last_goal = 0;
        env->episode_stats.episode_stagnate++;
        soft = 1;
    }

    apply_agent_rewards(env, &team_left_reward, &team_right_reward, terminal);
    if (soft) {
        changePlayMode(&env->stadium, PM_PlayOn, NEUTRAL);
        float x0, x1, y_half;
        spawn_box(env->episode_settings.difficulty, env->episode_settings.primary_side, 
            &x0, &x1, &y_half);
        float ball_x, ball_y;
        place_ball(env, &ball_x, &ball_y, x0, x1, y_half);
        for (int a = 0; a < env->num_agents; a++)
            place_player(env, a, ball_x, ball_y, x0, x1, y_half);
        ensure_play_on(env);
    }
    return 0;
}

static float score_fixed_horizon(RcssEmptyNet* env, Side side) {
    return side * (env->stadium.team_left_points - env->stadium.team_right_points);
}

// perf in fixed horizon doesn't mean anything. score should be the primary metric for fixed horizon.
// we set perf to 1 if the side won the episode, otherwise 0.
static float perf_fixed_horizon(RcssEmptyNet* env, Side side) {
    return is_winner(env, side);
}

#pragma endregion

void add_player_log(RcssEmptyNet* env, int player_index) {
    Log* log = &env->log;
    log->episode_return += env->episode_stats.players_return[player_index];
    log->episode_length += (float)env->episode_stats.players_tick[player_index];
    log->left_goal += (float)env->stadium.team_left_points;
    log->right_goal += (float)env->stadium.team_right_points;
    log->fault_left += (float)env->episode_stats.episode_fault_left;
    log->fault_right += (float)env->episode_stats.episode_fault_right;
    log->stagnate += (float)env->episode_stats.episode_stagnate;
    log->timeout += (float)env->episode_stats.episode_timeout;
    log->perf += env->episode_settings.early_terminal
        ? perf_early_terminal(env, env->episode_settings.primary_side)
        : perf_fixed_horizon(env, env->episode_settings.primary_side);
    log->score += env->episode_settings.early_terminal
        ? score_early_terminal(env, env->episode_settings.primary_side)
        : score_fixed_horizon(env, env->episode_settings.primary_side);
    log->difficulty += env->episode_settings.difficulty;
    log->success_rate += env->success_rate;
    log->n += 1.0f;
}

static void update_difficulty(RcssEmptyNet* env) {
    float rate = env->episode_settings.success_update_rate;
    env->success_rate = (1.0f - rate) * env->success_rate
        + rate * is_winner(env, env->episode_settings.primary_side);
    if (env->episode_settings.difficulty_rate > 0.0f) {
        env->episode_settings.difficulty += env->episode_settings.difficulty_rate
            * (env->success_rate - env->episode_settings.target_success_rate);
        if (env->episode_settings.difficulty < 0.0f) env->episode_settings.difficulty = 0.0f;
        if (env->episode_settings.difficulty > 1.0f) env->episode_settings.difficulty = 1.0f;
    }

    if (!env->episode_settings.early_terminal) {
        update_max_ticks(env);
    }
}

void add_log(RcssEmptyNet* env) {
    for (int i = 0; i < env->num_agents; i++)
        add_player_log(env, i);
}

void handle_rewards_and_terminals(RcssEmptyNet* env) {
    if (env->episode_settings.early_terminal)
        handle_early_terminal(env);
    else
        handle_fixed_horizon(env);

    if (env->agents[0].terminals[0] == 1.0f) {
        add_log(env);
        update_difficulty(env);
        puf_reset(env);
    }
}

static float norm_reward(float value, float scale) {
    float abs_scale = fabsf(scale);
    return abs_scale > 0.0f ? value / abs_scale : 0.0f;
}

static inline float sign(float x) {
    return (0.0f < x) - (x < 0.0f);
}

static void pack_time_obs(RcssEmptyNet* env, float *obs, int *idx) {
    obs[(*idx)++] = (float)env->episode_stats.tick / env->episode_settings.max_ticks;
}

static void pack_task_obs(RcssEmptyNet* env, int a, float *obs, int *idx) {
    // float ball_ang = angleFromBody(&env->stadium, a,
    //     env->stadium.ball_pos_x, env->stadium.ball_pos_y);
    // float gx = env->side == SIDE_LEFT ? GOAL_RIGHT_X : GOAL_LEFT_X;
    // float goal_ang = angleFromBody(&env->stadium, a, gx, GOAL_Y);
    // obs[(*idx)++] = sinf(ball_ang);
    // obs[(*idx)++] = cosf(ball_ang);
    // obs[(*idx)++] = sinf(goal_ang);
    // obs[(*idx)++] = cosf(goal_ang);
    obs[(*idx)++] = ballKickable(&env->stadium, a) ? 1.0f : 0.0f;
}

static void compute_observations(RcssEmptyNet* env) {
    for (int a = 0; a < env->num_agents; a++) {
        obs_t* obs = env->agents[a].observations;
        int obs_idx = 0;
        pack_time_obs(env, obs, &obs_idx);
        pack_task_obs(env, a, obs, &obs_idx);
        pack_observations_common(&env->stadium, a, obs, &obs_idx);
    }
}

void puf_reset(RcssEmptyNet* env) {
    clear_observations(env);
    memset(env->episode_stats.players_return, 0, sizeof(env->episode_stats.players_return));
    memset(env->episode_stats.players_tick, 0, sizeof(env->episode_stats.players_tick));
    env->episode_stats.tick = 0;
    env->tick_since_last_goal = 0;
    env->episode_stats.episode_fault_left = 0;
    env->episode_stats.episode_fault_right = 0;
    env->episode_stats.episode_stagnate = 0;
    env->episode_stats.episode_timeout = 0;

    stadium_reset(&env->stadium);
    changePlayMode(&env->stadium, PM_PlayOn, NEUTRAL);
    float x0, x1, y_half;
    spawn_box(env->episode_settings.difficulty, env->episode_settings.primary_side, &x0, &x1, &y_half);
    float ball_x, ball_y;
    place_ball(env, &ball_x, &ball_y, x0, x1, y_half);
    for (int a = 0; a < env->num_agents; a++)
        place_player(env, a, ball_x, ball_y, x0, x1, y_half);
    ensure_play_on(env);
    compute_observations(env);
}

void puf_step(RcssEmptyNet* env) {
    if (IsKeyDown(KEY_LEFT_SHIFT))
        return;

    for (int player_index = 0; player_index < env->num_agents; player_index++) {
        env->episode_stats.players_tick[player_index] += 1;
    }

    env->episode_stats.tick += 1;
    env->tick_since_last_goal += 1;

    for (int a = 0; a < env->num_agents; a++)
        act_one(&env->stadium, a, env->agents[a].actions);

    stadium_step(&env->stadium);

    handle_rewards_and_terminals(env);

    compute_observations(env);
}

#pragma region render stuff

#define RCSS_INSPECT_MIN_W 320
#define RCSS_INSPECT_MAX_W 640
#define RCSS_INSPECT_FONT 18
#define INSPECT_AGENT_ROWS 128
#define INSPECT_STAT_ROWS 8
#define INSPECT_PAD 6

typedef struct {
    TableRow agent_header;
    TableRow agent_rows[INSPECT_AGENT_ROWS];
    int n_agent;
    TableRow stat_header;
    TableRow stat_rows[INSPECT_STAT_ROWS];
    int n_stat;
} InspectTables;

static const char *inspect_side(Side s) {
    if (s == LEFT) return "LEFT";
    if (s == RIGHT) return "RIGHT";
    return "NEUTRAL";
}

static int inspect_line_h(void) {
    return RCSS_INSPECT_FONT + 2;
}

static void inspect_fill_header(TableRow *head, const Stadium *stadium, const char *label) {
    table_row_init(head, label, RCSS_SCORE_FG, NUM_PLAYERS);
    for (int a = 0; a < NUM_PLAYERS; a++) {
        snprintf(head->cells[a].text, sizeof(head->cells[a].text), "%d", a);
        head->cells[a].color = rcss_side_color(stadium->players.side[a]);
    }
}

static void inspect_fill_ptr_floats(TableRow *rows, int *n, int max, const char *name,
                                    int count, float **ptrs) {
    for (int i = 0; i < count; i++) {
        if (*n >= max) return;
        char label[40];
        if (count == 1) snprintf(label, sizeof(label), "%s", name);
        else snprintf(label, sizeof(label), "%s[%d]", name, i);
        table_row_init(&rows[*n], label, RCSS_SCORE_FG, NUM_PLAYERS);
        for (int a = 0; a < NUM_PLAYERS; a++) {
            if (ptrs[a])
                snprintf(rows[*n].cells[a].text, sizeof(rows[*n].cells[a].text), "%.4f", ptrs[a][i]);
            else
                snprintf(rows[*n].cells[a].text, sizeof(rows[*n].cells[a].text), "(null)");
        }
        *n += 1;
    }
}

static void inspect_fill(RcssEmptyNet *env, InspectTables *t) {
    float *obs[NUM_PLAYERS];
    float *act[NUM_PLAYERS];
    float *rew[NUM_PLAYERS];
    float *term[NUM_PLAYERS];
    int policy[NUM_PLAYERS];
    for (int a = 0; a < NUM_PLAYERS; a++) {
        obs[a] = env->agents[a].observations;
        act[a] = env->agents[a].actions;
        rew[a] = env->agents[a].rewards;
        term[a] = env->agents[a].terminals;
        policy[a] = env->agents[a].policy;
    }

    inspect_fill_header(&t->agent_header, &env->stadium, "agent");
    t->n_agent = 0;
    inspect_fill_ptr_floats(t->agent_rows, &t->n_agent, INSPECT_AGENT_ROWS, "obs", OBS_SIZE, obs);
    inspect_fill_ptr_floats(t->agent_rows, &t->n_agent, INSPECT_AGENT_ROWS, "actions", NUM_ATNS, act);
    inspect_fill_ptr_floats(t->agent_rows, &t->n_agent, INSPECT_AGENT_ROWS, "rewards", 1, rew);
    inspect_fill_ptr_floats(t->agent_rows, &t->n_agent, INSPECT_AGENT_ROWS, "terminals", 1, term);
    if (t->n_agent < INSPECT_AGENT_ROWS) {
        table_row_init(&t->agent_rows[t->n_agent], "action_mask", RCSS_SCORE_FG, NUM_PLAYERS);
        for (int a = 0; a < NUM_PLAYERS; a++)
            snprintf(t->agent_rows[t->n_agent].cells[a].text,
                     sizeof(t->agent_rows[t->n_agent].cells[a].text), "%s",
                     env->agents[a].action_mask ? "(set)" : "(null)");
        t->n_agent++;
    }
    if (t->n_agent < INSPECT_AGENT_ROWS) {
        table_row_ints(&t->agent_rows[t->n_agent], "policy", RCSS_SCORE_FG, policy, NUM_PLAYERS);
        t->n_agent++;
    }

    inspect_fill_header(&t->stat_header, &env->stadium, "agent");
    t->n_stat = 0;
    if (t->n_stat < INSPECT_STAT_ROWS) {
        table_row_floats(&t->stat_rows[t->n_stat], "players_return", RCSS_SCORE_FG, "%.4f",
                         env->episode_stats.players_return, NUM_PLAYERS);
        t->n_stat++;
    }
    if (t->n_stat < INSPECT_STAT_ROWS) {
        table_row_ints(&t->stat_rows[t->n_stat], "players_tick", RCSS_SCORE_FG,
                       env->episode_stats.players_tick, NUM_PLAYERS);
        t->n_stat++;
    }
}

static Table inspect_agents_table(const InspectTables *t) {
    Table table;
    table.font = rcss_font();
    table.font_size = RCSS_INSPECT_FONT;
    table.ncols = NUM_PLAYERS;
    table.header = &t->agent_header;
    table.rows = t->agent_rows;
    table.nrows = t->n_agent;
    return table;
}

static Table inspect_stats_table(const InspectTables *t) {
    Table table;
    table.font = rcss_font();
    table.font_size = RCSS_INSPECT_FONT;
    table.ncols = NUM_PLAYERS;
    table.header = &t->stat_header;
    table.rows = t->stat_rows;
    table.nrows = t->n_stat;
    return table;
}

static int inspect_bump_w(int w, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    int tw = rcss_text_width(buf, RCSS_INSPECT_FONT);
    return tw > w ? tw : w;
}

static int inspect_content_width(const InspectTables *t, const RcssEmptyNet *env) {
    Table agents = inspect_agents_table(t);
    Table stats = inspect_stats_table(t);
    int w = table_width(&agents);
    int sw = table_width(&stats);
    if (sw > w) w = sw;

    const Log *log = &env->log;
    w = inspect_bump_w(w, "episode_return: %.4f", log->episode_return);
    w = inspect_bump_w(w, "success_update_rate: %.4f", env->episode_settings.success_update_rate);
    w = inspect_bump_w(w, "starting_horizon_ticks: %d", env->episode_settings.starting_horizon_ticks);
    w = inspect_bump_w(w, "target_success_rate: %.4f", env->episode_settings.target_success_rate);
    w = inspect_bump_w(w, "primary_side: %s", inspect_side(env->episode_settings.primary_side));
    return w + INSPECT_PAD * 2;
}

static int inspect_width(const InspectTables *t, const RcssEmptyNet *env, int screen_w) {
    int content_w = inspect_content_width(t, env);
    int cap = RCSS_INSPECT_MAX_W;
    int half = screen_w / 2;
    if (cap > half) cap = half;
    int lo = RCSS_INSPECT_MIN_W;
    if (lo > cap) lo = cap;
    return rcss_clampi(content_w, lo, cap);
}

static int inspect_draw_title(int x, int y, const char *s, Color c) {
    rcss_draw_text(s, x, y, RCSS_INSPECT_FONT, c);
    return y + inspect_line_h();
}

static int inspect_draw_kv(int x, int y, const char *key, const char *value) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s: %s", key, value);
    rcss_draw_text(buf, x, y, RCSS_INSPECT_FONT, RCSS_SCORE_FG);
    return y + inspect_line_h();
}

static int inspect_draw_float(int x, int y, const char *key, const char *fmt, float v) {
    char val[64];
    snprintf(val, sizeof(val), fmt, v);
    return inspect_draw_kv(x, y, key, val);
}

static int inspect_draw_int(int x, int y, const char *key, int v) {
    char val[32];
    snprintf(val, sizeof(val), "%d", v);
    return inspect_draw_kv(x, y, key, val);
}

static void draw_inspect(Rectangle pane, RcssEmptyNet *env, const InspectTables *tables) {
    if (pane.width <= 1.0f || pane.height <= 1.0f) return;

    static int scroll = 0;
    static int last_content_h = 0;

    Table agents = inspect_agents_table(tables);
    Table stats = inspect_stats_table(tables);
    int pad = INSPECT_PAD;
    int line_h = inspect_line_h();
    int x = (int)pane.x + pad;

    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, pane);
    int view_h = (int)pane.height - pad * 2;
    int max_scroll = last_content_h - view_h;
    if (max_scroll < 0) max_scroll = 0;
    if (hover)
        scroll -= (int)GetMouseWheelMove() * line_h;
    scroll = rcss_clampi(scroll, 0, max_scroll);

    rcss_clip(pane);
    DrawRectangle((int)pane.x, (int)pane.y, (int)pane.width, (int)pane.height,
                  (Color){16, 16, 16, 255});
    DrawLine((int)pane.x, (int)pane.y, (int)pane.x, (int)pane.y + (int)pane.height,
             (Color){255, 255, 255, 40});

    int y_start = (int)pane.y + pad;
    int y = y_start - scroll;

    y = inspect_draw_title(x, y, "agents", RCSS_LEFT);
    table_draw(x, y, &agents);
    y += table_height(&agents);

    y += line_h;
    y = inspect_draw_title(x, y, "log", RCSS_LEFT);
    const Log *log = &env->log;
    y = inspect_draw_float(x, y, "perf", "%.4f", log->perf);
    y = inspect_draw_float(x, y, "score", "%.4f", log->score);
    y = inspect_draw_float(x, y, "episode_return", "%.4f", log->episode_return);
    y = inspect_draw_float(x, y, "episode_length", "%.4f", log->episode_length);
    y = inspect_draw_float(x, y, "difficulty", "%.4f", log->difficulty);
    y = inspect_draw_float(x, y, "success_rate", "%.4f", log->success_rate);
    y = inspect_draw_float(x, y, "left_goal", "%.4f", log->left_goal);
    y = inspect_draw_float(x, y, "right_goal", "%.4f", log->right_goal);
    y = inspect_draw_float(x, y, "fault_left", "%.4f", log->fault_left);
    y = inspect_draw_float(x, y, "fault_right", "%.4f", log->fault_right);
    y = inspect_draw_float(x, y, "stagnate", "%.4f", log->stagnate);
    y = inspect_draw_float(x, y, "timeout", "%.4f", log->timeout);
    y = inspect_draw_float(x, y, "n", "%.4f", log->n);

    y += line_h;
    y = inspect_draw_title(x, y, "episode_stats", RCSS_LEFT);
    table_draw(x, y, &stats);
    y += table_height(&stats);
    const EpisodeStats *st = &env->episode_stats;
    y = inspect_draw_int(x, y, "tick", st->tick);
    y = inspect_draw_int(x, y, "episode_fault_left", st->episode_fault_left);
    y = inspect_draw_int(x, y, "episode_fault_right", st->episode_fault_right);
    y = inspect_draw_int(x, y, "episode_stagnate", st->episode_stagnate);
    y = inspect_draw_int(x, y, "episode_timeout", st->episode_timeout);

    y += line_h;
    y = inspect_draw_title(x, y, "episode_settings", RCSS_LEFT);
    const EpisodeSettings *s = &env->episode_settings;
    y = inspect_draw_kv(x, y, "primary_side", inspect_side(s->primary_side));
    y = inspect_draw_int(x, y, "starting_horizon_ticks", s->starting_horizon_ticks);
    y = inspect_draw_int(x, y, "max_ticks", s->max_ticks);
    y = inspect_draw_int(x, y, "early_terminal", s->early_terminal);
    y = inspect_draw_float(x, y, "goal_reward", "%.4f", s->goal_reward);
    y = inspect_draw_float(x, y, "fault_reward", "%.4f", s->fault_reward);
    y = inspect_draw_float(x, y, "no_goal_timeout", "%.4f", s->no_goal_timeout);
    y = inspect_draw_float(x, y, "no_goal_reward", "%.4f", s->no_goal_reward);
    y = inspect_draw_float(x, y, "step_reward", "%.4f", s->step_reward);
    y = inspect_draw_float(x, y, "difficulty", "%.4f", s->difficulty);
    y = inspect_draw_float(x, y, "success_update_rate", "%.4f", s->success_update_rate);
    y = inspect_draw_float(x, y, "target_success_rate", "%.4f", s->target_success_rate);
    y = inspect_draw_float(x, y, "difficulty_rate", "%.4f", s->difficulty_rate);

    y += line_h;
    y = inspect_draw_title(x, y, "success_rate", RCSS_LEFT);
    y = inspect_draw_float(x, y, "success_rate", "%.4f", env->success_rate);

    last_content_h = (y + scroll) - y_start;
    EndScissorMode();
}

#pragma endregion

void puf_render(RcssEmptyNet* env) {
    if (!IsWindowReady()) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(960 + RCSS_INSPECT_MAX_W, 680, "PufferLib RCSS");
        SetTargetFPS(60);
    }

    static InspectTables tables;
    inspect_fill(env, &tables);

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int inspect_w = inspect_width(&tables, env, screen_w);

    Rectangle field = { 0, 0, (float)(screen_w - inspect_w), (float)screen_h };
    Rectangle inspect = { (float)(screen_w - inspect_w), 0, (float)inspect_w, (float)screen_h };

    BeginDrawing();
    ClearBackground(BLACK);
    draw_rcss(&env->stadium, field);
    draw_inspect(inspect, env, &tables);
    EndDrawing();
    puf_web_vsync();
}

void puf_close(RcssEmptyNet* env) {
    if (IsWindowReady()) {
        CloseWindow();
    }
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
    assert(NUM_PLAYERS >= 1 && "NUM_PLAYERS cannot be less than 1");
    env->num_agents = NUM_PLAYERS;
    env->episode_settings.primary_side = (Side)dict_get(kwargs, "side");
    env->episode_settings.starting_horizon_ticks = (int)dict_get(kwargs, "starting_horizon_ticks");
    env->episode_settings.goal_reward = (float)dict_get(kwargs, "goal_reward");
    env->episode_settings.fault_reward = (float)dict_get(kwargs, "fault_reward");
    env->episode_settings.no_goal_timeout = (float)dict_get(kwargs, "no_goal_timeout");
    env->episode_settings.no_goal_reward = (float)dict_get(kwargs, "no_goal_reward");
    env->episode_settings.early_terminal = (int)dict_get(kwargs, "early_terminal");
    env->episode_settings.step_reward = (float)dict_get(kwargs, "step_reward");
    env->episode_settings.difficulty = (float)dict_get(kwargs, "difficulty");
    env->episode_settings.success_update_rate = (float)dict_get(kwargs, "success_update_rate");
    env->episode_settings.target_success_rate = (float)dict_get(kwargs, "target_success_rate");
    env->episode_settings.difficulty_rate = (float)dict_get(kwargs, "difficulty_rate");
    for (int i = 0; i < env->num_agents; i++) {
        env->agents[i].policy = 0;
        env->agents[i].action_mask = NULL;
    }

    env->episode_settings.max_ticks = env->episode_settings.starting_horizon_ticks;
    env->success_rate = 0.0f;
    unsigned int left_players_count = env->episode_settings.primary_side == LEFT ? NUM_PLAYERS : 0;
    unsigned int right_players_count = NUM_PLAYERS - left_players_count;
    memset(env->episode_stats.players_return, 0, sizeof(env->episode_stats.players_return));
    memset(env->episode_stats.players_tick, 0, sizeof(env->episode_stats.players_tick));
    memset(&env->stadium, 0, sizeof(env->stadium));
    stadium_init(&env->stadium, env->rng, left_players_count, right_players_count);
    configure_stadium_paths(env);
}
