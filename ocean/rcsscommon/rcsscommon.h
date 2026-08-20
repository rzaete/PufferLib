#ifndef RCSSCOMMON_H
#define RCSSCOMMON_H

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "myserver.h"
#include "helpers.h"

#define OBS_BALL_STRIDE 4
#define OBS_SELF_STAMINA 3
#define OBS_PLAYER_STRIDE 6
#define OBS_SELF_GLOBAL_POS 2
#define NUM_ATNS 4
#define ACT_SIZES {6, 21, 37, 37}

const int PITCH_LENGTH = 105;
const int PITCH_WIDTH = 68;
const int PITCH_MARGIN = 5;
const int PITCH_LENGTH_TOTAL = PITCH_LENGTH + (2 * PITCH_MARGIN);
const int PITCH_WIDTH_TOTAL = PITCH_WIDTH + (2 * PITCH_MARGIN);
const float X_SCALE = (float)PITCH_LENGTH_TOTAL / 2.0f;
const float Y_SCALE = (float)PITCH_WIDTH_TOTAL / 2.0f;
const float PITCH_DIAGONAL_TOTAL = 138.96f;
const float PLAYER_SPEED_MAX = 1.05f;
const float BALL_SPEED_MAX = 3.0f;
const float STAMINA_MAX = 8000.0f;
const float STAMINA_CAPACITY_MAX = 130600.0f;
const float PI_F = 3.14159265358979323846f;
const float GOAL_RIGHT_X = (float)PITCH_LENGTH / 2.0f;
const float GOAL_LEFT_X = -(float)PITCH_LENGTH / 2.0f;
const float GOAL_Y = 0.0f;

#define SIDE_LEFT  1
#define SIDE_RIGHT -1
#define SIDE_NONE 0


