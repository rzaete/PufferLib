#ifndef RCSSCOMMON_H
#define RCSSCOMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "stadium.h"
#include "gameplay.h"
#include "puf_fn.h"

#define OBS_SELF_INDEX_STRIDE 1
#define OBS_TIME_STRIDE 1
#define OBS_BALL_STRIDE 4
#define OBS_SELF_STAMINA 3
#define OBS_PLAYER_STRIDE 6
#define OBS_SELF_GLOBAL_POS 2
#define NUM_ATNS 4
#define ACT_SIZES {6, 21, 37, 37}

#define PITCH_LENGTH_TOTAL (PITCH_LENGTH + (2 * PITCH_MARGIN))
#define PITCH_WIDTH_TOTAL (PITCH_WIDTH + (2 * PITCH_MARGIN))
#define X_SCALE ((float)PITCH_LENGTH_TOTAL / 2.0f)
#define Y_SCALE ((float)PITCH_WIDTH_TOTAL / 2.0f)
#define PITCH_DIAGONAL_TOTAL 138.96f
#define GOAL_RIGHT_X ((float)PITCH_LENGTH / 2.0f)
#define GOAL_LEFT_X (-(float)PITCH_LENGTH / 2.0f)
#define GOAL_Y 0.0f
#define RAD2DEG_COEF (180.0f/M_PIf)

#define SIDE_LEFT  1
#define SIDE_RIGHT -1
#define SIDE_NONE 0