float normalize_angle(float deg) {
    while (deg > 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

// Writes the created stadium through out_stadium (callers pass &env->stadium).
// Taking MyStadium* by value would only rebind a local copy and leave the
// caller's pointer NULL — bootstrap then segfaults in stadium_doInitPlayer.
int launch_myserver(int seed, unsigned int player_port, unsigned int coach_port, unsigned int offline_coach_port, const char *log_path, const char *text_log_name, MyStadium** out_stadium) {
    (void)log_path;
    char player_port_arg[256];
    char coach_port_arg[256];
    char offline_coach_port_arg[256];
    char text_log_fixed_name_arg[256];
    sprintf(player_port_arg, "server::port=%u", player_port);
    sprintf(coach_port_arg, "server::olcoach_port=%u", coach_port);
    sprintf(offline_coach_port_arg, "server::coach_port=%u", offline_coach_port);
    sprintf(text_log_fixed_name_arg, "server::text_log_fixed_name=%s", text_log_name);

    const char *const args[] = {
        RCSSSERVER_BINARY_PATH,
        player_port_arg,
        coach_port_arg,
        offline_coach_port_arg,
        "server::synch_mode=on",
        "server::auto_mode=on",
        "server::game_logging=off",
        "server::text_logging=off",
        "server::half_time=-1",
        "server::coach_w_referee=on",
        "server::ball_stuck_area=0"
    };

    static bool server_param_initialized = false;
    if (!server_param_initialized) {
        serverparam_init(sizeof(args) / sizeof(args[0]), (char**)args);
        server_param_initialized = true;
    }

    MyStadium* stadium = stadium_create();
    stadium_init(stadium, seed);
    *out_stadium = stadium;
    return 0;
}

int launch_myserver_no_port(int seed, const char *log_path, const char *text_log_name, bool synch_mode, MyStadium** out_stadium) {
    (void)log_path;
    char synch_mode_arg[256];
    char text_log_fixed_name_arg[256];
    sprintf(synch_mode_arg, "server::synch_mode=%s", synch_mode ? "on" : "off");
    sprintf(text_log_fixed_name_arg, "server::text_log_fixed_name=%s", text_log_name);

    const char *const args[] = {
        RCSSSERVER_BINARY_PATH,
        synch_mode_arg,
        "server::auto_mode=on",
        "server::game_logging=off",
        "server::text_logging=off",
        "server::half_time=-1",
        "server::coach_w_referee=on",
        "server::disable_ports=on",
        "server::ball_stuck_area=0"
    };

    static bool server_param_initialized = false;
    if (!server_param_initialized) {
        serverparam_init(sizeof(args) / sizeof(args[0]), (char**)args);
        server_param_initialized = true;
    }

    MyStadium* stadium = stadium_create();
    stadium_init(stadium, seed);
    *out_stadium = stadium;
    return 0;
}

int init_rcss(unsigned int seed, int use_monitor, bool synch_mode, MyStadium** out_stadium) {
    int err = 0;
    char start_timestamp[20];
    char server_log_file_name[256];
    get_current_timestamp_str(start_timestamp, sizeof(start_timestamp));
    sprintf(server_log_file_name, "%s-%u-server-out.log", start_timestamp, seed);
    char server_text_log_fixed_name[128];
    sprintf(server_text_log_fixed_name, "%s-%u-server", start_timestamp, seed);
    char env_log_file_name[256];
    sprintf(env_log_file_name, "%s-%u-env.log", start_timestamp, seed);

    if (use_monitor) {
        unsigned int player_port;
        unsigned int coach_port;
        unsigned int offline_coach_port;
        err = get_ports_for_rcss(&player_port, &coach_port, &offline_coach_port, &seed);
        if (err < 0) {
            printf("RcssEmptyNet: failed to get ports for rcss\n");
            return err;
        }

        launch_myserver(seed % 10000, player_port, coach_port, offline_coach_port, "/dev/null", server_text_log_fixed_name, out_stadium);
        msleep(100);
        launch_monitor(player_port, "/dev/null");
        msleep(100);
        stadium_doStep(*out_stadium);
    }
    else {
        launch_myserver_no_port(seed % 10000, "/dev/null", server_text_log_fixed_name, synch_mode, out_stadium);
    }
    return err;
}

void log_message(FILE* log_file, const char *format, ...) {
    if (!log_file) return;

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
}

// Uniform float in [lo, hi) via rand_r.
static inline float randf(unsigned int* rng, float lo, float hi) {
    return lo + (hi - lo) * ((float)(rand_r(rng) % 10000) / 10000.0f);
}

static inline int is_play_on(MyPlayMode pm) {
    return pm == MY_PM_PLAY_ON;
}

static inline int is_goal(MyPlayMode pm) {
    return pm == MY_PM_AFTER_GOAL_LEFT || pm == MY_PM_AFTER_GOAL_RIGHT;
}

// Side that scored, or SIDE_NONE if pm is not a goal.
static inline int goal_side(MyPlayMode pm) {
    if (pm == MY_PM_AFTER_GOAL_LEFT) return SIDE_LEFT;
    if (pm == MY_PM_AFTER_GOAL_RIGHT) return SIDE_RIGHT;
    return SIDE_NONE;
}

// Side at fault for a stoppage (OOB, foul, offside, …). SIDE_NONE if none.
static inline int side_at_fault(MyPlayMode pm) {
    switch (pm) {
    case MY_PM_KICK_IN_LEFT:
    case MY_PM_CORNER_KICK_LEFT:
    case MY_PM_GOAL_KICK_LEFT:
    case MY_PM_FREE_KICK_LEFT:
    case MY_PM_IND_FREE_KICK_LEFT:
        return SIDE_RIGHT;
    case MY_PM_KICK_IN_RIGHT:
    case MY_PM_CORNER_KICK_RIGHT:
    case MY_PM_GOAL_KICK_RIGHT:
    case MY_PM_FREE_KICK_RIGHT:
    case MY_PM_IND_FREE_KICK_RIGHT:
        return SIDE_LEFT;
    case MY_PM_OFFSIDE_LEFT:
    case MY_PM_FOUL_CHARGE_LEFT:
    case MY_PM_FOUL_PUSH_LEFT:
    case MY_PM_FOUL_MULTIPLE_ATTACKER_LEFT:
    case MY_PM_FOUL_BALL_OUT_LEFT:
    case MY_PM_BACK_PASS_LEFT:
    case MY_PM_FREE_KICK_FAULT_LEFT:
    case MY_PM_CATCH_FAULT_LEFT:
    case MY_PM_ILLEGAL_DEFENSE_LEFT:
        return SIDE_LEFT;
    case MY_PM_OFFSIDE_RIGHT:
    case MY_PM_FOUL_CHARGE_RIGHT:
    case MY_PM_FOUL_PUSH_RIGHT:
    case MY_PM_FOUL_MULTIPLE_ATTACKER_RIGHT:
    case MY_PM_FOUL_BALL_OUT_RIGHT:
    case MY_PM_BACK_PASS_RIGHT:
    case MY_PM_FREE_KICK_FAULT_RIGHT:
    case MY_PM_CATCH_FAULT_RIGHT:
    case MY_PM_ILLEGAL_DEFENSE_RIGHT:
        return SIDE_RIGHT;
    default:
        return SIDE_NONE;
    }
}

static inline int is_fault(MyPlayMode pm) {
    return side_at_fault(pm) != SIDE_NONE;
}

void pack_relative_ball_obs(
    float* obs, int* idx, MyPlayer* player, MyStadium* stadium, float s)
{
    double player_x, player_y;
    double ball_x, ball_y, ball_vel_x, ball_vel_y;
    player_pos(player, &player_x, &player_y);
    ball_pos(stadium, &ball_x, &ball_y);
    ball_vel(stadium, &ball_vel_x, &ball_vel_y);

    obs[(*idx)++] = s * (float)(ball_x - player_x) / PITCH_LENGTH_TOTAL;
    obs[(*idx)++] = s * (float)(ball_y - player_y) / PITCH_WIDTH_TOTAL;
    obs[(*idx)++] = s * (float)ball_vel_x / BALL_SPEED_MAX;
    obs[(*idx)++] = s * (float)ball_vel_y / BALL_SPEED_MAX;
}

void pack_relative_player_obs(
    float* obs, int* idx, MyPlayer* player, MyPlayer* other, float s)
{
    double player_x, player_y;
    double other_x, other_y, other_vx, other_vy, other_body_deg;
    MySide other_side;
    player_pos(player, &player_x, &player_y);
    player_pos(other, &other_x, &other_y);

    player_vel(other, &other_vx, &other_vy);
    player_body_angle(other, &other_body_deg);
    player_side(other, &other_side);
    float other_body = (float)other_body_deg;
    if (s < 0.0f) other_body = normalize_angle(other_body + 180.0f);

    obs[(*idx)++] = s * (float)(other_x - player_x) / PITCH_LENGTH_TOTAL;
    obs[(*idx)++] = s * (float)(other_y - player_y) / PITCH_WIDTH_TOTAL;
    obs[(*idx)++] = s * (float)other_vx / PLAYER_SPEED_MAX;
    obs[(*idx)++] = s * (float)other_vy / PLAYER_SPEED_MAX;
    obs[(*idx)++] = (other_body + 180.0f) / 360.0f;
    obs[(*idx)++] = (float)other_side * s;
}

void pack_self_pos_obs(
    float* obs, int* idx, MyPlayer* p, float s)
{
    double x, y;
    player_pos(p, &x, &y);
    obs[(*idx)++] = s * (float)x / X_SCALE;
    obs[(*idx)++] = s * (float)y / Y_SCALE;
}

void pack_player_obs(
    float* obs, int* idx, MyPlayer* p, float s)
{
    double x, y, vx, vy, body_deg;
    MySide side;
    player_pos(p, &x, &y);
    player_vel(p, &vx, &vy);
    player_body_angle(p, &body_deg);
    player_side(p, &side);
    float body = (float)body_deg;
    if (s < 0.0f) body = normalize_angle(body + 180.0f);
    obs[(*idx)++] = s * (float)x / X_SCALE;
    obs[(*idx)++] = s * (float)y / Y_SCALE;
    obs[(*idx)++] = s * (float)vx / PLAYER_SPEED_MAX;
    obs[(*idx)++] = s * (float)vy / PLAYER_SPEED_MAX;
    obs[(*idx)++] = (body + 180.0f) / 360.0f;
    obs[(*idx)++] = s * (float)side;
}

void pack_self_stamina_obs(float* obs, int* idx, MyPlayer* p) {
    double stamina, effort, capacity;
    player_stamina(p, &stamina);
    player_effort(p, &effort);
    player_stamina_capacity(p, &capacity);
    obs[(*idx)++] = (float)stamina / STAMINA_MAX;
    obs[(*idx)++] = (float)effort; // already ~[effort_min, 1]
    obs[(*idx)++] = (float)capacity / STAMINA_CAPACITY_MAX;
}

#define RCSS_MAX_SORTED_PLAYERS 32

void sort_players_by_position(MyPlayer **players, int *order, int n, float s) {
    float sx[RCSS_MAX_SORTED_PLAYERS];
    float sy[RCSS_MAX_SORTED_PLAYERS];
    if (n > RCSS_MAX_SORTED_PLAYERS) {
        fprintf(stderr, "sort_players_by_position: n=%d > %d\n", n, RCSS_MAX_SORTED_PLAYERS);
        exit(1);
    }
    for (int i = 0; i < n; i++) {
        double x, y;
        player_pos(players[order[i]], &x, &y);
        sx[i] = s * (float)x;
        sy[i] = s * (float)y;
    }
    for (int i = 1; i < n; i++) {
        int key = order[i];
        float key_sx = sx[i];
        float key_sy = sy[i];
        int j = i - 1;
        while (j >= 0 &&
               (sx[j] > key_sx || (sx[j] == key_sx && sy[j] > key_sy))) {
            order[j + 1] = order[j];
            sx[j + 1] = sx[j];
            sy[j + 1] = sy[j];
            j--;
        }
        order[j + 1] = key;
        sx[j + 1] = key_sx;
        sy[j + 1] = key_sy;
    }
}

int pack_observations_common(float *obs, int agent, int num_agents,
                             MyPlayer **players, MyStadium *stadium, int side,
                             const int *order) {
    int obs_idx = 0;
    float s = side == SIDE_LEFT ? 1.0f : -1.0f;
    pack_relative_ball_obs(obs, &obs_idx, players[agent], stadium, s);
    pack_player_obs(obs, &obs_idx, players[agent], s);
    for (int i = 0; i < num_agents; i++) {
        if (order[i] == agent)
            continue;
        pack_player_obs(obs, &obs_idx, players[order[i]], s);
    }
    pack_self_stamina_obs(obs, &obs_idx, players[agent]);
    return obs_idx;
}

void act_one(MyPlayer* player, const float* player_actions) {
    float new_actions[NUM_ATNS] = {0};
    const int act_sizes[] = ACT_SIZES;

    int action_type = (int)player_actions[0];

    int bins1 = act_sizes[1];
    int b1 = (int)player_actions[1];
    if (b1 < 0) b1 = 0;
    if (b1 >= bins1) b1 = bins1 - 1;
    // Dash power [0, 100]; kick power [-100, 100]
    if (action_type == 0)
        new_actions[1] = (b1 / (float)(bins1 - 1)) * 100.0f;
    else if (action_type == 2)
        new_actions[1] = -100 + (b1 / (float)(bins1 - 1)) * 200.0f;

    int bins2 = act_sizes[2];
    int b2 = (int)player_actions[2];
    if (b2 < 0) b2 = 0;
    if (b2 >= bins2) b2 = bins2 - 1;
    new_actions[2] = -180.0f + (b2 / (float)(bins2 - 1)) * 360.0f;

    int bins3 = act_sizes[3];
    int b3 = (int)player_actions[3];
    if (b3 < 0) b3 = 0;
    if (b3 >= bins3) b3 = bins3 - 1;
    new_actions[3] = -180.0f + (b3 / (float)(bins3 - 1)) * 360.0f;

    if (action_type == 0) {
        player_dash(player, new_actions[1], new_actions[2]);
    } else if (action_type == 1) {
        player_turn(player, new_actions[2]);
    } else if (action_type == 2) {
        player_kick(player, new_actions[1], new_actions[2]);
    } else if (action_type == 3) {
        player_tackle(player, new_actions[2], false);
    } else if (action_type == 4) {
        player_tackle(player, new_actions[2], true);
    } else if (action_type == 5) {
        // no body action
    } else {
        printf("invalid action type: %d\n", action_type);
        fflush(stdout);
    }

    if (new_actions[3] != 0) {
        player_turn_neck(player, new_actions[3]);
    }
}

void act(MyPlayer **players, int num_agents, float *actions) {
    for (int a = 0; a < num_agents; a++)
        act_one(players[a], &actions[a * NUM_ATNS]);
}

// static void compute_observations_one(RcssEmptyNet* env, int a, float* obs) {
    // const float* sense_body_data = player_body_obs_data(me);
    // const float* seen_ball_data = player_seen_ball_data(me);
    // const float* seen_ball_valid = player_seen_ball_valid(me);
    // const float* seen_flags_data = player_seen_flags_data(me);
    // const float* seen_flags_valid = player_seen_flags_valid(me);
    // const float* seen_players_data = player_seen_players_data(me);
    // const float* seen_players_valid = player_seen_players_valid(me);

    // // Ball
    // obs[obs_idx++] = seen_ball_valid[MY_SEEN_BALL_DIST];
    // obs[obs_idx++] = seen_ball_data[MY_SEEN_BALL_DIST] / PITCH_DIAGONAL_TOTAL;
    // obs[obs_idx++] = seen_ball_valid[MY_SEEN_BALL_DIST_CHNG];
    // obs[obs_idx++] = seen_ball_data[MY_SEEN_BALL_DIST_CHNG] / BALL_SPEED_MAX;
    // obs[obs_idx++] = seen_ball_valid[MY_SEEN_BALL_DIR];
    // obs[obs_idx++] = (seen_ball_data[MY_SEEN_BALL_DIR] + 180.0f) / 360.0f;
    // obs[obs_idx++] = seen_ball_valid[MY_SEEN_BALL_DIR_CHNG];
    // obs[obs_idx++] = (seen_ball_data[MY_SEEN_BALL_DIR_CHNG] + 180.0f) / 360.0f;

    // // Sense body
    // obs[obs_idx++] = sense_body_data[MY_BODY_SPEED_MAG] / PLAYER_SPEED_MAX;
    // obs[obs_idx++] = (sense_body_data[MY_BODY_SPEED_DIR] + 180.0f) / 360.0f;
    // obs[obs_idx++] = (sense_body_data[MY_BODY_NECK_ANGLE] + 180.0f) / 360.0f;

    // // Flags (19 landmarks × dist/dir with validity)
    // for (int i = 0; i < 19; i++) {
    //     obs[obs_idx++] = seen_flags_valid[(i * MY_SEEN_FLAG_STRIDE) + MY_SEEN_FLAG_DIST];
    //     obs[obs_idx++] = seen_flags_data[(i * MY_SEEN_FLAG_STRIDE) + MY_SEEN_FLAG_DIST] / PITCH_DIAGONAL_TOTAL;
    //     obs[obs_idx++] = seen_flags_valid[(i * MY_SEEN_FLAG_STRIDE) + MY_SEEN_FLAG_DIR];
    //     obs[obs_idx++] = (seen_flags_data[(i * MY_SEEN_FLAG_STRIDE) + MY_SEEN_FLAG_DIR] + 180.0f) / 360.0f;
    // }

    // // Seen other players (visual FOV SoA — not privileged positions).
    // // Slot layout matches stadium fill order; unused slots stay zero from memset.
    // int nseen = player_seen_player_count(me);
    // if (nseen > MAX_OTHER) nseen = MAX_OTHER;
    // for (int s = 0; s < nseen; s++) {
    //     int src = s * MY_SEEN_PLAYER_STRIDE;
    //     int base = OBS_EGO_SIZE + s * OBS_OTHER_STRIDE;
    //     obs[base + 0] = seen_players_valid[src + MY_SEEN_PLAYER_DIST];
    //     obs[base + 1] = seen_players_data[src + MY_SEEN_PLAYER_DIST] / PITCH_DIAGONAL_TOTAL;
    //     obs[base + 2] = seen_players_valid[src * MY_SEEN_PLAYER_DIR];
    //     obs[base + 3] = (seen_players_data[src + MY_SEEN_PLAYER_DIR] + 180.0f) / 360.0f;
    //     obs[base + 4] = seen_players_valid[src * MY_SEEN_PLAYER_BODY];
    //     obs[base + 5] = (seen_players_data[src + MY_SEEN_PLAYER_BODY] + 180.0f) / 360.0f;
    //     obs[base + 6] = seen_players_valid[src * MY_SEEN_PLAYER_FACE];
    //     obs[base + 7] = (seen_players_data[src + MY_SEEN_PLAYER_FACE] + 180.0f) / 360.0f;
    //     obs[base + 8] = seen_players_valid[src * MY_SEEN_PLAYER_KICKING];
    //     obs[base + 9] = seen_players_data[src + MY_SEEN_PLAYER_KICKING];

    //     // // side is ±1 (LEFT/RIGHT) when team is recognized; else 0
    //     // if (seen_players_valid[src + MY_SEEN_PLAYER_SIDE] > 0.5f)
    //     //     obs[base + 3] = seen_players_data[src + MY_SEEN_PLAYER_SIDE];
    //     // else
    //     //     obs[base + 3] = 0.0f;
    // }

    // // Side + target-goal cue (attacked goal; privileged so both sides share one policy)
    // int me_left = agent_is_left(env, a);
    // float gx = agent_goal_x(env, a);
    // float dx = gx - (float)player_x;
    // float dy = GOAL_Y - (float)player_y;
    // float dist = sqrtf(dx * dx + dy * dy);
    // float dir_deg = atan2f(dy, dx) * (180.0f / PI_F);
    // int cue = OBS_EGO_SIZE + OBS_OTHER_SIZE;
    // obs[cue + 0] = me_left ? 1.0f : -1.0f;
    // obs[cue + 1] = dist / PITCH_DIAGONAL_TOTAL;
    // obs[cue + 2] = (normalize_angle(dir_deg) + 180.0f) / 360.0f;

    // (void)obs_idx; // ego block ends at OBS_EGO_SIZE
// }

#endif // RCSSCOMMON_H