PUF_FN static inline float normalize_angle_deg(float deg) {
    while (deg > 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

PUF_FN static inline float Rad2Deg(float a) {
    return a * RAD2DEG_COEF;
}

PUF_FN static inline int is_play_on(PlayMode pm) {
    return pm == PM_PlayOn;
}

PUF_FN static inline int is_goal(PlayMode pm) {
    return pm == PM_AfterGoal_Left || pm == PM_AfterGoal_Right;
}

// Side that scored, or SIDE_NONE if pm is not a goal.
PUF_FN static inline int goal_side(PlayMode pm) {
    if (pm == PM_AfterGoal_Left) return SIDE_LEFT;
    if (pm == PM_AfterGoal_Right) return SIDE_RIGHT;
    return SIDE_NONE;
}

// Side at fault for a stoppage (OOB, foul, offside, …). SIDE_NONE if none.
PUF_FN static inline int side_at_fault(PlayMode pm) {
    switch (pm) {
    case PM_KickIn_Left:
    case PM_CornerKick_Left:
    case PM_GoalKick_Left:
    case PM_FreeKick_Left:
    case PM_IndFreeKick_Left:
        return SIDE_RIGHT;
    case PM_KickIn_Right:
    case PM_CornerKick_Right:
    case PM_GoalKick_Right:
    case PM_FreeKick_Right:
    case PM_IndFreeKick_Right:
        return SIDE_LEFT;
    case PM_OffSide_Left:
    case PM_Foul_Charge_Left:
    case PM_Foul_Push_Left:
    case PM_Foul_MultipleAttacker_Left:
    case PM_Foul_BallOut_Left:
    case PM_Back_Pass_Left:
    case PM_Free_Kick_Fault_Left:
    case PM_CatchFault_Left:
    case PM_Illegal_Defense_Left:
        return SIDE_LEFT;
    case PM_OffSide_Right:
    case PM_Foul_Charge_Right:
    case PM_Foul_Push_Right:
    case PM_Foul_MultipleAttacker_Right:
    case PM_Foul_BallOut_Right:
    case PM_Back_Pass_Right:
    case PM_Free_Kick_Fault_Right:
    case PM_CatchFault_Right:
    case PM_Illegal_Defense_Right:
        return SIDE_RIGHT;
    default:
        return SIDE_NONE;
    }
}

PUF_FN static inline int is_fault(PlayMode pm) {
    return side_at_fault(pm) != SIDE_NONE;
}

PUF_FN void pack_relative_ball_obs(Stadium *stadium, int player_index, float s, float *obs, int *idx) {
    obs[(*idx)++] = s * (stadium->ball_pos_x - stadium->players.pos_x[player_index]) / PITCH_LENGTH_TOTAL;
    obs[(*idx)++] = s * (stadium->ball_pos_y - stadium->players.pos_y[player_index]) / PITCH_WIDTH_TOTAL;
    obs[(*idx)++] = s * stadium->ball_vel_x / BALL_SPEED_MAX;
    obs[(*idx)++] = s * stadium->ball_vel_y / BALL_SPEED_MAX;
}

PUF_FN void pack_relative_player_obs(Stadium *stadium, int player_index, int other_index, float s, float *obs, int *idx) {
    float other_body = Rad2Deg(normalize_angle(stadium->players.angle_body_committed[other_index]));
    if (s < 0.0f) other_body = normalize_angle_deg(other_body + 180.0f);

    obs[(*idx)++] = s * (stadium->players.pos_x[other_index] - stadium->players.pos_x[player_index]) / PITCH_LENGTH_TOTAL;
    obs[(*idx)++] = s * (stadium->players.pos_y[other_index] - stadium->players.pos_y[player_index]) / PITCH_WIDTH_TOTAL;
    obs[(*idx)++] = s * stadium->players.vel_x[other_index] / PLAYER_SPEED_MAX;
    obs[(*idx)++] = s * stadium->players.vel_y[other_index] / PLAYER_SPEED_MAX;
    obs[(*idx)++] = (other_body + 180.0f) / 360.0f;
    obs[(*idx)++] = stadium->players.side[other_index] * s;
}

PUF_FN void pack_self_pos_obs(Stadium *stadium, int player_index, float s, float *obs, int *idx) {
    obs[(*idx)++] = s * stadium->players.pos_x[player_index] / X_SCALE;
    obs[(*idx)++] = s * stadium->players.pos_y[player_index] / Y_SCALE;
}

PUF_FN void pack_player_obs(Stadium *stadium, int player_index, float s, float *obs, int *idx) {
    float body = Rad2Deg(normalize_angle(stadium->players.angle_body_committed[player_index]));
    if (s < 0.0f) body = normalize_angle_deg(body + 180.0f);
    obs[(*idx)++] = s * stadium->players.pos_x[player_index] / X_SCALE;
    obs[(*idx)++] = s * stadium->players.pos_y[player_index] / Y_SCALE;
    obs[(*idx)++] = s * stadium->players.vel_x[player_index] / PLAYER_SPEED_MAX;
    obs[(*idx)++] = s * stadium->players.vel_y[player_index] / PLAYER_SPEED_MAX;
    obs[(*idx)++] = (body + 180.0f) / 360.0f;
    obs[(*idx)++] = s * stadium->players.side[player_index];
}

PUF_FN void pack_self_stamina_obs(Stadium *stadium, int player_index, float *obs, int *idx) {
    obs[(*idx)++] = stadium->players.stamina[player_index] / STAMINA_MAX;
    obs[(*idx)++] = stadium->players.effort[player_index]; // already ~[effort_min, 1]
    obs[(*idx)++] = stadium->players.stamina_capacity[player_index] / STAMINA_CAPACITY;
}

PUF_FN void pack_observations_common(Stadium *stadium, int agent, int num_agents, int side, float *obs, int *idx) {
    float s = side == SIDE_LEFT ? 1.0f : -1.0f;
    obs[(*idx)++] = agent;
    pack_relative_ball_obs(stadium, agent, s, obs, idx);
    for (int i = 0; i < num_agents; i++) {
        pack_player_obs(stadium, i, s, obs, idx);
    }
    pack_self_stamina_obs(stadium, agent, obs, idx);
}

PUF_FN void act_one(Stadium *stadium, int player_index, const float* player_actions) {
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
        dash(stadium, player_index, new_actions[1], new_actions[2]);
    } else if (action_type == 1) {
        turn(stadium, player_index, new_actions[2], &stadium->seed);
    } else if (action_type == 2) {
        kick(stadium, player_index, new_actions[1], new_actions[2]);
    } else if (action_type == 3) {
        tackle(stadium, player_index, new_actions[2], false);
    } else if (action_type == 4) {
        tackle(stadium, player_index, new_actions[2], true);
    } else if (action_type == 5) {
        // no body action
    } else {
#ifndef __CUDA_ARCH__
        printf("invalid action type: %d\n", action_type);
        fflush(stdout);
#endif
    }

    if (new_actions[3] != 0) {
        turn_neck(stadium, player_index, new_actions[3]);
    }
}

PUF_FN void act(Stadium *stadium, int num_agents, float *actions) {
    for (int a = 0; a < num_agents; a++)
        act_one(stadium, a, &actions[a * NUM_ATNS]);
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