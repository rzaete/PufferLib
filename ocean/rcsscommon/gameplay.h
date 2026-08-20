#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <float.h>
#include <string.h>
#include "stadium.h"
#include "puf_fn.h"

#define TWO_PIF (2.0f * M_PIf)
#define DEG2RAD_COEF (M_PIf/180.0f)

PUF_FN void kickTaken(Stadium *stadium, int kicker_index, float accel_x, float accel_y);
PUF_FN void tackleTaken(Stadium *stadium, int tackler_index, float accel_x, float accel_y, const bool foul);
PUF_FN void failedTackleTaken(Stadium *stadium, int tackler_index, const bool foul);
PUF_FN void collisions(Stadium *stadium);
#if REFEREES_ENABLED
PUF_FN void ref_playModeChange(Stadium *stadium, PlayMode pm);
PUF_FN bool ref_isPenaltyShootOut(const PlayMode pm, const Side side);
PUF_FN void ref_kickTaken(Stadium *stadium, int kicker_index, const float accel_r);
PUF_FN void ref_failedKickTaken(Stadium *stadium, int kicker_index);
PUF_FN void ref_tackleTaken(Stadium *stadium, int tackler_index, const float accel_r, const bool foul);
PUF_FN void ref_failedTackleTaken(Stadium *stadium, int tackler_index, const bool foul);
#endif

#pragma region base_utilities

PUF_FN static inline float r(float x, float y) {
    return sqrtf(x * x + y * y);
}

PUF_FN static inline float r2(float x, float y) {
    return x * x + y * y;
}

PUF_FN static inline float th(float x, float y) {
    return (x == 0.0f) && (y == 0.0f) ? 0.0f : atan2f(y, x);
}

PUF_FN static inline float distance(float x1, float y1, float x2, float y2) {
    return r(x1 - x2, y1 - y2);
}

PUF_FN static inline float distance2(float x1, float y1, float x2, float y2) {
    return r2(x1 - x2, y1 - y2);
}

PUF_FN static inline void normalize(float *x, float *y, const float l) {
    float vec_r = r(*x, *y);
    float coef = (l / fmaxf(vec_r, EPS));
    *x *= coef;
    *y *= coef;
}

PUF_FN static inline float add_eps(float x) {
    return nextafterf(x, HUGE_VALF);
}

PUF_FN static inline float subtract_eps(float x) {
    return nextafterf(x, -HUGE_VALF);
}

PUF_FN static inline void rotate(float *x, float *y, const float ang) {
    float c = cosf(ang);
    float s = sinf(ang);
    float new_x = (*x) * c - (*y) * s;
    float new_y = (*x) * s + (*y) * c;
    *x = new_x;
    *y = new_y;
}

PUF_FN static inline unsigned int puf_rand(unsigned int *seed) {
#ifdef __CUDA_ARCH__
    unsigned int x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *seed = x ? x : 0xA341316Cu;
    return *seed;
#else
    return (unsigned int)rand_r(seed);
#endif
}

PUF_FN static inline float drand(unsigned int *seed, float low, float high) {
    if (low > high) {
        float temp = low;
        low = high;
        high = temp;
    }

    if (high - low < 1.0e-10f)
        return (low + high) * 0.5f;

#ifdef __CUDA_ARCH__
    return low + (puf_rand(seed) * (1.0f / 4294967296.0f)) * (high - low);
#else
    return low + ((float)puf_rand(seed) / (float)RAND_MAX) * (high - low);
#endif
}

PUF_FN static inline float brand(unsigned int *seed, float prob) {
#ifdef __CUDA_ARCH__
    return (puf_rand(seed) * (1.0f / 4294967296.0f)) < prob;
#else
    return (float)puf_rand(seed) / (RAND_MAX + 1.0) < prob;
#endif
}

PUF_FN static inline float normalize_angle(float ang) {
    if (fabsf(ang) > TWO_PIF)
        ang = fmodf(ang, TWO_PIF);
    if (ang < -M_PIf)
        ang += TWO_PIF;
    if (ang > M_PIf)
        ang -= TWO_PIF;
    return ang;
}

PUF_FN static inline float Deg2Rad(const float a) {
    return a * DEG2RAD_COEF;
}

PUF_FN static inline float clamp(float x, float low, float high) {
    if (x < low)
        return low;
    if (x > high)
        return high;
    return x;
}

PUF_FN static inline void from_polar(float r, float ang, float *out_x, float *out_y) {
    *out_x = r * cosf(ang);
    *out_y = r * sinf(ang);
}

PUF_FN static inline bool between(float x, float y, float begin_x, float begin_y, float end_x, float end_y) {
    if (begin_x > end_x) {
        return between(x, y, end_x, end_y, begin_x, begin_y);
    }

    if (begin_x <= x && x <= end_x) {
        if (begin_y < end_y)
            return begin_y <= y && y <= end_y;
        else
            return begin_y >= y && y >= end_y;
    }

    return false;
}

#pragma endregion

PUF_FN static inline float normalize_dash_power(const float p) {
    return clamp(p, MIN_DASH_POWER, MAX_DASH_POWER);
}

PUF_FN static inline float normalize_dash_angle(const float d) {
    float dir = clamp(d, MIN_DASH_ANGLE, MAX_DASH_ANGLE);
    if (DASH_ANGLE_STEP < EPS ) {
        // players can dash any direction.
    }
    else {
        // The dash direction is discretized by server::dash_angle_step
        dir = DASH_ANGLE_STEP * rint(dir / DASH_ANGLE_STEP);
    }
    return dir;
}

PUF_FN static inline float NormalizeMoment(const float p) {
    return Deg2Rad(clamp(p, MIN_MOMENT, MAX_MOMENT));
}

PUF_FN static inline float NormalizeNeckMoment(const float p) {
    return Deg2Rad(clamp(p, MIN_NECK_MOMENT, MAX_NECK_MOMENT));
}

PUF_FN static inline float NormalizeNeckAngle(const float p) {
    return clamp(p, MIN_NECK_ANGLE, MAX_NECK_ANGLE);
}

PUF_FN static inline float NormalizeKickPower(const float p) {
    return clamp(p, MIN_POWER, MAX_POWER);
}

PUF_FN void calcDashAccel(Stadium *stadium, const float consumed_stamina, float leg_dash_power, float leg_dash_dir, int player_index, float *out_x, float *out_y) {
    const float power = normalize_dash_power(leg_dash_power < 0.0f ? -consumed_stamina : consumed_stamina * 2.0f);
    const float unnormalized_dir_rate = fabsf(leg_dash_dir) > 90.0f 
        ? BACK_DASH_RATE - ((BACK_DASH_RATE - SIDE_DASH_RATE)) * (1.0f - (fabsf(leg_dash_dir) - 90.0f) / 90.0f) 
        : SIDE_DASH_RATE + ((1.0f - SIDE_DASH_RATE) * (1.0f - fabsf(leg_dash_dir) / 90.0f));
    const float dir_rate = clamp(unnormalized_dir_rate , 0.0f, 1.0f);
    float accel_magnitude = fabsf(stadium->players.effort[player_index] * power * dir_rate * stadium->players.player_type[player_index].dash_power_rate);
    if (stadium->players.pos_y[player_index] < 0.0f)
        accel_magnitude /= (stadium->players.side[player_index] == LEFT ? SLOWNESS_ON_TOP_FOR_LEFT : SLOWNESS_ON_TOP_FOR_RIGHT);

    from_polar(accel_magnitude, normalize_angle(stadium->players.angle_body_committed[player_index] + Deg2Rad(leg_dash_dir)), out_x, out_y);
    if (power < 0.0f) {
        *out_x = -(*out_x);
        *out_y = -(*out_y);
    }
}

PUF_FN void apply_legs_effects(Stadium *stadium) {
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i])
            continue;

        if (stadium->players.left_leg_command_type[i] != DASH && stadium->players.right_leg_command_type[i] != DASH)
            continue;

        stadium->players.dash_count[i] += 1;
        float left_power = stadium->players.left_leg_dash_power[i];
        float right_power = stadium->players.right_leg_dash_power[i];
        float left_consumed_stamina = (left_power < 0.0f ? -left_power : left_power * 0.5);
        float right_consumed_stamina = (right_power < 0.0f ? -right_power : right_power * 0.5);
        float consumed_stamina = left_consumed_stamina + right_consumed_stamina;
        if (consumed_stamina < 1.0e-5f)
            continue;

        consumed_stamina = fminf(consumed_stamina, stadium->players.stamina[i] + stadium->players.player_type[i].extra_stamina);
        left_consumed_stamina = consumed_stamina * left_consumed_stamina / ( left_consumed_stamina + right_consumed_stamina );
        right_consumed_stamina = consumed_stamina * right_consumed_stamina / ( left_consumed_stamina + right_consumed_stamina );
            float left_accel_x, left_accel_y, right_accel_x, right_accel_y;
            calcDashAccel(stadium, left_consumed_stamina, stadium->players.left_leg_dash_power[i],
                stadium->players.left_leg_dash_dir[i], i, &left_accel_x, &left_accel_y);
            calcDashAccel(stadium, right_consumed_stamina, stadium->players.right_leg_dash_power[i],
                stadium->players.right_leg_dash_dir[i], i, &right_accel_x, &right_accel_y);
            float body_unit_x, body_unit_y;
            from_polar(1.0f, stadium->players.angle_body_committed[i], &body_unit_x, &body_unit_y);
            float vel_l_x = stadium->players.vel_x[i] + left_accel_x;
            float vel_l_y = stadium->players.vel_y[i] + left_accel_y;
            float vel_r_x = stadium->players.vel_x[i] + right_accel_x;
            float vel_r_y = stadium->players.vel_y[i] + right_accel_y;
            const float vel_l_body = body_unit_x * vel_l_x + body_unit_y * vel_l_y;
            const float vel_r_body = body_unit_x * vel_r_x + body_unit_y * vel_r_y;
            float new_vel_x = (vel_r_x + vel_l_x) / 2.0f;
            float new_vel_y = (vel_r_y + vel_l_y) / 2.0f;
            stadium->players.accel_x[i] += new_vel_x - stadium->players.vel_x[i];
            stadium->players.accel_y[i] += new_vel_y - stadium->players.vel_y[i];
            if (stadium->players.left_leg_command_type[i] == DASH && stadium->players.right_leg_command_type[i] == DASH) {
                float omega = (vel_l_body - vel_r_body) / ( stadium->players.player_type[i].player_size * 2.0f );
                stadium->players.angle_body[i] = normalize_angle(stadium->players.angle_body_committed[i] +
                    (1.0f + drand(&stadium->seed, -stadium->players.randp[i], stadium->players.randp[i])) * omega);
        }

        stadium->players.stamina[i] = fmaxf(0.0f, stadium->players.stamina[i] - consumed_stamina);
    }
}

PUF_FN void reset_command_flags(Stadium *stadium) {
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) 
            continue;

        if (stadium->players.kick_cycles[i] >= 0)
            stadium->players.kick_cycles[i] -= 1;

        if (stadium->players.dash_cycles[i] >= 0)
            stadium->players.dash_cycles[i] -= 1;

        if (stadium->players.tackle_cycles[i] > 0)
            stadium->players.tackle_cycles[i] -= 1;

        if (stadium->players.foul_cycles[i] > 0)
            stadium->players.foul_cycles[i] -= 1;

        if (stadium->players.kick_cycles[i] <= 0 && stadium->players.tackle_cycles[i] == 0 && stadium->players.foul_cycles[i] == 0)
            stadium->players.command_done[i] = false;

        stadium->players.turn_neck_done[i] = false;
        stadium->players.done_received[i] = false;
        stadium->players.left_leg_command_type[i] = NONE;
        stadium->players.right_leg_command_type[i] = NONE;
        stadium->players.left_leg_dash_power[i] = 0.0f;
        stadium->players.right_leg_dash_power[i] = 0.0f;
        stadium->players.left_leg_dash_dir[i] = 0.0f;
        stadium->players.right_leg_dash_dir[i] = 0.0f;
    }
}

PUF_FN void updateAngle_player(Stadium *stadium, int player_index) {
    stadium->players.angle_body_committed[player_index] = stadium->players.angle_body[player_index];
    stadium->players.angle_neck_committed[player_index] = stadium->players.angle_neck[player_index];
}

PUF_FN void collidedWithPost(Stadium *stadium, int player_index) {
    stadium->players.state[player_index] |= STATE_POST_COLLIDE;
    stadium->players.post_collide[player_index] = true;
}

PUF_FN void collidedWithBall(Stadium *stadium, int player_index) {
    stadium->players.state[player_index] |= (STATE_BALL_TO_PLAYER | STATE_BALL_COLLIDE);
    stadium->players.ball_collide[player_index] = true;
}

PUF_FN void collidedWithPlayer(Stadium *stadium, int player_index) {
    stadium->players.state[player_index] |= STATE_PLAYER_COLLIDE;
    stadium->players.player_collide[player_index] = true;
}

PUF_FN void clearCollision(float *post_collision_pos_x, float *post_collision_pos_y, int *collision_count) {
    *post_collision_pos_x = 0.0f;
    *post_collision_pos_y = 0.0f;
    *collision_count = 0;
}

PUF_FN void update_stamina_and_capacities_and_reset_states(Stadium *stadium) {
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) 
            continue;

        // myo: updateStamina
        if (stadium->players.stamina[i] <= RECOVER_DEC_THR * STAMINA_MAX) {
            if (stadium->players.recovery[i] > RECOVER_MIN)
                stadium->players.recovery[i] -= RECOVER_DEC;

            if (stadium->players.recovery[i] < RECOVER_MIN)
                stadium->players.recovery[i] = RECOVER_MIN;
        }

        if (stadium->players.stamina[i] <= EFFORT_DEC_THR * STAMINA_MAX) {
            if (stadium->players.effort[i] > stadium->players.player_type[i].effort_min)
                stadium->players.effort[i] -= EFFORT_DEC;

            if (stadium->players.effort[i] < stadium->players.player_type[i].effort_min)
                stadium->players.effort[i] = stadium->players.player_type[i].effort_min;
        }

        if (stadium->players.stamina[i] >= EFFORT_INC_THR * STAMINA_MAX) {
            if (stadium->players.effort[i] < stadium->players.player_type[i].effort_max) {
                stadium->players.effort[i] += EFFORT_INC;
                if (stadium->players.effort[i] > stadium->players.player_type[i].effort_max)
                    stadium->players.effort[i] = stadium->players.player_type[i].effort_max;
            }
        }

        float stamina_inc = fminf(stadium->players.recovery[i] * stadium->players.player_type[i].stamina_inc_max, STAMINA_MAX - stadium->players.stamina[i]);
        if (STAMINA_CAPACITY >= 0.0f) {
            if (stamina_inc > stadium->players.stamina_capacity[i])
                stamina_inc = stadium->players.stamina_capacity[i];
        }

        stadium->players.stamina[i] += stamina_inc;
        if (stadium->players.stamina[i] > STAMINA_MAX)
            stadium->players.stamina[i] = STAMINA_MAX;

        if (STAMINA_CAPACITY >= 0.0f) {
            stadium->players.stamina_capacity[i] -= stamina_inc;
            if (stadium->players.stamina_capacity[i] < 0.0f)
                stadium->players.stamina_capacity[i] = 0.0f;
        }

        // myo: updateCapacity
        stadium->players.hear_capacity_from_teammate[i] += HEAR_INC;
        if (stadium->players.hear_capacity_from_teammate[i] > (int)HEAR_MAX)
            stadium->players.hear_capacity_from_teammate[i] = HEAR_MAX;

        stadium->players.hear_capacity_from_opponent[i] += HEAR_INC;
        if (stadium->players.hear_capacity_from_opponent[i] > (int)HEAR_MAX)
            stadium->players.hear_capacity_from_opponent[i] = HEAR_MAX;

        if (stadium->players.goalie_catch_ban[i] > 0 )
            stadium->players.goalie_catch_ban[i] -= 1;

        // myo: resetState
        int state = (STATE_STAND | STATE_GOALIE | STATE_DISCARD | STATE_YELLOW_CARD | STATE_RED_CARD);
        if (stadium->players.kick_cycles[i] > 0)
            state |= (STATE_KICK | STATE_KICK_FAULT);

        if (stadium->players.tackle_cycles[i] > 0)
            state |= (STATE_TACKLE | STATE_TACKLE_FAULT);

        if (stadium->players.foul_cycles[i] > 0)
            state |= STATE_FOUL_CHARGED;

        stadium->players.state[i] &= state;
    }
}

#pragma region player_commands

PUF_FN void dashLeftLeg(Stadium *stadium, int player_index, double power, double dir) {
    if (stadium->players.left_leg_command_type[player_index] != NONE)
        return;

    stadium->players.left_leg_dash_power[player_index] = normalize_dash_power( power );
    stadium->players.left_leg_dash_dir[player_index] = normalize_dash_angle( dir );
    stadium->players.left_leg_command_type[player_index] = DASH;
    stadium->players.dash_cycles[player_index] = 1;
    stadium->players.command_done[player_index] = true;
}

PUF_FN void dashRightLeg(Stadium *stadium, int player_index, double power, double dir) {
    if (stadium->players.right_leg_command_type[player_index] != NONE)
        return;

    stadium->players.right_leg_dash_power[player_index] = normalize_dash_power( power );
    stadium->players.right_leg_dash_dir[player_index] = normalize_dash_angle( dir );
    stadium->players.right_leg_command_type[player_index] = DASH;
    stadium->players.dash_cycles[player_index] = 1;
    stadium->players.command_done[player_index] = true;
}

PUF_FN void dash(Stadium *stadium, int player_index, double power, double dir) {
    if (!stadium->players.command_done[player_index]) {
        dashLeftLeg(stadium, player_index, power, dir);
        dashRightLeg(stadium, player_index, power, dir);
    }
}

PUF_FN void turn(Stadium *stadium, int player_index, float moment, unsigned int *seed) {
    if (!stadium->players.command_done[player_index]) {
        if (stadium->players.left_leg_command_type[player_index] == NONE)
            stadium->players.left_leg_command_type[player_index] = TURN;
        
        if (stadium->players.right_leg_command_type[player_index] == NONE)
            stadium->players.right_leg_command_type[player_index] = TURN;

        stadium->players.angle_body[player_index] = normalize_angle(stadium->players.angle_body_committed[player_index]
            + (1.0f + drand(seed, -stadium->players.randp[player_index], stadium->players.randp[player_index]))
            * NormalizeMoment(moment)
            / (1.0f + stadium->players.player_type[player_index].inertia_moment * r(stadium->players.vel_x[player_index], stadium->players.vel_y[player_index])));
        stadium->players.turn_count[player_index] += 1;
        stadium->players.command_done[player_index] = true;
    }
}

PUF_FN void turn_neck(Stadium *stadium, int player_index, double moment) {
    if (!stadium->players.turn_neck_done[player_index]) {
        stadium->players.angle_neck[player_index] = NormalizeNeckAngle(stadium->players.angle_neck_committed[player_index] + NormalizeNeckMoment(moment));
        stadium->players.turn_neck_count[player_index] += 1;
        stadium->players.turn_neck_done[player_index] = true;
    }
}

PUF_FN float kickableArea(Stadium *stadium, int player_index) {
    return stadium->players.player_type[player_index].player_size + BALL_SIZE + stadium->players.player_type[player_index].kickable_margin;
}

PUF_FN bool ballKickable(Stadium *stadium, int player_index) {
    float area = kickableArea(stadium, player_index);
    return distance2(stadium->players.pos_x[player_index], stadium->players.pos_y[player_index], stadium->ball_pos_x, stadium->ball_pos_y)
        <= area * area;
}

PUF_FN float angleFromBody(Stadium *stadium, const int player_index, float obj_pos_x, float obj_pos_y) {
    return normalize_angle(th(obj_pos_x - stadium->players.pos_x[player_index], obj_pos_y - stadium->players.pos_y[player_index]) - stadium->players.angle_body_committed[player_index]);
}

PUF_FN void kick(Stadium *stadium, int player_index, double power, double dir) {
    if (stadium->players.command_done[player_index])
        return;

    stadium->players.command_done[player_index] = true;
    stadium->players.kick_cycles[player_index] = 1;
    power = NormalizeKickPower( power );
    dir = NormalizeMoment( dir );
    stadium->players.state[player_index] |= STATE_KICK;

    if (stadium->playmode == PM_BeforeKickOff ||
        stadium->playmode == PM_AfterGoal_Left ||
        stadium->playmode == PM_AfterGoal_Right ||
        stadium->playmode == PM_OffSide_Left ||
        stadium->playmode == PM_OffSide_Right ||
        stadium->playmode == PM_Illegal_Defense_Left ||
        stadium->playmode == PM_Illegal_Defense_Right ||
        stadium->playmode == PM_Foul_Charge_Left ||
        stadium->playmode == PM_Foul_Charge_Right ||
        stadium->playmode == PM_Foul_Push_Left ||
        stadium->playmode == PM_Foul_Push_Right ||
        stadium->playmode == PM_Back_Pass_Left ||
        stadium->playmode == PM_Back_Pass_Right ||
        stadium->playmode == PM_Free_Kick_Fault_Left ||
        stadium->playmode == PM_Free_Kick_Fault_Right ||
        stadium->playmode == PM_CatchFault_Left ||
        stadium->playmode == PM_CatchFault_Right ||
        stadium->playmode == PM_TimeOver ) {
        stadium->players.state[player_index] |= STATE_KICK_FAULT;
        return;
    }

    if (!ballKickable(stadium, player_index)) {
        stadium->players.state[player_index] |= STATE_KICK_FAULT;
        #if REFEREES_ENABLED
        ref_failedKickTaken(stadium, player_index);
        #endif
        return;
    }

    float dir_diff = fabsf(angleFromBody(stadium, player_index, stadium->ball_pos_x, stadium->ball_pos_y));
    float dist_ball = r(stadium->ball_pos_x - stadium->players.pos_x[player_index], stadium->ball_pos_y - stadium->players.pos_y[player_index]) - stadium->players.player_type[player_index].player_size - BALL_SIZE;
    float eff_power = power * stadium->players.player_type[player_index].kick_power_rate * 
        (1.0f - 0.25f * dir_diff / M_PIf - 0.25f * dist_ball / stadium->players.player_type[player_index].kickable_margin);
    float accel_x, accel_y;
    from_polar(eff_power, dir + stadium->players.angle_body_committed[player_index], &accel_x, &accel_y);

    // [0.5, 1.0]
    float pos_rate = 0.5f + 0.25f * (dir_diff / M_PIf + dist_ball / stadium->players.player_type[player_index].kickable_margin);
    // [0.5, 1.0]
    float speed_rate = 0.5f + 0.5f * (r(stadium->ball_vel_x, stadium->ball_vel_y) / (BALL_SPEED_MAX * BALL_DECAY));
    // [0, 2*kick_rand]
    float max_rand = stadium->players.kick_rand[player_index] * (power / MAX_POWER) * (pos_rate + speed_rate);
    float kick_noise_x, kick_noise_y;
    from_polar(drand(&stadium->seed, 0.0f, max_rand), 
        drand(&stadium->seed, -M_PIf, M_PIf), &kick_noise_x, &kick_noise_y);
    accel_x += kick_noise_x;
    accel_y += kick_noise_y;
    kickTaken(stadium, player_index, accel_x, accel_y);
    stadium->players.kick_count[player_index] += 1;
}

PUF_FN void tackle(Stadium *stadium, int player_index, double power_or_angle, bool foul) {
    if (stadium->players.command_done[player_index])
        return;

    if (stadium->players.left_leg_command_type[player_index] == NONE)
        stadium->players.left_leg_command_type[player_index] = TACKLE;

    if (stadium->players.right_leg_command_type[player_index] == NONE)
        stadium->players.right_leg_command_type[player_index] = TACKLE;

    stadium->players.command_done[player_index] = true;
    stadium->players.tackle_cycles[player_index] = TACKLE_CYCLES;
    stadium->players.tackle_count[player_index] += 1;
    float player_2_ball_x = stadium->ball_pos_x - stadium->players.pos_x[player_index];
    float player_2_ball_y = stadium->ball_pos_y - stadium->players.pos_y[player_index];
    rotate(&player_2_ball_x, &player_2_ball_y, -stadium->players.angle_body_committed[player_index]);
    float tackle_dist = (player_2_ball_x > 0.0f ? TACKLE_DIST : TACKLE_BACK_DIST);
    if (fabsf( tackle_dist ) <= 1.0e-5f) {
        stadium->players.state[player_index] |= STATE_TACKLE_FAULT;
        return;
    }

    float exponent = TACKLE_EXPONENT;

    // 2009-10-22 akiyama: foul option
    if ( foul ) {
        foul = false;
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (stadium->players.enable[i] && stadium->players.side[i] != stadium->players.side[player_index] && ballKickable(stadium, i)) {
                foul = true;
                exponent = FOUL_EXPONENT;
                break;
            }
        }
    }

    // tackle failure probability
    float prob = (powf(fabsf( player_2_ball_x ) / tackle_dist, exponent ) + 
        powf(fabsf( player_2_ball_y ) / TACKLE_WIDTH, exponent));

    if (prob < 1.0f) {
        if (brand(&stadium->seed, 1 - prob)) {
            stadium->players.state[player_index] |= STATE_TACKLE;
            if (stadium->playmode == PM_BeforeKickOff ||
                stadium->playmode == PM_AfterGoal_Left ||
                stadium->playmode == PM_AfterGoal_Right  ||
                stadium->playmode == PM_OffSide_Left ||
                stadium->playmode == PM_OffSide_Right ||
                stadium->playmode == PM_Illegal_Defense_Left ||
                stadium->playmode == PM_Illegal_Defense_Right ||
                stadium->playmode == PM_Foul_Charge_Left ||
                stadium->playmode == PM_Foul_Charge_Right ||
                stadium->playmode == PM_Foul_Push_Left ||
                stadium->playmode == PM_Foul_Push_Right ||
                stadium->playmode == PM_Back_Pass_Left ||
                stadium->playmode == PM_Back_Pass_Right ||
                stadium->playmode == PM_Free_Kick_Fault_Left ||
                stadium->playmode == PM_Free_Kick_Fault_Right ||
                stadium->playmode == PM_TimeOver )
                return;

            if ( foul )
                stadium->players.tackle_cycles[player_index] = 0;

            float power_rate = 1.0f;
            float accel_x, accel_y;
            // 2008-02-07 akiyama
            // new tackle model based on the Thomas Gabel's proposal
            float angle = NormalizeMoment( power_or_angle );
            float eff_power = (MAX_BACK_TACKLE_POWER + ((MAX_TACKLE_POWER - MAX_BACK_TACKLE_POWER) * 
                ( 1.0f - ( fabsf( angle ) / M_PIf )))) * TACKLE_POWER_RATE;
            eff_power *= 1.0f - 0.5f * (fabsf(th(player_2_ball_x, player_2_ball_y)) / M_PIf);
            from_polar(eff_power, angle + stadium->players.angle_body_committed[player_index], &accel_x, &accel_y);

            // akiyama 2008-01-30
            // new kick noise
            // [0.5, 1]
            float pos_rate = 0.5f + 0.5f * (1.0f - prob);
            // [0.5, 1]
            float speed_rate = 0.5f + 0.5f * (r(stadium->ball_vel_x, stadium->ball_vel_y) / (BALL_SPEED_MAX * BALL_DECAY));
            // [0, 2*tackle_rand]
            // tackle_rand = kick_rand * server::tackle_rand_factor
            float max_rand = stadium->players.kick_rand[player_index] * TACKLE_RAND_FACTOR * power_rate * (pos_rate + speed_rate);
            float tackle_noise_x, tackle_noise_y;
            from_polar(drand(&stadium->seed, 0.0f, max_rand), 
                drand(&stadium->seed, -M_PIf, M_PIf ), &tackle_noise_x, &tackle_noise_y);
            accel_x += tackle_noise_x;
            accel_y += tackle_noise_y;
            tackleTaken(stadium, player_index, accel_x, accel_y, foul);
        }
        else {
            failedTackleTaken(stadium, player_index, foul);
            stadium->players.state[player_index] |= (STATE_TACKLE | STATE_TACKLE_FAULT);
        }
    }
    else {
        failedTackleTaken(stadium, player_index, foul);
        stadium->players.state[player_index] |= STATE_TACKLE_FAULT;
    }
}

#pragma endregion

PUF_FN void noise(float randp, float vel_x, float vel_y, unsigned int *seed, float *out_x, float *out_y) {
    float maxrnd = randp * r(vel_x, vel_y);
    from_polar(drand(seed, 0.0f, maxrnd), drand(seed, -M_PIf, M_PIf), out_x, out_y);
}

PUF_FN void nearestPost(float x, float y, const float size, float *center_x, float *center_y, float* radius) {
    float nearest_gpost_x, nearest_gpost_y;
    if (y > 0) {
        if (x > 0) {
            nearest_gpost_x = PITCH_LENGTH * 0.5f - GOAL_POST_RADIUS;
            nearest_gpost_y = GOAL_WIDTH * 0.5f + GOAL_POST_RADIUS;
        }
        else {
            nearest_gpost_x = -PITCH_LENGTH * 0.5f + GOAL_POST_RADIUS;
            nearest_gpost_y = GOAL_WIDTH * 0.5f + GOAL_POST_RADIUS;
        }
    }
    else {
        if (x > 0 ) {
            nearest_gpost_x = PITCH_LENGTH * 0.5f - GOAL_POST_RADIUS;
            nearest_gpost_y = -GOAL_WIDTH * 0.5f - GOAL_POST_RADIUS;
        }
        else {
            nearest_gpost_x = -PITCH_LENGTH * 0.5f + GOAL_POST_RADIUS;
            nearest_gpost_y = -GOAL_WIDTH * 0.5f - GOAL_POST_RADIUS;
        }
    }

    *center_x = nearest_gpost_x;
    *center_y = nearest_gpost_y;
    *radius = GOAL_POST_RADIUS + size;
}

PUF_FN static inline void nearestEdgeCircle(float center_x, float center_y, const float radius, float x, float y, float *out_x, float *out_y) {
    float diff_x = x - center_x;
    float diff_y = y - center_y;
    if (diff_x == 0.0f && diff_y == 0.0f) {
        diff_x = EPS;
        diff_y = EPS;
    }

    normalize(&diff_x, &diff_y, radius);
    *out_x = center_x + diff_x;
    *out_y = center_y + diff_y;
}

PUF_FN static inline void nearestHEdge(const float l, const float r, const float t, const float b, float x, float y, float *out_x, float *out_y) {
    *out_x = fminf(fmaxf(x, l), r);
    *out_y = fabsf(y - t) < fabsf(y - b) ? t : b;
}

PUF_FN static inline void nearestVEdge(const float l, const float r, const float t, const float b, float x, float y, float *out_x, float *out_y) {
    *out_x = fabsf(x - l) < fabsf(x - r) ? l : r;
    *out_y = fminf(fmaxf(y, t), b);
}

PUF_FN static inline void nearestEdge(const float l, const float r, const float t, const float b, float x, float y, float *out_x, float *out_y) {
    if (fminf(fabsf(x - l), fabsf(x - r)) < fminf(fabsf(y - t), fabsf(y - b)))
        nearestVEdge(l, r, t, b, x, y, out_x, out_y);
    else
        nearestHEdge(l, r, t, b, x, y, out_x, out_y);
}

PUF_FN static inline bool inArea(const float l, const float r, const float t, const float b, float p_x, float p_y) {
    return (p_x >= l) && (p_x <= r) && (p_y <= t) && (p_y >= b);
}

PUF_FN void placePlayersInField(Stadium *stadium) {
    static const float pitch_right = PITCH_LENGTH / 2.0f + PITCH_MARGIN;
    static const float pitch_top = PITCH_WIDTH / 2.0f + PITCH_MARGIN;
    static const float pitch_left = -pitch_right;
    static const float pitch_bottom = -pitch_top;

    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (!stadium->players.enable[i]) continue;
        if (!inArea(pitch_left, pitch_right, pitch_top, pitch_bottom, stadium->players.pos_x[i], stadium->players.pos_y[i])) {
            float new_pos_x, new_pos_y;
            nearestEdge(pitch_left, pitch_right, pitch_top, pitch_bottom, stadium->players.pos_x[i], stadium->players.pos_y[i], &new_pos_x, &new_pos_y);
            stadium->players.pos_x[i] = new_pos_x;
            stadium->players.pos_y[i] = new_pos_y;
        }
    }
}

PUF_FN bool contain(const int *array, const int array_size, const int value) {
    for (int i = 0 ; i < array_size ; i++)
        if (array[i] == value)
            return true;

    return false;
}

PUF_FN void score(Stadium *stadium, const Side side) {
    if (side == LEFT && stadium->team_left_enabled)
        stadium->team_left_points += 1;

    if (side == RIGHT && stadium->team_right_enabled)
        stadium->team_right_points += 1;
}

PUF_FN void penaltyScore(Stadium *stadium, const Side side, const bool scored) {
    if ( side == LEFT && stadium->team_left_enabled )
    {
        if ( scored )
        {
            stadium->team_left_pen_point += 1;
            stadium->team_left_pen_taken += 1;
        }
        else
        {
            stadium->team_left_pen_taken += 1;
        }
    }

    if ( side == RIGHT && stadium->team_right_enabled )
    {
        if ( scored )
        {
            stadium->team_right_pen_point += 1;
            stadium->team_right_pen_taken += 1;
        }
        else
        {
            stadium->team_right_pen_taken += 1;
        }
    }
}

PUF_FN void penaltyWinner(Stadium *stadium, const Side side) {
    if (side == LEFT)
        stadium->team_left_pen_won = true;

    if (side == RIGHT)
        stadium->team_right_pen_won = true;
}

PUF_FN void changePlayMode(Stadium *stadium, const PlayMode pm) {
    stadium->playmode = pm;
    #if REFEREES_ENABLED
    ref_playModeChange(stadium, pm);
    #endif

    if (pm == PM_KickOff_Left
        || pm == PM_KickIn_Left
        || pm == PM_FreeKick_Left
        || pm == PM_IndFreeKick_Left
        || pm == PM_CornerKick_Left
        || pm == PM_GoalKick_Left
        || pm == PM_Illegal_Defense_Right)
        stadium->kick_off_side = LEFT;
    else if (pm == PM_KickOff_Right
        || pm == PM_KickIn_Right
        || pm == PM_FreeKick_Right
        || pm == PM_IndFreeKick_Right
        || pm == PM_CornerKick_Right
        || pm == PM_GoalKick_Right
        || pm == PM_Illegal_Defense_Left)
        stadium->kick_off_side = RIGHT;
    else if (pm == PM_Drop_Ball)
        stadium->kick_off_side = NEUTRAL;

    if (pm == PM_PlayOn)
        stadium->last_playon_start = stadium->time;

    // mydiff
    // if ( pm != PM_AfterGoal_Left
    //      && pm != PM_AfterGoal_Right )
    // {    
    //     sendRefereeAudio( playmode_strings[pm] );
    // }
}

PUF_FN void recoverStaminaCapacity(Stadium *stadium, int player_index) {
    stadium->players.stamina_capacity[player_index] = STAMINA_CAPACITY;
}

PUF_FN void placeBall(Stadium *stadium, const Side kick_off_side, float x, float y) {
    stadium->ball_pos_x = x;
    stadium->ball_pos_y = y;
    stadium->ball_vel_x = 0.0f;
    stadium->ball_vel_y = 0.0f;
    stadium->ball_accel_x = 0.0f;
    stadium->ball_accel_y = 0.0f;
    stadium->kick_off_side = kick_off_side;
}

PUF_FN void callHalfTime(Stadium *stadium, const Side kick_off_side, const int half_time_count) {
    stadium->ball_catcher_index = -1;
    placeBall(stadium, kick_off_side, 0.0f, 0.0f);
    changePlayMode(stadium, PM_BeforeKickOff);

    if (half_time_count < NR_NORMAL_HALFS)
        stadium->time = HALF_TIME * half_time_count;
    else {
        int extra_count = half_time_count - NR_NORMAL_HALFS;
        stadium->time = HALF_TIME * NR_NORMAL_HALFS + EXTRA_HALF_TIME * extra_count;
    }

    // recover only stamina capacity at the start of extra halves
    if (half_time_count == NR_NORMAL_HALFS) {
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;
            recoverStaminaCapacity(stadium, i);
        }
    }

    // mydiff
    // M_weather.halfTime();
}

PUF_FN void kickTaken(Stadium *stadium, int kicker_index, float accel_x, float accel_y) {
    stadium->ball_catcher_index = -1;
    stadium->ball_accel_x += accel_x;
    stadium->ball_accel_y += accel_y;

    #if REFEREES_ENABLED
    const float accel_r = r(accel_x, accel_y);
    ref_kickTaken(stadium, kicker_index, accel_r);
    #endif
}

PUF_FN void tackleTaken(Stadium *stadium, int tackler_index, float accel_x, float accel_y, const bool foul) {
    stadium->ball_catcher_index = -1;
    stadium->ball_accel_x += accel_x;
    stadium->ball_accel_y += accel_y;

    #if REFEREES_ENABLED
    const float accel_r = r(accel_x, accel_y);
    ref_tackleTaken(stadium, tackler_index, accel_r, foul);
    #endif
}

PUF_FN void failedTackleTaken(Stadium *stadium, int tackler_index, const bool foul) {
    #if REFEREES_ENABLED
    ref_failedTackleTaken(stadium, tackler_index, foul);
    #endif
}

PUF_FN void setPlayerState(Stadium *stadium, int player_index, const int state) {
    if (!stadium->players.enable[player_index])
        return;
    stadium->players.state[player_index] |= state;
}

PUF_FN void punishFoulPlay(Stadium *stadium, int player_index) {
    if (!stadium->players.enable[player_index])
        return;
    stadium->players.foul_count[player_index] += 1;
}

PUF_FN void player_disable(Stadium *stadium, int player_index) {
    if (stadium->players.goalie[player_index] && stadium->ball_catcher_index == player_index)
        stadium->ball_catcher_index = -1;

    stadium->players.enable[player_index] = false;
    int card = stadium->players.state[player_index];
    card &= (STATE_YELLOW_CARD | STATE_RED_CARD);
    stadium->players.state[player_index] = STATE_DISABLE;
    stadium->players.state[player_index] |= card;
    stadium->players.pos_x[player_index] = -(stadium->players.unum[player_index] * 3 * stadium->players.side[player_index]);
    stadium->players.pos_y[player_index] = -PITCH_WIDTH / 2.0f - 3.0f;
    stadium->players.vel_x[player_index] = 0.0f;
    stadium->players.vel_y[player_index] = 0.0f;
    stadium->players.accel_x[player_index] = 0.0f;
    stadium->players.accel_y[player_index] = 0.0f;

    // todo : need to figure out how to handle this
    // option 1: move the player to the end of array and change the players_size to active_players_size and reduce it by one.
    // cons: have to shift all the players after the disabled one to the left and then update all the indexes and pointers
    // to the players array (e.g. touch_ref_last_indirect_kicker_index)
    // option 2: have a active_players (initially copied from players array) and then remove the disabled player from it. 
    // cons: do the pointers and indices point to active_players array or players array?
    // cons: how does this work if we want to use SOA? (maybe add active and inactive arrays?)
}

PUF_FN void player_discard(Stadium *stadium, int player_index) {
    if (stadium->players.state[player_index] & STATE_STAND ) {
        player_disable(stadium, player_index);
        if (!(stadium->players.state[player_index] & STATE_DISCARD))
            stadium->players.state[player_index] |= STATE_DISCARD;
        else
            stadium->players.state[player_index] &= ~STATE_DISCARD;
    }
}

PUF_FN void yellowCard(Stadium *stadium, int player_index) {
    if (!stadium->players.enable[player_index])
        return;
    if (stadium->players.card_count[player_index] == 1) {
        player_discard(stadium, player_index);
        stadium->players.state[player_index] &= ~STATE_YELLOW_CARD;
        stadium->players.state[player_index] |= STATE_RED_CARD;
        stadium->players.card_count[player_index] = 2;
    }
    else {
        stadium->players.state[player_index] |= STATE_YELLOW_CARD;
        ++stadium->players.card_count[player_index];
    }
}

PUF_FN void redCard(Stadium *stadium, int player_index) {
    if (!stadium->players.enable[player_index])
        return;
    player_discard(stadium, player_index);
    stadium->players.state[player_index] &= ~STATE_YELLOW_CARD;
    stadium->players.state[player_index] |= STATE_RED_CARD;
    stadium->players.card_count[player_index] = 2;   
}

#if REFEREES_ENABLED

PUF_FN void ref_placeBallAndChangePlayMode(Stadium *stadium, const PlayMode pm, const Side kick_off_side, float x, float y) {
    placeBall(stadium, kick_off_side, x, y);
    if (ref_isPenaltyShootOut(stadium->playmode, NEUTRAL) && (pm == PM_PlayOn || pm ==  PM_Drop_Ball)) {
        ; // never change pm to play_on in penalty mode
    }
    else
        changePlayMode(stadium, pm);
}

PUF_FN bool ref_inPenaltyArea(const Side side, float pos_x, float pos_y) {
    if (side != RIGHT) {
        // according to FIFA the ball is catchable if it is at
        // least partly within the penalty area, thus we add ball size
        static const float pen_left = -PITCH_LENGTH/2 + PENALTY_AREA_LENGTH/2.0 - (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
        static const float pen_right = -PITCH_LENGTH/2 + PENALTY_AREA_LENGTH/2.0 + (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
        static const float pen_top = (PENALTY_AREA_WIDTH + BALL_SIZE * 2) / 2;
        static const float pen_bottom = -pen_top;
        if (inArea(pen_left, pen_right, pen_top, pen_bottom, pos_x, pos_y))
            return true;
    }

    if (side != LEFT) {
        // according to FIFA the ball is catchable if it is at
        // least partly within the penalty area, thus we add ball size
        static const float pen_left = +PITCH_LENGTH/2 - PENALTY_AREA_LENGTH/2.0 - (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
        static const float pen_right = +PITCH_LENGTH/2 - PENALTY_AREA_LENGTH/2.0 + (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
        static const float pen_top = (PENALTY_AREA_WIDTH + BALL_SIZE * 2) / 2;
        static const float pen_bottom = -pen_top;
        if (inArea(pen_left, pen_right, pen_top, pen_bottom, pos_x, pos_y))
            return true;
    }

    return false;
}

PUF_FN void ref_checkFoul(Stadium *stadium, int tackler_index, const bool foul, 
    bool * detect_charge, bool * detect_yellow, bool * detect_red) {
    bool foul_charge = false;
    bool yellow_card = false;
    bool red_card = false;

    // 2011-05-14 akiyama
    // added red card probability
    const float ball_dist2 = distance2(stadium->players.pos_x[tackler_index], stadium->players.pos_y[tackler_index], stadium->ball_pos_x, stadium->ball_pos_y);
    const float ball_angle = th(stadium->ball_pos_x - stadium->players.pos_x[tackler_index], stadium->ball_pos_y - stadium->players.pos_y[tackler_index]);
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;
        if (stadium->players.side[i] == stadium->players.side[tackler_index])
            continue;

        if (!ballKickable(stadium, i))
            continue; // no kickable

        bool pre_check = false;
        if (foul) {
            stadium->players.foul_cycles[i] = FOUL_CYCLES;
            stadium->players.command_done[i] = true;
            stadium->players.state[i] |= STATE_FOUL_CHARGED;
            if (brand(&stadium->seed, stadium->players.player_type[tackler_index].foul_detect_probability)) {
                pre_check = true;
                foul_charge = true;
            }
        }

        if (!(stadium->players.dash_cycles[i] >= 0))
            continue; // no dashing

        float player_rel_x = stadium->players.pos_x[i] - stadium->players.pos_x[tackler_index];
        float player_rel_y = stadium->players.pos_y[i] - stadium->players.pos_y[tackler_index];
        if (r2(player_rel_x, player_rel_y) > ball_dist2)
            continue; // further than ball

        rotate(&player_rel_x, &player_rel_y, -ball_angle);
        if (player_rel_x < 0.0 || fabsf(player_rel_y) > stadium->players.size[i] + stadium->players.size[tackler_index])
            continue;

        float body_diff = fabsf(normalize_angle(stadium->players.angle_body_committed[i] - ball_angle));
        if (body_diff > M_PIf * 0.5f)
            continue;

        if (foul) {
            if (pre_check) {
                yellow_card = true;
                if (brand(&stadium->seed, RED_CARD_PROBABILITY)) {
                    yellow_card = false;
                    red_card = true;
                }
            }
        }
        else {
            if (brand(&stadium->seed, stadium->players.player_type[tackler_index].foul_detect_probability)) {
                foul_charge = true;
                if (brand(&stadium->seed, RED_CARD_PROBABILITY))
                    yellow_card = true;
            }
        }
    }

    *detect_charge = foul_charge;
    *detect_yellow = yellow_card;
    *detect_red = red_card;
}

PUF_FN void ref_clearPlayersFromBall(Stadium *stadium, const Side side) {
    // I would really prefer if we did not teleport players around the field. In
    // my mind players should be given time to move away from the ball and given
    // yellow cards if they repeatedly fail to stay clear.  Two yellows and your
    // out of the game.

    const PlayMode pm = stadium->playmode;
    const float clear_dist = (pm == PM_Illegal_Defense_Left
        || pm == PM_Illegal_Defense_Right
        || pm == PM_Back_Pass_Left
        || pm == PM_Back_Pass_Right
        || ((pm == PM_Foul_Charge_Left
            || pm == PM_Foul_Push_Left)
            && ref_inPenaltyArea(LEFT, stadium->ball_pos_x, stadium->ball_pos_y))
        || ((pm == PM_Foul_Charge_Right
            || pm == PM_Foul_Push_Right)
            && ref_inPenaltyArea(RIGHT, stadium->ball_pos_x, stadium->ball_pos_y)))
        ? GOAL_AREA_LENGTH
        : KICK_OFF_CLEAR_DISTANCE;
    const bool indirect = (pm == PM_Back_Pass_Left
        || pm == PM_Back_Pass_Right
        || pm == PM_Foul_Charge_Left
        || pm == PM_Foul_Charge_Right
        || pm == PM_Foul_Push_Left
        || pm == PM_Foul_Push_Right
        || pm == PM_IndFreeKick_Left
        || pm == PM_IndFreeKick_Right);
    //const double goal_half_width = ServerParam::instance().goalWidth()*0.5;

    const float max_x = PITCH_LENGTH * 0.5f + PITCH_MARGIN;
    const float max_y = PITCH_WIDTH * 0.5f + PITCH_MARGIN;

    const bool ball_at_corner
        = (fabsf(stadium->ball_pos_x) > max_x - clear_dist && fabsf(stadium->ball_pos_y) > max_y - clear_dist);

    for (int loop = 0; loop < 10; ++loop) {
        bool exist = false;
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;
            if (side == NEUTRAL || stadium->players.side[i] == side) {
                if (indirect && fabsf(stadium->players.pos_x[i]) >= PITCH_LENGTH * 0.5f)
                    // defender is allowed to stand on the goal line.
                    continue;

                float clear_area_radius = clear_dist + stadium->players.size[i];
                if (distance(stadium->ball_pos_x, stadium->ball_pos_y, stadium->players.pos_x[i], stadium->players.pos_y[i]) <= clear_area_radius) {
                    float expand_clear_area_radius = clear_dist + stadium->players.size[i] + 1.0e-5f;
                    float new_pos_x, new_pos_y;
                    nearestEdgeCircle(stadium->ball_pos_x, stadium->ball_pos_y, expand_clear_area_radius, stadium->players.pos_x[i], stadium->players.pos_y[i], &new_pos_x, &new_pos_y);
                    if (ball_at_corner && fabsf(new_pos_x) > PITCH_LENGTH * 0.5f && fabsf(new_pos_y) > PITCH_WIDTH * 0.5f) {
                        new_pos_x -= stadium->ball_pos_x;
                        new_pos_y -= stadium->ball_pos_y;
                        rotate(&new_pos_x, &new_pos_y, M_PIf);
                        new_pos_x += stadium->ball_pos_x;
                        new_pos_y += stadium->ball_pos_y;
                    }

                    if (indirect && fabsf(new_pos_x) > PITCH_LENGTH * 0.5f) {
                        float tangent = (new_pos_y - stadium->ball_pos_y) / (new_pos_x - stadium->ball_pos_x);
                        new_pos_x = PITCH_LENGTH * 0.5f * (new_pos_x > 0.0f ? 1.0f : -1.0f);
                        new_pos_y = stadium->ball_pos_y + tangent * (new_pos_x - stadium->ball_pos_x);
                    }

                    if (fabsf(new_pos_x) > max_x) {
                        float r = clear_dist + stadium->players.size[i];
                        float theta = acosf((max_x - fabsf(stadium->ball_pos_x)) / r);
                        float tmp_y = fabsf(r * sinf(theta));
                        new_pos_x = new_pos_x < 0.0f ? -max_x : +max_x;
                        new_pos_y = stadium->ball_pos_y + 
                            (new_pos_y < stadium->ball_pos_y ? - tmp_y - 1.0e-5f : + tmp_y + 1.0e-5f);
                    }

                    if (fabsf(new_pos_y) > max_y) {
                        float r = clear_dist + stadium->players.size[i];
                        float theta = acosf((max_y - fabsf(stadium->ball_pos_y)) / r);
                        float tmp_x = fabsf(r * sinf(theta));
                        new_pos_x = stadium->ball_pos_x + 
                            (new_pos_x < stadium->ball_pos_x ? - tmp_x - 1.0e-5f : + tmp_x + 1.0e-5f);
                        new_pos_y = new_pos_y < 0.0 ? -max_y : +max_y;
                    }

                    if (fabsf(new_pos_x) > max_x || fabsf(new_pos_y) > max_y) {
                        new_pos_x -= stadium->ball_pos_x;
                        new_pos_y -= stadium->ball_pos_y;
                        rotate(&new_pos_x, &new_pos_y, M_PIf);
                        new_pos_x += stadium->ball_pos_x;
                        new_pos_y += stadium->ball_pos_y;
                    }

                    stadium->players.pos_x[i] = new_pos_x;
                    stadium->players.pos_y[i] = new_pos_y;
                    stadium->players.vel_x[i] = 0.0f;
                    stadium->players.vel_y[i] = 0.0f;
                    stadium->players.accel_x[i] = 0.0f;
                    stadium->players.accel_y[i] = 0.0f;
                    exist = true;
                }
            }
        }

        if (exist)
            collisions(stadium);
        else
            break;
    }
}

PUF_FN bool ref_isPenaltyShootOut(const PlayMode pm, const Side side) {
    bool bLeft = false, bRight = true;
    switch (pm) {
    case PM_PenaltySetup_Left:
    case PM_PenaltyReady_Left:
    case PM_PenaltyTaken_Left:
    case PM_PenaltyMiss_Left:
    case PM_PenaltyScore_Left:
        bLeft = true;
        break;
    case PM_PenaltySetup_Right:
    case PM_PenaltyReady_Right:
    case PM_PenaltyTaken_Right:
    case PM_PenaltyMiss_Right:
    case PM_PenaltyScore_Right:
        bRight = true;
        break;
    default:
        return false;
    }

    if (side == NEUTRAL && (bLeft == true || bRight == true))
        return true;
    else if (side == LEFT && bLeft == true)
        return true;
    else if (side == RIGHT && bRight == true)
        return true;
    else
        return false;
}

PUF_FN void ref_placePlayersInTheirField(Stadium *stadium) {
    const bool kick_off_offside = (KICK_OFF_OFFSIDE && (stadium->playmode == PM_KickOff_Left || stadium->playmode == PM_KickOff_Right));
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;
        switch (stadium->players.side[i]) {
        case LEFT:
            if (stadium->players.pos_x[i] > 0) {
                if (kick_off_offside) {
                    stadium->players.pos_x[i] = -stadium->players.size[i];
                    stadium->players.pos_y[i] = stadium->players.pos_y[i];
                }
                else {
                    stadium->players.pos_x[i] = drand(&stadium->seed, -PITCH_LENGTH/2.0f, 0.0f);
                    stadium->players.pos_y[i] = drand(&stadium->seed, -PITCH_WIDTH/2.0f, PITCH_WIDTH/2.0f);
                }
            }
            break;
        case RIGHT:
            if (stadium->players.pos_x[i] < 0) {
                if (kick_off_offside) {
                    stadium->players.pos_x[i] = stadium->players.size[i];
                    stadium->players.pos_y[i] = stadium->players.pos_y[i];
                }
                else {
                    stadium->players.pos_x[i] = drand(&stadium->seed, 0.0f, PITCH_LENGTH/2.0f);
                    stadium->players.pos_y[i] = drand(&stadium->seed, -PITCH_WIDTH/2.0f, PITCH_WIDTH/2.0f);
                }
            }
            break;
        case NEUTRAL:
        default:
            break;
        }

        if (stadium->players.side[i] != stadium->kick_off_side) {
            float expand_c_radius = KICK_OFF_CLEAR_DISTANCE + stadium->players.size[i];
            if (distance(0.0f, 0.0f, stadium->players.pos_x[i], stadium->players.pos_y[i]) <= expand_c_radius)
                nearestEdgeCircle(0.0f, 0.0f, expand_c_radius, stadium->players.pos_x[i], stadium->players.pos_y[i], &stadium->players.pos_x[i], &stadium->players.pos_y[i]);
        }
    }
}

PUF_FN void ref_truncateToPitch(float *x, float *y) {
    *x = fminf(*x, +PITCH_LENGTH * 0.5f);
    *x = fmaxf(*x, -PITCH_LENGTH * 0.5f);
    *y = fminf(*y, +PITCH_WIDTH * 0.5f);
    *y = fmaxf(*y, -PITCH_WIDTH * 0.5f);
}

PUF_FN void ref_moveOutOfPenalty(const Side side, float *x, float *y) {
    if (side != RIGHT) {
        if (*x <= (-PITCH_LENGTH * 0.5f +PENALTY_AREA_LENGTH) && fabsf(*y) <= PENALTY_AREA_WIDTH * 0.5f) {
            *x = add_eps(-PITCH_LENGTH * 0.5f + PENALTY_AREA_LENGTH);
            if (*y > 0)
                *y = add_eps(+PENALTY_AREA_WIDTH * 0.5f);
            else
                *y = subtract_eps(-PENALTY_AREA_WIDTH * 0.5f);
        }
    }

    if (side != LEFT) {
        if (*x >= (PITCH_LENGTH * 0.5 - PENALTY_AREA_LENGTH) && fabsf(*y) <= PENALTY_AREA_WIDTH * 0.5) {
            *x = subtract_eps(PITCH_LENGTH * 0.5f - PENALTY_AREA_LENGTH);
            if(*y > 0)
                *y = add_eps(+PENALTY_AREA_WIDTH * 0.5f);
            else
                *y = subtract_eps(-PENALTY_AREA_WIDTH * 0.5f);
        }
    }
}

PUF_FN void ref_moveOutOfGoalArea(const Side side, float *x, float *y) {
    if (side != RIGHT)
        if (*x <= (-PITCH_LENGTH * 0.5f + GOAL_AREA_LENGTH) && fabsf(*y) <= GOAL_AREA_WIDTH * 0.5f)
            *x = add_eps(-PITCH_LENGTH * 0.5f + GOAL_AREA_LENGTH);

    if (side != LEFT)
        if (*x >= (PITCH_LENGTH * 0.5f - GOAL_AREA_LENGTH) && fabsf(*y) <= GOAL_AREA_WIDTH * 0.5f)
            *x = subtract_eps(PITCH_LENGTH * 0.5f - GOAL_AREA_LENGTH);
}

PUF_FN void ref_awardDropBall(Stadium *stadium, float x, float y) {
    stadium->ball_catcher_index = -1;
    ref_truncateToPitch(&x, &y);
    ref_moveOutOfPenalty(NEUTRAL, &x, &y);

    ref_placeBallAndChangePlayMode(stadium, PM_Drop_Ball, NEUTRAL, x, y);
    placePlayersInField(stadium);

    if (!ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        changePlayMode(stadium, PM_PlayOn);
}

PUF_FN void ref_awardFreeKick(Stadium *stadium, const Side side, float x, float y) {
    ref_truncateToPitch(&x, &y);
    ref_moveOutOfPenalty((Side)(-side), &x, &y);

    if (side == LEFT)
        ref_placeBallAndChangePlayMode(stadium, PM_FreeKick_Left, LEFT, x, y);
    else if(side == RIGHT)
        ref_placeBallAndChangePlayMode(stadium, PM_FreeKick_Right, RIGHT, x, y);
}

PUF_FN void ref_awardGoalKick(Stadium *stadium, const Side side, float x, float y) {
    if (y > 0.0f)
        y = GOAL_AREA_WIDTH * 0.5f;
    else
        y = -GOAL_AREA_WIDTH * 0.5f;

    stadium->ball_catcher_index = -1;
    if (side == LEFT) {
        x = -PITCH_LENGTH * 0.5f + GOAL_AREA_LENGTH;
        ref_placeBallAndChangePlayMode(stadium, PM_GoalKick_Left, LEFT, x, y);
    }
    else {
        x = PITCH_LENGTH * 0.5f - GOAL_AREA_LENGTH;
        ref_placeBallAndChangePlayMode(stadium, PM_GoalKick_Right, RIGHT, x, y);
    }
}

PUF_FN void ref_awardCornerKick(Stadium *stadium, const Side side, float x, float y) {
    stadium->ball_catcher_index = -1;
    if (y > 0)
        y = PITCH_WIDTH * 0.5f - CORNER_KICK_MARGIN;
    else
        y = -PITCH_WIDTH * 0.5f + CORNER_KICK_MARGIN;

    if (side == LEFT) {
        x = PITCH_LENGTH * 0.5f - CORNER_KICK_MARGIN;
        ref_placeBallAndChangePlayMode(stadium, PM_CornerKick_Left, LEFT, x, y);
    }
    else {
        x = -PITCH_LENGTH * 0.5f + CORNER_KICK_MARGIN;
        ref_placeBallAndChangePlayMode(stadium, PM_CornerKick_Right, RIGHT, x, y);
    }
}

PUF_FN void ref_awardKickIn(Stadium *stadium, const Side side, float x, float y) {
    stadium->ball_catcher_index = -1;
    ref_truncateToPitch(&x, &y);
    if (side == LEFT)
        ref_placeBallAndChangePlayMode(stadium, PM_KickIn_Left, LEFT, x, y);
    else
        ref_placeBallAndChangePlayMode(stadium, PM_KickIn_Right, RIGHT, x, y);
}

PUF_FN bool ref_crossGoalLine(Stadium *stadium, const Side side, float prev_ball_pos_x, float prev_ball_pos_y) {
    if (prev_ball_pos_x == stadium->ball_pos_x)
        // ball cannot have crossed gline
        return false;

    if (fabsf(stadium->ball_pos_x) <= PITCH_LENGTH * 0.5f + BALL_SIZE)
        // ball hasn't crossed gline
        return false;

    if (fabsf(prev_ball_pos_x) > PITCH_LENGTH * 0.5f + BALL_SIZE)
        // ball already over the gline
        return false;

    if ((side * stadium->ball_pos_x) >= 0.0f)
        //ball in wrong half
        return false;

    if (fabsf(prev_ball_pos_y) > (GOAL_WIDTH * 0.5f + GOAL_POST_RADIUS)
        && fabsf(prev_ball_pos_x) > PITCH_LENGTH * 0.5f)
        // then the only goal that could have been scored would be
        // from going behind the goal post.  I'm pretty sure that
        // isn't possible anyway, but just in case this function acts
        // as a double check
        return false;

    float delta_x = stadium->ball_pos_x - prev_ball_pos_x;
    float delta_y = stadium->ball_pos_y - prev_ball_pos_y;

    // we already checked above that ball.pos.x != prev_ball_pos.x, so delta_x cannot be zero.
    float gradient = delta_y / delta_x;
    float offset = prev_ball_pos_y - gradient * prev_ball_pos_x;

    // determine y for x = ServerParam::PITCH_LENGTH*0.5 + ServerParam::instance().ballSize() * -side
    float x = (PITCH_LENGTH * 0.5f + BALL_SIZE) * -side;
    float y_intercept = gradient * x + offset;

    return fabsf(y_intercept) <= (GOAL_WIDTH * 0.5f + GOAL_POST_RADIUS);
}

#endif

#if TIME_REFEREE_ENABLED

PUF_FN void time_ref_analyse(Stadium *stadium) {
    const PlayMode pm = stadium->playmode;
    if (pm == PM_BeforeKickOff
        || pm == PM_TimeOver
        || pm == PM_AfterGoal_Right
        || pm == PM_AfterGoal_Left
        || pm == PM_OffSide_Right
        || pm == PM_OffSide_Left
        || pm == PM_Illegal_Defense_Left
        || pm == PM_Illegal_Defense_Right
        || pm == PM_Foul_Charge_Right
        || pm == PM_Foul_Charge_Left
        || pm == PM_Foul_Push_Right
        || pm == PM_Foul_Push_Left
        || pm == PM_Back_Pass_Right
        || pm == PM_Back_Pass_Left
        || pm == PM_Free_Kick_Fault_Right
        || pm == PM_Free_Kick_Fault_Left
        || pm == PM_CatchFault_Right
        || pm == PM_CatchFault_Left)
        return;

    /* if a value of half_time is negative, then ignore time. */
    if (HALF_TIME > 0) {
        int normal_time = HALF_TIME * NR_NORMAL_HALFS;
        int maximum_time = normal_time + EXTRA_HALF_TIME * NR_EXTRA_HALFS;

        /* check for penalty shoot-outs, half_time and extra_time. */
        if (stadium->time >= maximum_time) {
            if (PENALTY_SHOOT_OUTS && stadium->team_left_points == stadium->team_right_points)
                return; // handled by PenaltyRef
            else {
                // mydiff
                // stadium.sendRefereeAudio( "time_up" );
                changePlayMode(stadium, PM_TimeOver);
                return;
            }
        }
        // overtime
        else if (stadium->time >= normal_time) {
            int extra_count = (stadium->time_ref_s_half_time_count + 1) - NR_NORMAL_HALFS;
            if (!stadium->team_left_enabled || !stadium->team_right_enabled) {
                // mydiff
                // M_stadium.sendRefereeAudio( "time_up_without_a_team" );
                changePlayMode(stadium, PM_TimeOver);
                return;
            }
            // when golden_goal is on,
            //    referee always checks the score difference.
            //    if score is different, the game is finished immediately.
            else if (GOLDEN_GOAL && stadium->team_left_points != stadium->team_right_points) {
                // mydiff
                // M_stadium.sendRefereeAudio( "time_up" );
                changePlayMode(stadium, PM_TimeOver);
                return;
            }
            // check half time in overtime
            else if (stadium->time >= (normal_time + (EXTRA_HALF_TIME * extra_count))) {
                // when normal halves have just finished (i.e. extra_count==0),
                // referee always check the score difference.
                if (extra_count == 0 && stadium->team_left_points != stadium->team_right_points) {
                    // mydiff
                    // M_stadium.sendRefereeAudio( "time_up" );
                    changePlayMode(stadium, PM_TimeOver);
                }
                // otherwise, the game is go into the overtime.
                else {
                    ++stadium->time_ref_s_half_time_count;
                    // mydiff
                    // M_stadium.sendRefereeAudio( "time_extended" );
                    Side kick_off_side = (stadium->time_ref_s_half_time_count % 2 == 0 ? LEFT : RIGHT);
                    callHalfTime(stadium, kick_off_side, stadium->time_ref_s_half_time_count);
                    ref_placePlayersInTheirField(stadium);
                }

                return;
            }
        }
        // if not in overtime, check whether halfTime() cycles have been passed
        else if (stadium->time >= HALF_TIME * ( stadium->time_ref_s_half_time_count + 1 )) {
            ++stadium->time_ref_s_half_time_count;
            Side kick_off_side = (stadium->time_ref_s_half_time_count % 2 == 0 ? LEFT : RIGHT);
            // mydiff
            // M_stadium.sendRefereeAudio( "half_time" );
            callHalfTime(stadium, kick_off_side, stadium->time_ref_s_half_time_count);
            ref_placePlayersInTheirField(stadium);
            return;
        }
    }
}

#endif

#if BALL_STUCK_REFEREE_ENABLED

PUF_FN void ball_stuck_ref_analyse(Stadium *stadium) {
    if (BALL_STUCK_AREA <= 0.0f || DROP_BALL_TIME <= 0)
        return;

    if (stadium->playmode != PM_PlayOn) {
        stadium->ball_stuck_ref_last_ball_pos_x = stadium->ball_pos_x;
        stadium->ball_stuck_ref_last_ball_pos_y = stadium->ball_pos_y;
        stadium->ball_stuck_ref_counter = 0;
        return;
    }

    if (distance2(stadium->ball_pos_x, stadium->ball_pos_y, stadium->ball_stuck_ref_last_ball_pos_x, stadium->ball_stuck_ref_last_ball_pos_x) <= powf(BALL_STUCK_AREA, 2.0f)) {
        stadium->ball_stuck_ref_counter += 1;
        if (stadium->ball_stuck_ref_counter >= DROP_BALL_TIME) {
            stadium->ball_stuck_ref_last_ball_pos_x = stadium->ball_pos_x;
            stadium->ball_stuck_ref_last_ball_pos_y = stadium->ball_pos_y;
            stadium->ball_stuck_ref_counter = 0;
            ref_awardDropBall(stadium, stadium->ball_pos_x, stadium->ball_pos_y);
        }
    }
    else {
        stadium->ball_stuck_ref_last_ball_pos_x = stadium->ball_pos_x;
        stadium->ball_stuck_ref_last_ball_pos_y = stadium->ball_pos_y;
        stadium->ball_stuck_ref_counter = 0;
    }
}

#endif

#if OFFSIDE_REFEREE_ENABLED

PUF_FN void offside_ref_playModeChange(Stadium *stadium, PlayMode pm) {
    if (pm != PM_PlayOn)
        stadium->offside_ref_offside_candidates_size = 0;
}

PUF_FN void offside_ref_checkPlayerAfterOffside(Stadium *stadium) {
    Side offsideside = NEUTRAL;
    float center_x = 0.0f, center_y = 0.0f, size_x = 0.0f, size_y = 0.0f;
    if (stadium->playmode == PM_OffSide_Right) {
        center_x = +PITCH_LENGTH / 4 + stadium->offside_ref_offside_pos_x / 2;
        size_x = PITCH_LENGTH / 2 - stadium->offside_ref_offside_pos_x;
        size_y = PITCH_WIDTH;
        offsideside = RIGHT;
    }
    else if (stadium->playmode == PM_OffSide_Left) {
        center_x = -PITCH_LENGTH / 4 + stadium->offside_ref_offside_pos_x / 2;
        size_x = PITCH_LENGTH / 2 + stadium->offside_ref_offside_pos_x;
        size_y = PITCH_WIDTH;
        offsideside = LEFT;
    }
    else
        return;

    const float c_center_x = stadium->offside_ref_offside_pos_x;
    const float c_center_y = stadium->offside_ref_offside_pos_y;
    const float c_radius = 2.5f;
    const float left = center_x - size_x / 2.0f;
    const float right = center_x + size_x / 2.0f;
    const float top = center_y + size_y / 2.0f;
    const float bottom = center_y - size_y / 2.0f;
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;
        if ( stadium->players.side[i] != offsideside ) continue;

        if (distance(c_center_x, c_center_y, stadium->players.pos_x[i], stadium->players.pos_y[i]) <= c_radius)
            nearestEdgeCircle(c_center_x, c_center_y, c_radius, stadium->players.pos_x[i], stadium->players.pos_y[i], &stadium->players.pos_x[i], &stadium->players.pos_y[i]);

        if (!inArea(left, right, top, bottom, stadium->players.pos_x[i], stadium->players.pos_y[i])) {
            float new_pos_x, new_pos_y;
            nearestVEdge(left, right, top, bottom, stadium->players.pos_x[i], stadium->players.pos_y[i], &new_pos_x, &new_pos_y);
            if (stadium->playmode == PM_OffSide_Right )
                new_pos_x += OFFSIDE_KICK_MARGIN;
            else
                new_pos_x -= OFFSIDE_KICK_MARGIN;
            stadium->players.pos_x[i] = new_pos_x;
            stadium->players.pos_y[i] = new_pos_y;
        }
    }
}

PUF_FN void offside_ref_analyse(Stadium *stadium) {
    if (!USE_OFFSIDE)
        return;

    if (ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    if (stadium->playmode == PM_BeforeKickOff || stadium->playmode == PM_KickOff_Left || stadium->playmode == PM_KickOff_Right) {
        if (KICK_OFF_OFFSIDE)
            ref_placePlayersInTheirField(stadium);
        return;
    }

    if (stadium->playmode == PM_OffSide_Left) {
        stadium->offside_ref_after_offside_time += 1;
        if (stadium->offside_ref_after_offside_time > AFTER_OFFSIDE_WAIT - CLEAR_PLAYER_TIME)
            ref_clearPlayersFromBall(stadium, LEFT);

        offside_ref_checkPlayerAfterOffside(stadium);
        if (stadium->offside_ref_after_offside_time > AFTER_OFFSIDE_WAIT)
            changePlayMode(stadium, PM_FreeKick_Right);
        return;
    }

    if (stadium->playmode == PM_OffSide_Right) {
        stadium->offside_ref_after_offside_time += 1;
        if (stadium->offside_ref_after_offside_time > AFTER_OFFSIDE_WAIT - CLEAR_PLAYER_TIME)
            ref_clearPlayersFromBall(stadium, RIGHT);

        offside_ref_checkPlayerAfterOffside(stadium);
        if (stadium->offside_ref_after_offside_time > AFTER_OFFSIDE_WAIT)
            changePlayMode(stadium, PM_FreeKick_Left);
        return;
    }

    if (stadium->playmode != PM_PlayOn) {
        stadium->offside_ref_offside_candidates_size = 0;
        return;
    }
}

PUF_FN void offside_ref_callOffside(Stadium *stadium) {
    if ( ref_isPenaltyShootOut( stadium->playmode, NEUTRAL ) )
        return;

    if ( stadium->offside_ref_last_kicker_side == NEUTRAL )
        return;

    static const float pt_l = -(PITCH_LENGTH - 2.0f * CORNER_KICK_MARGIN) * 0.5f;
    static const float pt_r = (PITCH_LENGTH - 2.0f * CORNER_KICK_MARGIN) * 0.5f;
    static const float pt_t = (PITCH_WIDTH - 2.0f * CORNER_KICK_MARGIN) * 0.5f;
    static const float pt_b = -(PITCH_WIDTH - 2.0f * CORNER_KICK_MARGIN) * 0.5f;

    static const float g_l_l = -PITCH_LENGTH / 2.0f + GOAL_AREA_LENGTH / 2.0f - GOAL_AREA_LENGTH * 0.5f;
    static const float g_l_r = -PITCH_LENGTH / 2.0f + GOAL_AREA_LENGTH / 2.0f + GOAL_AREA_LENGTH * 0.5f;
    static const float g_l_t = GOAL_AREA_WIDTH * 0.5f;
    static const float g_l_b = -GOAL_AREA_WIDTH * 0.5f;

    static const float g_r_l = +PITCH_LENGTH / 2.0f - GOAL_AREA_LENGTH / 2.0f - GOAL_AREA_LENGTH * 0.5f;
    static const float g_r_r = +PITCH_LENGTH / 2.0f - GOAL_AREA_LENGTH / 2.0f + GOAL_AREA_LENGTH * 0.5f;
    static const float g_r_t = GOAL_AREA_WIDTH * 0.5f;
    static const float g_r_b = -GOAL_AREA_WIDTH * 0.5f;

    float pos_x, pos_y;
    if (stadium->offside_ref_offside_pos_x > PITCH_LENGTH / 2.0f
        || inArea(g_r_l, g_r_r, g_r_t, g_r_b, stadium->offside_ref_offside_pos_x, stadium->offside_ref_offside_pos_y)) {
        pos_x = + PITCH_LENGTH/2.0f - GOAL_AREA_LENGTH;
        pos_y = ( stadium->offside_ref_offside_pos_y > 0 ? 1 : -1 ) * GOAL_AREA_WIDTH/2.0f;
    }
    else if ( stadium->offside_ref_offside_pos_x < - PITCH_LENGTH / 2.0f
        || inArea(g_l_l, g_l_r, g_l_t, g_l_b, stadium->offside_ref_offside_pos_x, stadium->offside_ref_offside_pos_y)) {
        pos_x = - PITCH_LENGTH/2.0f + GOAL_AREA_LENGTH;
        pos_y = ( stadium->offside_ref_offside_pos_y > 0 ? 1 : -1 ) * GOAL_AREA_WIDTH/2.0f;
    }
    else if ( !inArea(pt_l, pt_r, pt_t, pt_b, stadium->offside_ref_offside_pos_x, stadium->offside_ref_offside_pos_y))
        nearestEdge(pt_l, pt_r, pt_t, pt_b, stadium->offside_ref_offside_pos_x, stadium->offside_ref_offside_pos_y, &pos_x, &pos_y);
    else {
        pos_x = stadium->offside_ref_offside_pos_x;
        pos_y = stadium->offside_ref_offside_pos_y;
    }

    if (stadium->offside_ref_last_kicker_side == LEFT)
        ref_placeBallAndChangePlayMode(stadium, PM_OffSide_Left, RIGHT, pos_x, pos_y);
    else
        ref_placeBallAndChangePlayMode(stadium, PM_OffSide_Right, LEFT, pos_x, pos_y);

    stadium->ball_catcher_index = -1;
    stadium->offside_ref_offside_candidates_size = 0;
    placePlayersInField(stadium);
    //clearPlayersFromBall( M_last_kicker_side );
    stadium->offside_ref_after_offside_time = 0;
}

PUF_FN void offside_ref_setOffsideMark(Stadium *stadium, int kicker_index, const float accel_r) {
    if (!USE_OFFSIDE)
        return;

    if ( stadium->offside_ref_last_kick_time == stadium->time
         && stadium->offside_ref_last_kick_stoppage_time == stadium->stoppage_time )
    {
        stadium->offside_ref_last_kicker_side = NEUTRAL;
        stadium->offside_ref_offside_candidates_size = 0;
        return;
    }

    if ( stadium->offside_ref_last_kick_time != stadium->time )
    {
        stadium->offside_ref_last_kick_accel_r = 0.0;
    }

    stadium->offside_ref_last_kick_time = stadium->time;
    stadium->offside_ref_last_kick_stoppage_time = stadium->stoppage_time;

    if ( stadium->offside_ref_last_kick_accel_r < accel_r )
    {
        stadium->offside_ref_last_kicker_side = stadium->players.side[kicker_index];
        stadium->offside_ref_last_kick_accel_r = accel_r;
    }

    for (int i = 0 ; i < stadium->offside_ref_offside_candidates_size ; i++) {
        if (stadium->offside_ref_offside_candidates_player_index[i] == kicker_index) {
            float candidate_pos_x = stadium->offside_ref_offside_candidates_pos_x[i];
            float candidate_pos_y = stadium->offside_ref_offside_candidates_pos_y[i];
            stadium->offside_ref_offside_pos_x = candidate_pos_x;
            stadium->offside_ref_offside_pos_y = candidate_pos_y;
            offside_ref_callOffside(stadium);
            return;
        }
    }

    stadium->offside_ref_offside_candidates_size = 0;

    if ( stadium->playmode == PM_GoalKick_Left
         || stadium->playmode == PM_GoalKick_Right
         || stadium->playmode == PM_KickIn_Left
         || stadium->playmode == PM_KickIn_Right
         || stadium->playmode == PM_CornerKick_Left
         || stadium->playmode == PM_CornerKick_Right )
    {
        return;
    }

    float first = 0.0;
    float second = 0.0;
    float offside_line = 0.0;

    switch (stadium->players.side[kicker_index]) {
    case LEFT:
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;

            if ( stadium->players.side[i] == RIGHT )
            {
                if ( stadium->players.pos_x[i] > second )
                {
                    second = stadium->players.pos_x[i];
                    if ( second > first )
                    {
                        float temp = first;
                        first = second;
                        second = temp;
                    }
                }
            }
        }

        if ( second > stadium->ball_pos_x )
        {
            offside_line = second;
        }
        else
        {
            offside_line = stadium->ball_pos_x;
        }

        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;

            if (stadium->players.side[i] == LEFT && stadium->players.pos_x[i] > offside_line && stadium->players.unum[i] != stadium->players.unum[kicker_index]) {
                stadium->offside_ref_offside_candidates_player_index[stadium->offside_ref_offside_candidates_size] = i;
                stadium->offside_ref_offside_candidates_pos_x[stadium->offside_ref_offside_candidates_size] = offside_line;
                stadium->offside_ref_offside_candidates_pos_y[stadium->offside_ref_offside_candidates_size] = stadium->players.pos_y[i];
                stadium->offside_ref_offside_candidates_size += 1;
            }
        }
        break;

    case RIGHT:
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;

            if ( stadium->players.side[i] == LEFT )
            {
                if ( stadium->players.pos_x[i] < second )
                {
                    second = stadium->players.pos_x[i];
                    if ( second < first )
                    {
                        float temp = first;
                        first = second;
                        second = temp;
                    }
                }
            }
        }

        if ( second < stadium->ball_pos_x )
        {
            offside_line = second;
        }
        else
        {
            offside_line = stadium->ball_pos_x;
        }

        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;

            if (stadium->players.side[i] == RIGHT && stadium->players.pos_x[i] < offside_line && stadium->players.unum[i] != stadium->players.unum[kicker_index]) {
                stadium->offside_ref_offside_candidates_player_index[stadium->offside_ref_offside_candidates_size] = i;
                stadium->offside_ref_offside_candidates_pos_x[stadium->offside_ref_offside_candidates_size] = offside_line;
                stadium->offside_ref_offside_candidates_pos_y[stadium->offside_ref_offside_candidates_size] = stadium->players.pos_y[i];
                stadium->offside_ref_offside_candidates_size += 1;
            }
        }
        break;
    case NEUTRAL:
    default:
        break;
    }
}

PUF_FN void offside_ref_ballTouched(Stadium *stadium, int player_index) {
    offside_ref_setOffsideMark(stadium, player_index, 0.0f);
}

PUF_FN void offside_ref_checkIntentionalAction(Stadium *stadium, int kicker_index) {
    if (!USE_OFFSIDE)
        return;

    for (int i = 0 ; i < stadium->offside_ref_offside_candidates_size ; i++) {
        if (stadium->offside_ref_offside_candidates_player_index[i] == kicker_index && 
            distance2(stadium->players.pos_x[kicker_index], stadium->players.pos_y[kicker_index], stadium->ball_pos_x, stadium->ball_pos_y) < 
            powf(OFFSIDE_ACTIVE_AREA_SIZE, 2.0f)) {
            stadium->offside_ref_offside_pos_x = stadium->offside_ref_offside_candidates_pos_x[i];
            stadium->offside_ref_offside_pos_y = stadium->offside_ref_offside_candidates_pos_y[i];
            offside_ref_callOffside(stadium);
        }
    }
}

PUF_FN void offside_ref_failedKickTaken(Stadium *stadium, int kicker_index) {
    offside_ref_checkIntentionalAction(stadium, kicker_index);
}

PUF_FN void offside_ref_kickTaken(Stadium *stadium, int kicker_index, const float accel_r) {
    offside_ref_setOffsideMark(stadium, kicker_index, accel_r);
}

PUF_FN void offside_ref_tackleTaken(Stadium *stadium, int tackler_index, const double accel_r, const bool foul) {
    offside_ref_setOffsideMark(stadium, tackler_index, accel_r );
}

PUF_FN void offside_ref_failedTackleTaken(Stadium *stadium, int tackler_index, const bool foul) {
    offside_ref_checkIntentionalAction(stadium, tackler_index );
}

#endif

#if FREE_KICK_REFEREE_ENABLED

PUF_FN bool free_kick_ref_goalKick(PlayMode pm) {
    return (pm == PM_GoalKick_Right || pm == PM_GoalKick_Left);
}

PUF_FN bool free_kick_ref_freeKick(PlayMode pm) {
    switch(pm) {
    case PM_KickOff_Right:
    case PM_KickIn_Right:
    case PM_FreeKick_Right:
    case PM_CornerKick_Right:
    case PM_IndFreeKick_Right:
    case PM_KickOff_Left:
    case PM_KickIn_Left:
    case PM_FreeKick_Left:
    case PM_CornerKick_Left:
    case PM_IndFreeKick_Left:
        return true;
    default:
        return false;
    }
}

PUF_FN void free_kick_ref_playModeChange(Stadium *stadium, PlayMode pm) {
    if (pm != PM_PlayOn)
        stadium->free_kick_ref_kick_taken = false;

    if (pm == PM_KickOff_Left
        || pm == PM_KickIn_Left
        || pm == PM_FreeKick_Left
        || pm == PM_IndFreeKick_Left
        || pm == PM_CornerKick_Left
        || pm == PM_GoalKick_Left)
        ref_clearPlayersFromBall(stadium, RIGHT );
    else if (pm == PM_KickOff_Right
        || pm == PM_KickIn_Right
        || pm == PM_FreeKick_Right
        || pm == PM_IndFreeKick_Right
        || pm == PM_CornerKick_Right
        || pm == PM_GoalKick_Right )
        ref_clearPlayersFromBall(stadium, LEFT );
    else if (pm == PM_Drop_Ball)
        ref_clearPlayersFromBall(stadium, NEUTRAL );

    if (free_kick_ref_goalKick(pm)) {
        stadium->free_kick_ref_timer = DROP_BALL_TIME;
        if (!free_kick_ref_goalKick(stadium->playmode))
            stadium->free_kick_ref_goal_kick_count = 0;
        else
            stadium->free_kick_ref_goal_kick_count++;
    }
    else
        stadium->free_kick_ref_goal_kick_count = 0;

    if (free_kick_ref_freeKick(pm))
        stadium->free_kick_ref_timer = DROP_BALL_TIME;

    if (!free_kick_ref_freeKick(pm) && !free_kick_ref_goalKick(pm))
        stadium->free_kick_ref_timer = -1;

    if (pm == PM_Free_Kick_Fault_Left || pm == PM_Free_Kick_Fault_Right) {
        stadium->ball_catcher_index = -1;
        stadium->free_kick_ref_after_free_kick_fault_time = 0;
    }
}

PUF_FN void free_kick_ref_placePlayersForGoalkick(Stadium *stadium) {
    static const float p_l_left = -PITCH_LENGTH/2 + PENALTY_AREA_LENGTH/2.0 - (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
    static const float p_l_right = -PITCH_LENGTH/2 + PENALTY_AREA_LENGTH/2.0 + (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
    static const float p_l_top = (PENALTY_AREA_WIDTH + BALL_SIZE * 2) / 2;
    static const float p_l_bottom = -p_l_top;

    static const float p_r_left = +PITCH_LENGTH/2 - PENALTY_AREA_LENGTH/2.0 - (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
    static const float p_r_right = -PITCH_LENGTH/2 + PENALTY_AREA_LENGTH/2.0 + (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
    static const float p_r_top = (PENALTY_AREA_WIDTH + BALL_SIZE * 2) / 2;
    static const float p_r_bottom = -p_r_top;

    float p_area_left;
    float p_area_right;
    float p_area_top;
    float p_area_bottom;
    int oppside;
    if (stadium->playmode == PM_GoalKick_Left) {
        oppside = RIGHT;
        p_area_left = p_l_left;
        p_area_right = p_l_right;
        p_area_top = p_l_top;
        p_area_bottom = p_l_bottom;
    }
    else {
        oppside = LEFT;
        p_area_left = p_r_left;
        p_area_right = p_r_right;
        p_area_top = p_r_top;
        p_area_bottom = p_r_bottom;
    }

    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;
        if (stadium->players.side[i] == oppside) {
            const float size = stadium->players.size[i];
            float expand_area_left = p_area_left - size;
            float expand_area_right = p_area_right + size;
            float expand_area_top = p_area_top - size;
            float expand_area_bottom = p_area_bottom + size;

            if (inArea(expand_area_left, expand_area_right, expand_area_top, expand_area_bottom, stadium->players.pos_x[i], stadium->players.pos_y[i])) {
                float new_pos_x, new_pos_y;
                nearestEdge(expand_area_left, expand_area_right, expand_area_top, expand_area_bottom, 
                    stadium->players.pos_x[i], stadium->players.pos_y[i], &new_pos_x, &new_pos_y);
                if (new_pos_x * oppside >= PITCH_LENGTH / 2.0f)
                    new_pos_x = (PITCH_LENGTH / 2.0f - PENALTY_AREA_LENGTH - size) * oppside;

                stadium->players.pos_x[i] = new_pos_x;
                stadium->players.pos_y[i] = new_pos_y;
            }
        }
    }
}

PUF_FN void free_kick_ref_analyse(Stadium *stadium) {
    const PlayMode pm = stadium->playmode;
    if (ref_isPenaltyShootOut(pm, NEUTRAL))
        return;

    if (pm == PM_Free_Kick_Fault_Left) {
        stadium->free_kick_ref_after_free_kick_fault_time += 1;
        if (stadium->free_kick_ref_after_free_kick_fault_time > AFTER_FREE_KICK_FAULT_WAIT - CLEAR_PLAYER_TIME)
            ref_clearPlayersFromBall(stadium, LEFT);

        if ( stadium->free_kick_ref_after_free_kick_fault_time > AFTER_FREE_KICK_FAULT_WAIT)
            changePlayMode(stadium, PM_FreeKick_Right);
        
        return;
    }

    if (pm == PM_Free_Kick_Fault_Right) {
        stadium->free_kick_ref_after_free_kick_fault_time += 1;
        if (stadium->free_kick_ref_after_free_kick_fault_time > AFTER_FREE_KICK_FAULT_WAIT - CLEAR_PLAYER_TIME)
            ref_clearPlayersFromBall(stadium, RIGHT);

        if ( stadium->free_kick_ref_after_free_kick_fault_time > AFTER_FREE_KICK_FAULT_WAIT )
            changePlayMode(stadium, PM_FreeKick_Left);
        
        return;
    }

    if (pm == PM_Back_Pass_Left
        || pm == PM_Back_Pass_Right
        || pm == PM_CatchFault_Left
        || pm == PM_CatchFault_Right)
        // analyzed by CatchRef
        return;

    if (pm == PM_GoalKick_Right || pm == PM_GoalKick_Left) {
        free_kick_ref_placePlayersForGoalkick(stadium);
        placePlayersInField(stadium);
        if (!ref_inPenaltyArea(NEUTRAL, stadium->ball_pos_x, stadium->ball_pos_y))
            changePlayMode(stadium, PM_PlayOn);
        else {
            if (stadium->free_kick_ref_kick_taken && PROPER_GOAL_KICKS) {
                if (r(stadium->ball_vel_x, stadium->ball_vel_y) < STOPPED_BALL_VEL) {
                    if (stadium->free_kick_ref_goal_kick_count >= MAX_GOAL_KICKS)
                        ref_awardFreeKick(stadium, (Side)(-stadium->players.side[stadium->free_kick_ref_kick_taker_index]), stadium->ball_pos_x, stadium->ball_pos_y);
                    else
                        ref_awardGoalKick(stadium, stadium->players.side[stadium->free_kick_ref_kick_taker_index], stadium->ball_pos_x, stadium->ball_pos_y);
                }
            }
            else {
                if (stadium->free_kick_ref_timer > -1)
                    stadium->free_kick_ref_timer--;

                if (stadium->free_kick_ref_timer == 0)
                    ref_awardDropBall(stadium, stadium->ball_pos_x, stadium->ball_pos_y);
            }
        }

        return;
    }

    if (pm != PM_PlayOn
        && pm != PM_BeforeKickOff
        && pm != PM_TimeOver
        && pm != PM_AfterGoal_Right
        && pm != PM_AfterGoal_Left
        && pm != PM_OffSide_Right
        && pm != PM_OffSide_Left
        && pm != PM_Illegal_Defense_Left
        && pm != PM_Illegal_Defense_Right
        && pm != PM_Foul_Charge_Right
        && pm != PM_Foul_Charge_Left
        && pm != PM_Foul_Push_Right
        && pm != PM_Foul_Push_Left
        && pm != PM_Back_Pass_Right
        && pm != PM_Back_Pass_Left
        && pm != PM_Free_Kick_Fault_Right
        && pm != PM_Free_Kick_Fault_Left
        && pm != PM_CatchFault_Right
        && pm != PM_CatchFault_Left ) {
        if (stadium->ball_vel_x != 0.0f || stadium->ball_vel_y != 0.0f)
            changePlayMode(stadium, PM_PlayOn);
    }

    placePlayersInField(stadium);
    if (pm != PM_PlayOn
        && pm != PM_TimeOver
        && pm != PM_GoalKick_Left
        && pm != PM_GoalKick_Right
        && pm != PM_OffSide_Left
        && pm != PM_OffSide_Right
        && pm != PM_Illegal_Defense_Left
        && pm != PM_Illegal_Defense_Right
        && pm != PM_Foul_Charge_Left
        && pm != PM_Foul_Charge_Right
        && pm != PM_Foul_Push_Right
        && pm != PM_Foul_Push_Left
        && pm != PM_Back_Pass_Left
        && pm != PM_Back_Pass_Right
        && pm != PM_Free_Kick_Fault_Left
        && pm != PM_Free_Kick_Fault_Right
        && pm != PM_CatchFault_Left
        && pm != PM_CatchFault_Right )
        ref_clearPlayersFromBall(stadium, (Side)(-stadium->kick_off_side));

    if (pm == PM_KickOff_Right
        || pm == PM_KickIn_Right
        || pm == PM_FreeKick_Right
        || pm == PM_CornerKick_Right
        || pm == PM_IndFreeKick_Right
        || pm == PM_KickOff_Left
        || pm == PM_KickIn_Left
        || pm == PM_FreeKick_Left
        || pm == PM_CornerKick_Left
        || pm == PM_IndFreeKick_Left) {
        if (stadium->free_kick_ref_timer > -1)
            stadium->free_kick_ref_timer -= 1;

        if (stadium->free_kick_ref_timer == 0)
            ref_awardDropBall(stadium, stadium->ball_pos_x, stadium->ball_pos_y);
    }
}

PUF_FN void free_kick_ref_callFreeKickFault(Stadium *stadium, Side side, float x, float y) {
    ref_truncateToPitch(&x, &y);
    ref_moveOutOfPenalty(side, &x, &y);
    stadium->ball_catcher_index = -1;
    if ( side == LEFT )
        ref_placeBallAndChangePlayMode(stadium, PM_Free_Kick_Fault_Left, RIGHT, x, y);
    else if ( side == RIGHT )
        ref_placeBallAndChangePlayMode(stadium, PM_Free_Kick_Fault_Right, LEFT, x, y);

    stadium->free_kick_ref_after_free_kick_fault_time = 0;
}

PUF_FN void free_kick_ref_ballTouched(Stadium *stadium, int player_index) {
    if ( ( stadium->playmode == PM_GoalKick_Left
           && stadium->players.side[player_index] != LEFT )
         || ( stadium->playmode == PM_GoalKick_Right
              && stadium->players.side[player_index] != RIGHT )
         ) {
        // opponent player kicks tha ball while ball is in penalty area.
        ref_awardGoalKick(stadium, (Side)(-stadium->players.side[player_index]), stadium->ball_pos_x, stadium->ball_pos_y);
        stadium->free_kick_ref_goal_kick_count = -1;
        stadium->free_kick_ref_kick_taken = false;
        return;
    }

    if ( stadium->free_kick_ref_kick_taken ) {
        if ( player_index == stadium->free_kick_ref_kick_taker_index && FREE_KICK_FAULTS) {
            if ( stadium->players.dash_count[stadium->free_kick_ref_kick_taker_index] > stadium->free_kick_ref_kick_taker_dashes ) {
                setPlayerState(stadium, stadium->free_kick_ref_kick_taker_index, STATE_FREE_KICK_FAULT);
                free_kick_ref_callFreeKickFault(stadium, stadium->players.side[stadium->free_kick_ref_kick_taker_index],
                                   stadium->ball_pos_x, stadium->ball_pos_y);
            }
            /// else do nothing yet as the player just colided with the ball instead of dashing into it
        }
        else
            stadium->free_kick_ref_kick_taken = false;
    }
}

PUF_FN void free_kick_ref_kickTaken(Stadium *stadium, int kicker_index, const float accel_r) {
    if (ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    if (free_kick_ref_goalKick(stadium->playmode)) {
        if ((stadium->playmode == PM_GoalKick_Left && stadium->players.side[kicker_index] != LEFT)
            || (stadium->playmode == PM_GoalKick_Right && stadium->players.side[kicker_index] != RIGHT)) {
            // opponent player kicks tha ball while ball is in penalty areas.
            ref_awardGoalKick(stadium, (Side)(-stadium->players.side[kicker_index]), stadium->ball_pos_x, stadium->ball_pos_y);
            stadium->free_kick_ref_goal_kick_count = -1;
            stadium->free_kick_ref_kick_taken = false;
            return;
        }

        if (stadium->free_kick_ref_kick_taken) {
            // ball was not kicked directly into play (i.e. out of penalty area
            // without touching another player
            if (kicker_index != stadium->free_kick_ref_kick_taker_index) {
                if (PROPER_GOAL_KICKS)
                    ref_awardGoalKick(stadium, stadium->players.side[stadium->free_kick_ref_kick_taker_index], stadium->ball_pos_x, stadium->ball_pos_y);
            }
            else if (stadium->free_kick_ref_kick_taker_dashes != stadium->players.dash_count[kicker_index]) {
                if (FREE_KICK_FAULTS) {
                    setPlayerState(stadium, kicker_index, STATE_FREE_KICK_FAULT);
                    free_kick_ref_callFreeKickFault(stadium, stadium->players.side[kicker_index], stadium->ball_pos_x, stadium->ball_pos_y);
                }
                else if (PROPER_GOAL_KICKS) {
                    ref_awardGoalKick(stadium, stadium->players.side[kicker_index], stadium->ball_pos_x, stadium->ball_pos_y);
                }
            }
            // else it's part of a compound kick
        }
        //else
        //{
        stadium->free_kick_ref_kick_taken = true;
        stadium->free_kick_ref_kick_taker_index = kicker_index;
        stadium->free_kick_ref_kick_taker_dashes = stadium->players.dash_count[kicker_index];
        //}
    }
    else if (stadium->playmode == PM_OffSide_Left || stadium->playmode == PM_OffSide_Right) {
        // do nothing
    }
    else if (stadium->playmode != PM_PlayOn) {
        stadium->free_kick_ref_kick_taken = true;
        stadium->free_kick_ref_kick_taker_index = kicker_index;
        stadium->free_kick_ref_kick_taker_dashes = stadium->players.dash_count[kicker_index];
        changePlayMode(stadium, PM_PlayOn);
    }
    else // PM_PlayOn
    {
        if (stadium->free_kick_ref_kick_taken) {
            if (kicker_index == stadium->free_kick_ref_kick_taker_index && FREE_KICK_FAULTS) {
                if (stadium->players.dash_count[kicker_index] > stadium->free_kick_ref_kick_taker_dashes) {
                    setPlayerState(stadium, kicker_index, STATE_FREE_KICK_FAULT);
                    free_kick_ref_callFreeKickFault(stadium, stadium->players.side[kicker_index], stadium->ball_pos_x, stadium->ball_pos_y);
                }
            }
            else {
                stadium->free_kick_ref_kick_taken = false;
            }
        }
    }
}

PUF_FN void free_kick_ref_tackleTaken(Stadium *stadium, int tackler_index, const double accel_r, const bool foul) {
    free_kick_ref_kickTaken(stadium, tackler_index, accel_r );
}

#endif

#if TOUCH_REFEREE_ENABLED

PUF_FN bool touch_ref_indirectFreeKick(const PlayMode pm){
    switch(pm) {
    case PM_IndFreeKick_Right:
    case PM_IndFreeKick_Left:
        return true;
    default:
        return false;
    }
}

PUF_FN void touch_ref_playModeChange(Stadium *stadium, PlayMode pm) {
    if (pm != PM_PlayOn)
        stadium->touch_ref_last_touched_index = -1;

    if (touch_ref_indirectFreeKick(pm)) {
        stadium->touch_ref_last_indirect_kicker_index = -1;
        stadium->touch_ref_indirect_mode = true;
    }
    else if (pm != PM_PlayOn && pm != PM_Drop_Ball) {
        stadium->touch_ref_last_indirect_kicker_index = -1;
        stadium->touch_ref_indirect_mode = false;
    }
}

PUF_FN bool touch_ref_checkGoal(Stadium *stadium) {
    if (stadium->playmode == PM_AfterGoal_Left
        || stadium->playmode == PM_AfterGoal_Right
        || stadium->playmode == PM_TimeOver)
        return false;

    if (stadium->touch_ref_indirect_mode)
        return false;

    // FIFA rules:  Ball has to be completely outside of the pitch to be considered out
    //    static RArea pt( PVector(0.0,0.0),
    //                       PVector( ServerParam::PITCH_LENGTH
    //                                + ServerParam::instance().ballSize() * 2,
    //                                ServerParam::PITCH_WIDTH
    //                                + ServerParam::instance().ballSize() * 2 ) );

    if (fabsf(stadium->ball_pos_x) <= PITCH_LENGTH * 0.5 + BALL_SIZE)
        return false;

    if ((stadium->ball_catcher_index == -1 || stadium->players.side[stadium->ball_catcher_index] == LEFT)
        && ref_crossGoalLine(stadium, LEFT, stadium->touch_ref_prev_ball_pos_x, stadium->touch_ref_prev_ball_pos_y)
        && !ref_isPenaltyShootOut(stadium->playmode, NEUTRAL)) {
        score(stadium, RIGHT);
        // mydiff
        // announceGoal( M_stadium.teamRight() );
        stadium->touch_ref_after_goal_time = 0;
        placeBall(stadium, LEFT, stadium->ball_pos_x, stadium->ball_pos_y);
        if (HALF_TIME >= 0 && GOLDEN_GOAL && stadium->time >= HALF_TIME * NR_NORMAL_HALFS)
            changePlayMode(stadium, PM_TimeOver);
        else
            changePlayMode(stadium, PM_AfterGoal_Right);
        return true;
    }
    else if ((stadium->ball_catcher_index == -1 || stadium->players.side[stadium->ball_catcher_index] == RIGHT)
        && ref_crossGoalLine(stadium, RIGHT, stadium->touch_ref_prev_ball_pos_x, stadium->touch_ref_prev_ball_pos_y)
        && ! ref_isPenaltyShootOut(stadium->playmode, NEUTRAL)) {
        score(stadium, LEFT);
        // mydiff
        // announceGoal( M_stadium.teamLeft() );
        stadium->touch_ref_after_goal_time = 0;
        placeBall(stadium, RIGHT, stadium->ball_pos_x, stadium->ball_pos_y);
        if (HALF_TIME >= 0 && GOLDEN_GOAL && stadium->time >= HALF_TIME * NR_NORMAL_HALFS)
            changePlayMode(stadium, PM_TimeOver);
        else
            changePlayMode(stadium, PM_AfterGoal_Left);
        return true;
    }

    return false;
}

PUF_FN void touch_ref_analyse(Stadium *stadium) {
    if (ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    if (stadium->playmode == PM_AfterGoal_Left ) {
        if ( ++stadium->touch_ref_after_goal_time > AFTER_GOAL_WAIT ) {
            ref_placeBallAndChangePlayMode(stadium, PM_KickOff_Right, RIGHT, 0.0f, 0.0f);
            ref_placePlayersInTheirField(stadium);
        }
        return;
    }

    if (stadium->playmode == PM_AfterGoal_Right ) {
        if ( ++stadium->touch_ref_after_goal_time > AFTER_GOAL_WAIT ) {
            ref_placeBallAndChangePlayMode(stadium, PM_KickOff_Left, LEFT, 0.0f, 0.0f);
            ref_placePlayersInTheirField(stadium);
        }
        return;
    }

    if (touch_ref_checkGoal(stadium))
        return;

    if (stadium->playmode != PM_AfterGoal_Left
        && stadium->playmode != PM_AfterGoal_Right
        && stadium->playmode != PM_TimeOver) {
        if (fabsf(stadium->ball_pos_x) > PITCH_LENGTH * 0.5f + BALL_SIZE) {
            // check for goal kick or corner kick
            Side side = NEUTRAL;
            if (stadium->touch_ref_last_touched_index != -1)
                side = stadium->players.side[stadium->touch_ref_last_touched_index];

            if (stadium->ball_pos_x > PITCH_LENGTH * 0.5f + BALL_SIZE) {
                if (side == RIGHT)
                    ref_awardCornerKick(stadium, LEFT, stadium->ball_pos_x, stadium->ball_pos_y);
                else if (stadium->ball_catcher_index == -1)
                    ref_awardGoalKick(stadium, RIGHT, stadium->ball_pos_x, stadium->ball_pos_y);
                else
                    // the ball is caught and the goalie must have
                    // moved to a position beyond the opponents goal
                    // line.  Let the catch ref clean up the mess
                    return;
            }
            else if (stadium->ball_pos_x < PITCH_LENGTH * 0.5f - BALL_SIZE) {
                if (side == LEFT)
                    ref_awardCornerKick(stadium, RIGHT, stadium->ball_pos_x, stadium->ball_pos_y);
                else if (stadium->ball_catcher_index == -1)
                    ref_awardGoalKick(stadium, LEFT, stadium->ball_pos_x, stadium->ball_pos_y);
                else
                    // the ball is caught and the goalie must have
                    // moved to a position beyond the opponents goal
                    // line.  Let the catch ref clean up the mess
                    return;
            }
        }
        else if (fabsf(stadium->ball_pos_y) > PITCH_WIDTH * 0.5f + BALL_SIZE) {
            // check for kick in.
            Side side = NEUTRAL;
            if (stadium->touch_ref_last_touched_index != -1)
                side = stadium->players.side[stadium->touch_ref_last_touched_index];

            if (side == NEUTRAL)
                // somethings gone wrong but give a drop ball
                ref_awardDropBall(stadium, stadium->ball_pos_x, stadium->ball_pos_y);
            else
                ref_awardKickIn(stadium, (Side)(-side), stadium->ball_pos_x, stadium->ball_pos_y);
        }
    }

    stadium->touch_ref_prev_ball_pos_x = stadium->ball_pos_x;
    stadium->touch_ref_prev_ball_pos_y = stadium->ball_pos_y;
}

PUF_FN void touch_ref_ballTouched(Stadium *stadium, int kicker_index) {
    if ( fabsf( stadium->ball_pos_x )
         <= PITCH_LENGTH * 0.5 + BALL_SIZE )
    {
        if ( stadium->playmode == PM_PlayOn
             && stadium->touch_ref_last_indirect_kicker_index != -1
             && kicker_index != stadium->touch_ref_last_indirect_kicker_index )
        {
            stadium->touch_ref_last_indirect_kicker_index = -1;
            stadium->touch_ref_indirect_mode = false;
        }

        if ( stadium->touch_ref_indirect_mode )
        {
            stadium->touch_ref_last_indirect_kicker_index = kicker_index;
        }

        if ( stadium->touch_ref_last_touched_time == stadium->time
             && stadium->touch_ref_last_touched_accel_r <= 0.0 )
        {
            stadium->touch_ref_last_touched_index = kicker_index;
            stadium->touch_ref_last_touched_accel_r = 0.0;
        }
        else if ( stadium->touch_ref_last_touched_time < stadium->time )
        {
            stadium->touch_ref_last_touched_index = kicker_index;
            stadium->touch_ref_last_touched_time = stadium->time;
            stadium->touch_ref_last_touched_accel_r = 0.0;
        }
    }
}

PUF_FN void touch_ref_kickTaken(Stadium *stadium, int kicker_index, const float accel_r) {
    if (fabsf(stadium->ball_pos_x) <= PITCH_LENGTH * 0.5f + BALL_SIZE) {
        if (stadium->playmode == PM_PlayOn && stadium->touch_ref_last_indirect_kicker_index != -1 && kicker_index != stadium->touch_ref_last_indirect_kicker_index) {
            stadium->touch_ref_last_indirect_kicker_index = -1;
            stadium->touch_ref_indirect_mode = false;
        }

        if (stadium->touch_ref_indirect_mode )
            stadium->touch_ref_last_indirect_kicker_index = kicker_index;

        if (stadium->touch_ref_last_touched_time == stadium->time && stadium->touch_ref_last_touched_accel_r <= accel_r) {
            stadium->touch_ref_last_touched_index = kicker_index;
            stadium->touch_ref_last_touched_accel_r = accel_r;
        }
        else if (stadium->touch_ref_last_touched_time < stadium->time) {
            stadium->touch_ref_last_touched_index = kicker_index;
            stadium->touch_ref_last_touched_time = stadium->time;
            stadium->touch_ref_last_touched_accel_r = accel_r;
        }
    }
}

PUF_FN void touch_ref_tackleTaken(Stadium *stadium, int tackler_index, const double accel_r, const bool foul) {
    touch_ref_kickTaken(stadium, tackler_index, accel_r );
}
#endif

#if CATCH_REFEREE_ENABLED

PUF_FN void catch_ref_playModeChange(Stadium *stadium, PlayMode pm) {
    if (pm != PM_PlayOn) {
        stadium->catch_ref_before_last_back_passer_index = -1;
        stadium->catch_ref_last_back_passer_index = -1;
    }

    if (pm == PM_Back_Pass_Left || pm == PM_Back_Pass_Right) {
        stadium->ball_catcher_index = -1;
        stadium->catch_ref_after_back_pass_time = 0;
    }
    else if (pm == PM_CatchFault_Left || pm == PM_CatchFault_Right) {
        stadium->ball_catcher_index = -1;
        stadium->catch_ref_after_catch_fault_time = 0;
    }
}

PUF_FN void catch_ref_callCatchFault(Stadium *stadium, Side side, float x, float y) {
    ref_truncateToPitch(&x, &y);
    //pos = moveIntoPenalty( side, pos );
    ref_moveOutOfPenalty(side, &x, &y);
    stadium->ball_catcher_index = -1;
    if ( side == LEFT )
        ref_placeBallAndChangePlayMode(stadium, PM_CatchFault_Left, RIGHT, x, y);
    else if ( side == RIGHT )
        ref_placeBallAndChangePlayMode(stadium, PM_CatchFault_Right, LEFT, x, y);

    stadium->catch_ref_after_catch_fault_time = 0;
}

PUF_FN void catch_ref_analyse(Stadium *stadium) {
    const PlayMode pm = stadium->playmode;

    stadium->catch_ref_team_l_touched = false;
    stadium->catch_ref_team_r_touched = false;

    if ( ref_isPenaltyShootOut(pm, NEUTRAL) )
    {
        return;
    }

    if ( pm == PM_Back_Pass_Left )
    {
        ++stadium->catch_ref_after_back_pass_time;

        //clearPlayersFromBall( LEFT );
        if ( stadium->catch_ref_after_back_pass_time > AFTER_BACKPASS_WAIT - CLEAR_PLAYER_TIME )
        {
            ref_clearPlayersFromBall(stadium, LEFT );
        }

        if ( stadium->catch_ref_after_back_pass_time > AFTER_BACKPASS_WAIT )
        {
            //M_stadium.changePlayMode( PM_FreeKick_Right );
            changePlayMode(stadium, PM_IndFreeKick_Right );
        }
        return;
    }

    if ( pm == PM_Back_Pass_Right )
    {
        ++stadium->catch_ref_after_back_pass_time;

        //clearPlayersFromBall( RIGHT );
        if ( stadium->catch_ref_after_back_pass_time > AFTER_BACKPASS_WAIT - CLEAR_PLAYER_TIME )
        {
            ref_clearPlayersFromBall(stadium, RIGHT );
        }

        if ( stadium->catch_ref_after_back_pass_time > AFTER_BACKPASS_WAIT )
        {
            //M_stadium.changePlayMode( PM_FreeKick_Left );
            changePlayMode(stadium, PM_IndFreeKick_Left );
        }
        return;
    }

    if ( pm == PM_CatchFault_Left )
    {
        ++stadium->catch_ref_after_catch_fault_time;

        //clearPlayersFromBall( LEFT );
        if ( stadium->catch_ref_after_catch_fault_time > AFTER_CATCH_FAULT_WAIT - CLEAR_PLAYER_TIME )
        {
            ref_clearPlayersFromBall(stadium, LEFT );
        }

        if ( stadium->catch_ref_after_catch_fault_time > AFTER_CATCH_FAULT_WAIT )
        {
            //M_stadium.changePlayMode( PM_IndFreeKick_Right );
            changePlayMode(stadium, PM_FreeKick_Right );
        }
        return;
    }

    if ( pm == PM_CatchFault_Right )
    {
        ++stadium->catch_ref_after_catch_fault_time;

        //clearPlayersFromBall( RIGHT );
        if ( stadium->catch_ref_after_catch_fault_time > AFTER_CATCH_FAULT_WAIT - CLEAR_PLAYER_TIME )
        {
            ref_clearPlayersFromBall(stadium, RIGHT );
        }

        if ( stadium->catch_ref_after_catch_fault_time > AFTER_CATCH_FAULT_WAIT )
        {
            //M_stadium.changePlayMode( PM_IndFreeKick_Left );
            changePlayMode(stadium, PM_FreeKick_Left );
        }
        return;
    }


    if (stadium->ball_catcher_index != -1
        && pm != PM_AfterGoal_Left
        && pm != PM_AfterGoal_Right
        && pm != PM_TimeOver
        && ! ref_inPenaltyArea(stadium->players.side[stadium->ball_catcher_index], stadium->ball_pos_x, stadium->ball_pos_y))
        catch_ref_callCatchFault(stadium, stadium->players.side[stadium->ball_catcher_index], stadium->ball_pos_x, stadium->ball_pos_y);
}

PUF_FN void catch_ref_ballTouched(Stadium *stadium, int player_index) {
    // If ball is not kicked, back pass violation is never taken.

    if ( stadium->players.side[player_index] == LEFT )
    {
        stadium->catch_ref_team_l_touched = true;
    }
    else if ( stadium->players.side[player_index] == RIGHT )
    {
        stadium->catch_ref_team_r_touched = true;
    }

    if ( stadium->catch_ref_team_l_touched && stadium->catch_ref_team_r_touched )
    {
        stadium->catch_ref_before_last_back_passer_index = -1;
        stadium->catch_ref_last_back_passer_index = -1;
    }
}

PUF_FN void catch_ref_kickTaken(Stadium *stadium, int kicker_index, const float accel_r) {
    if (stadium->players.side[kicker_index] == LEFT)
        stadium->catch_ref_team_l_touched = true;
    else if (stadium->players.side[kicker_index] == RIGHT)
        stadium->catch_ref_team_r_touched = true;

    if (stadium->catch_ref_team_l_touched && stadium->catch_ref_team_r_touched) {
        stadium->catch_ref_last_back_passer_index = -1;
        stadium->catch_ref_before_last_back_passer_index = -1;
        return;
    }

    //! check if a different player kicked the ball
    if (kicker_index != stadium->catch_ref_last_back_passer_index) {
        stadium->catch_ref_before_last_back_passer_index = stadium->catch_ref_last_back_passer_index;
        stadium->catch_ref_last_back_passer_index = kicker_index;
        stadium->catch_ref_last_back_passer_time = stadium->time;
    }
    else {
        //! same player is kicking the ball again, update last kick time
        stadium->catch_ref_last_back_passer_time = stadium->time;
    }
}

PUF_FN void catch_ref_tackleTaken(Stadium *stadium, int tackler_index, const double accel_r, const bool foul) {
    catch_ref_kickTaken(stadium, tackler_index, accel_r );
}
#endif

#if FOUL_REFEREE_ENABLED

PUF_FN void foul_ref_playModeChange(Stadium *stadium, PlayMode pm) {
    if (pm == PM_Foul_Charge_Left
        || pm == PM_Foul_Charge_Right
        || pm == PM_Foul_Push_Left
        || pm == PM_Foul_Push_Right) {
        stadium->foul_ref_after_foul_time = 0;
    }
}

PUF_FN void foul_ref_analyse(Stadium *stadium) {
    const PlayMode pm = stadium->playmode;

    if ( ref_isPenaltyShootOut( pm, NEUTRAL ) )
    {
        return;
    }

    if ( pm == PM_Foul_Charge_Left
         || pm == PM_Foul_Push_Left )
    {
        ++stadium->foul_ref_after_foul_time;

        //clearPlayersFromBall( LEFT );
        if ( stadium->foul_ref_after_foul_time > AFTER_FOUL_WAIT - CLEAR_PLAYER_TIME )
        {
            ref_clearPlayersFromBall(stadium, LEFT );
        }

        if ( stadium->foul_ref_after_foul_time > AFTER_FOUL_WAIT )
        {
            if ( ref_inPenaltyArea( LEFT, stadium->ball_pos_x, stadium->ball_pos_y ) )
            {
                changePlayMode(stadium, PM_IndFreeKick_Right );
            }
            else
            {
                changePlayMode(stadium, PM_FreeKick_Right );
            }
        }
        return;
    }

    if ( pm == PM_Foul_Charge_Right
         || pm == PM_Foul_Push_Right )
    {
        ++stadium->foul_ref_after_foul_time;

        //clearPlayersFromBall( RIGHT );
        if ( stadium->foul_ref_after_foul_time > AFTER_FOUL_WAIT - CLEAR_PLAYER_TIME )
        {
            ref_clearPlayersFromBall(stadium, RIGHT );
        }

        if ( stadium->foul_ref_after_foul_time > AFTER_FOUL_WAIT )
        {
            if ( ref_inPenaltyArea( RIGHT, stadium->ball_pos_x, stadium->ball_pos_y) )
            {
                changePlayMode(stadium, PM_IndFreeKick_Left );
            }
            else
            {
                changePlayMode(stadium, PM_FreeKick_Left );
            }
        }
        return;
    }
}

PUF_FN void foul_ref_callFoul(Stadium *stadium, int tackler_index) {
    float pos_x = stadium->players.pos_x[tackler_index], pos_y = stadium->players.pos_y[tackler_index];
    ref_truncateToPitch(&pos_x, &pos_y);
    ref_moveOutOfGoalArea(NEUTRAL, &pos_x, &pos_y);
    stadium->ball_catcher_index = -1;
    if (stadium->players.side[tackler_index] == LEFT) {
        ref_moveOutOfPenalty(RIGHT, &pos_x, &pos_y);
        ref_placeBallAndChangePlayMode(stadium, PM_Foul_Charge_Left, RIGHT, pos_x, pos_y);
    }
    else if (stadium->players.side[tackler_index] == RIGHT) {
        ref_moveOutOfPenalty(LEFT, &pos_x, &pos_y);
        ref_placeBallAndChangePlayMode(stadium, PM_Foul_Charge_Right, LEFT, pos_x, pos_y);
    }

    punishFoulPlay(stadium, tackler_index);
    stadium->foul_ref_after_foul_time = 0;
}

PUF_FN void foul_ref_callYellowCard(Stadium *stadium, int tackler_index) {
    foul_ref_callFoul(stadium, tackler_index);
    yellowCard(stadium, tackler_index);
}

PUF_FN void foul_ref_callRedCard(Stadium *stadium, int tackler_index) {
    foul_ref_callFoul(stadium, tackler_index);
    redCard(stadium, tackler_index);
}

PUF_FN void foul_ref_tackleTaken(Stadium *stadium, int tackler_index, const double accel_r, const bool foul) {
    if (ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    bool detect_charge = false;
    bool detect_yellow = false;
    bool detect_red = false;
    ref_checkFoul(stadium, tackler_index, foul, &detect_charge, &detect_yellow, &detect_red);
    if (detect_red)
        foul_ref_callRedCard(stadium, tackler_index);
    else if (detect_yellow)
        foul_ref_callYellowCard(stadium, tackler_index);
    else if (detect_charge)
        foul_ref_callFoul(stadium, tackler_index);
}
#endif

#if PENALTY_REFEREE_ENABLED

PUF_FN void penalty_ref_playModeChange(Stadium *stadium, PlayMode pm) {
    // if mode changes, reset the timer
    if (!ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    if (pm == PM_PenaltySetup_Left || pm == PM_PenaltySetup_Right) {
        stadium->penalty_ref_last_taker_index = -1;
        stadium->penalty_ref_timer = PEN_SETUP_WAIT;
    }
    else if (pm == PM_PenaltyReady_Left || pm == PM_PenaltyReady_Right) {
        stadium->penalty_ref_last_taker_index = -1;
        stadium->penalty_ref_timer = PEN_READY_WAIT;
    }
    else if (pm == PM_PenaltyTaken_Left || pm == PM_PenaltyTaken_Right)
        stadium->penalty_ref_timer = PEN_TAKEN_WAIT;
    else if (pm == PM_PenaltyMiss_Left  || pm == PM_PenaltyMiss_Right 
        || pm == PM_PenaltyScore_Left || pm == PM_PenaltyScore_Right) {
        stadium->penalty_ref_last_taker_index = -1;
        stadium->penalty_ref_timer = PEN_BEFORE_SETUP_WAIT;
    }
    else
        stadium->penalty_ref_last_taker_index = -1;
}

PUF_FN void penalty_ref_penalty_init(Stadium *stadium) {
    // change the play mode such that the other side can take the penalty
    // and place the ball at the penalty spot
    stadium->penalty_ref_cur_pen_taker = stadium->penalty_ref_cur_pen_taker == LEFT ? RIGHT : LEFT;
    PlayMode pm = stadium->penalty_ref_cur_pen_taker == LEFT ? PM_PenaltySetup_Left : PM_PenaltySetup_Right;
    ref_placeBallAndChangePlayMode(stadium, pm, NEUTRAL, -stadium->penalty_ref_pen_side * (PITCH_LENGTH / 2.0f - PEN_DIST_X), 0.0f);
}

PUF_FN void penalty_ref_startPenaltyShootout(Stadium *stadium) {
    // if normal and extra time are over -> start the penalty procedure or quit
    if ( stadium->penalty_ref_first_time
         && PENALTY_SHOOT_OUTS
         && stadium->playmode != PM_BeforeKickOff
         && stadium->team_left_points == stadium->team_right_points
         && ( ( HALF_TIME < 0
                && NR_NORMAL_HALFS + NR_EXTRA_HALFS == 0 )
              || ( HALF_TIME >= 0
                   && ( stadium->time >=
                        ( HALF_TIME * NR_NORMAL_HALFS
                          + EXTRA_HALF_TIME * NR_EXTRA_HALFS ) ) )
              )
         )
    {
        if ( drand(&stadium->seed, 0, 1 ) < 0.5 )       // choose random side of the playfield
        {
            stadium->penalty_ref_pen_side = LEFT;            // and inform players
        }
        else
        {
            stadium->penalty_ref_pen_side = RIGHT;
        }

        stadium->ball_catcher_index = -1;
        // mydiff
        // M_stadium.sendRefereeAudio( stadium->penalty_ref_pen_side == LEFT
        //                             ? "penalty_onfield_l"
        //                             : "penalty_onfield_r" );

        // choose at random who starts (note that in penalty_init, actually the
        // opposite player is chosen since there the playMode changes)
        stadium->penalty_ref_cur_pen_taker = ( drand(&stadium->seed, 0, 1 ) < 0.5 ) ? LEFT : RIGHT;

        // place the goalkeeper of the opposite field close to the penalty goal
        // otherwise it is hard to get there before pen_setup_wait cycles
        Side side = ( stadium->penalty_ref_pen_side == LEFT ) ? RIGHT : LEFT;
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (stadium->players.enable[i] && stadium->players.side[i] == side && stadium->players.goalie[i]) {
                stadium->players.pos_x[i] = -stadium->penalty_ref_pen_side * (PITCH_LENGTH/2-10);
                stadium->players.pos_y[i] = 10;
            }
        }

        penalty_ref_penalty_init(stadium);
        stadium->penalty_ref_first_time = false;
    }
}

PUF_FN bool penalty_ref_penalty_check_players(Stadium *stadium, const Side side ) {
    PlayMode pm = stadium->playmode;
    int iOutsideCircle = 0;
    bool bCheck = true;
    float posGoalie_x, posGoalie_y;
    //int     iPlayerOutside = -1, iGoalieNr=-1;
    int outside_player_index = -1;
    int goalie_index = -1;

    if (pm == PM_PenaltyMiss_Left || pm == PM_PenaltyMiss_Right || pm == PM_PenaltyScore_Left || pm == PM_PenaltyScore_Right)
        return true;

    // for all players from side 'side' get the goalie pos and count how many
    // players are outside the center circle.
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;

        if ( stadium->players.side[i] == side ) {
            if (stadium->players.goalie[i]) {
                goalie_index = i;
                posGoalie_x = stadium->players.pos_x[i];
                posGoalie_y = stadium->players.pos_y[i];
                continue;
            }

            float c_radius = KICK_OFF_CLEAR_DISTANCE - stadium->players.size[i];
            if (!(distance(0.0f, 0.0f, stadium->players.pos_x[i], stadium->players.pos_y[i]) <= c_radius)) {
                iOutsideCircle++;
                outside_player_index = i;
            }
        }
    }

    if (goalie_index == -1)
        return false;

    // if the 'side' equals the one that takes the penalty shoot out
    if (side == stadium->penalty_ref_cur_pen_taker) {
        // in case that goalie takes penalty kick
        // or taker goes into the center circle
        if (iOutsideCircle == 0) {
            if (pm == PM_PenaltySetup_Left || pm == PM_PenaltySetup_Right) {
                if (distance(stadium->players.pos_x[goalie_index], stadium->players.pos_y[goalie_index], stadium->ball_pos_x, stadium->ball_pos_y) > 2.0f)
                {
                    // bCheck = false;
                }
                else
                    outside_player_index = goalie_index;
            }
        }
        // if goalie not outside field, check fails
        else if (fabsf(posGoalie_x) < PITCH_LENGTH / 2.0f - 1.5f || fabsf(posGoalie_y) < PENALTY_AREA_WIDTH / 2.0f - 1.5f)
            bCheck = false;
        // only one should be outside the circle -> player that takes penalty
        else if (iOutsideCircle > 1)
            bCheck = false;
        // in setup, player outside circle should be close to ball
        else if ((pm == PM_PenaltySetup_Left || pm == PM_PenaltySetup_Right) && iOutsideCircle == 1)
            if (outside_player_index != -1 
                && distance(stadium->players.pos_x[outside_player_index], stadium->players.pos_y[outside_player_index], stadium->ball_pos_x, stadium->ball_pos_y) > 2.0f)
                bCheck = false;
    }
    else //other team
    {
        // goalie does not stand in front of goal line
        if (stadium->playmode != PM_PenaltyTaken_Left && stadium->playmode != PM_PenaltyTaken_Right)
            if (fabsf(posGoalie_x) < PITCH_LENGTH / 2.0f - PEN_MAX_GOALIE_DIST_X || fabsf(posGoalie_y) > GOAL_WIDTH * 0.5f)
                bCheck = false;
        // when receiving the penalty every player should be in center circle
        if (iOutsideCircle != 0)
            bCheck = false;
    }

    if (bCheck && outside_player_index != -1) {
        // if in setup and already in set -> check fails
        if ((side == LEFT && stadium->playmode == PM_PenaltySetup_Left
            && contain(stadium->penalty_ref_sLeftPenTaken, stadium->penalty_ref_sLeftPenTaken_size, 
            stadium->players.unum[outside_player_index]))
            || (side == RIGHT && stadium->playmode == PM_PenaltySetup_Right
            && contain(stadium->penalty_ref_sRightPenTaken, stadium->penalty_ref_sRightPenTaken_size, 
            stadium->players.unum[outside_player_index])))
            bCheck = false;
    }

    return bCheck;
}

PUF_FN int penalty_ref_getCandidateTaker(Stadium *stadium) {
    const int *sPenTaken = stadium->penalty_ref_cur_pen_taker == LEFT
        ? stadium->penalty_ref_sLeftPenTaken
        : stadium->penalty_ref_sRightPenTaken;

    const int sPenTaken_size = stadium->penalty_ref_cur_pen_taker == LEFT
        ? stadium->penalty_ref_sLeftPenTaken_size
        : stadium->penalty_ref_sRightPenTaken_size;

    int candidate_index = -1;
    int goalie_index = -1;
    float min_dist2 = FLT_MAX;

    // first find the closest player to the ball
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;
        if (stadium->players.side[i] != stadium->penalty_ref_cur_pen_taker) 
            continue;

        if (contain(sPenTaken, sPenTaken_size, stadium->players.unum[i]))
            // players that have already taken a kick cannot be
            // counted as a potential kicker.
            continue;

        if (stadium->players.goalie[i]) {
            goalie_index = i;
            continue;
        }

        float d2 = distance2(stadium->players.pos_x[i], stadium->players.pos_y[i], stadium->ball_pos_x, stadium->ball_pos_y);
        if(d2 < min_dist2) {
            min_dist2 = d2;
            candidate_index = i;
        }
    }

    if (candidate_index == -1)
        return goalie_index;

    return candidate_index;
}

PUF_FN void penalty_ref_placeTakerTeamPlayers(Stadium *stadium) {
    const bool bPenTaken = ( stadium->playmode == PM_PenaltyTaken_Right
                             || stadium->playmode == PM_PenaltyTaken_Left );

    const int taker_index = stadium->penalty_ref_last_taker_index != -1 
        ? stadium->penalty_ref_last_taker_index 
        : penalty_ref_getCandidateTaker(stadium) ;

    const float goalie_wait_pos_x = -stadium->penalty_ref_pen_side * ( PITCH_LENGTH / 2.0f + 2.0f );
    const float goalie_wait_pos_b_y = +25.0f;
    const float goalie_wait_pos_t_y = -25.0f;

    // then replace the players from the specified side
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;

        if ( stadium->players.side[i] != stadium->penalty_ref_cur_pen_taker ) continue;

        if ( i == taker_index )
        {
            if ( ! bPenTaken
                 && distance(stadium->players.pos_x[taker_index], stadium->players.pos_y[taker_index], stadium->ball_pos_x, stadium->ball_pos_y) > 2.0f )
            {
                float c_radius = 2.0f;
                nearestEdgeCircle(stadium->ball_pos_x, stadium->ball_pos_y, c_radius, stadium->players.pos_x[taker_index], stadium->players.pos_y[taker_index], &stadium->players.pos_x[i], &stadium->players.pos_y[i]);
            }
        }
        else
        {
            if ( stadium->players.goalie[i] )
            {
                float c_center_y = stadium->players.pos_y[i] > 0.0f ? goalie_wait_pos_b_y : goalie_wait_pos_t_y;
                float c_radius = 2.0f;
                if (!(distance(goalie_wait_pos_x, c_center_y, stadium->players.pos_x[i], stadium->players.pos_y[i]) <= c_radius))
                    nearestEdgeCircle(goalie_wait_pos_x, c_center_y, c_radius, stadium->players.pos_x[i], stadium->players.pos_y[i], &stadium->players.pos_x[i], &stadium->players.pos_y[i]);
            }
            else // not goalie
            {
                float c_radius = KICK_OFF_CLEAR_DISTANCE - stadium->players.size[i];
                if (!(distance(0.0f, 0.0f, stadium->players.pos_x[i], stadium->players.pos_y[i]) <= c_radius) )
                    nearestEdgeCircle(0.0f, 0.0f, c_radius, stadium->players.pos_x[i], stadium->players.pos_y[i], &stadium->players.pos_x[i], &stadium->players.pos_y[i]);
            }
        }
    }
}

PUF_FN void penalty_ref_placeOtherTeamPlayers(Stadium *stadium) {
    const bool bPenTaken = ( stadium->playmode == PM_PenaltyTaken_Right
                             || stadium->playmode == PM_PenaltyTaken_Left );
    const double goalie_line
        = ( stadium->penalty_ref_pen_side == LEFT
            ? - PITCH_LENGTH/2.0f + PEN_MAX_GOALIE_DIST_X
            : + PITCH_LENGTH/2.0f - PEN_MAX_GOALIE_DIST_X );

    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;

        if ( stadium->players.side[i] == stadium->penalty_ref_cur_pen_taker ) continue;

        // only move goalie in case the penalty has not been started yet.
        if ( stadium->players.goalie[i] )
        {
            if ( ! bPenTaken )
            {
                if ( stadium->penalty_ref_pen_side == LEFT )
                {
                    if ( stadium->players.pos_x[i] - goalie_line > 0.0f)
                    {
                        stadium->players.pos_x[i] = goalie_line - 1.5f;
                        stadium->players.pos_y[i] =  0.0f;
                    }
                }
                else
                {
                    if ( stadium->players.pos_x[i] - goalie_line < 0.0f )
                    {
                        stadium->players.pos_x[i] = goalie_line + 1.5f;
                        stadium->players.pos_y[i] =  0.0f;
                    }
                }
            }
        }
        else // not goalie
        {
            float c_radius = KICK_OFF_CLEAR_DISTANCE - stadium->players.size[i];
            
            if (!(distance(0.0f, 0.0f, stadium->players.pos_x[i], stadium->players.pos_y[i]) <= c_radius))
            {
                // place other players in circle in penalty area
                //p->moveTo( PVector::fromPolar( 6.5, Deg2Rad( i*15 ) ) );
                nearestEdgeCircle(0.0f, 0.0f, c_radius, stadium->players.pos_x[i], stadium->players.pos_y[i], &stadium->players.pos_x[i], &stadium->players.pos_y[i]);
            }
        }
    }
}

PUF_FN void penalty_ref_penalty_place_all_players(Stadium *stadium, const Side side ) {
    if (side == stadium->penalty_ref_cur_pen_taker)
        penalty_ref_placeTakerTeamPlayers(stadium);
    else // other team
        penalty_ref_placeOtherTeamPlayers(stadium);
}

PUF_FN void penalty_ref_penalty_check_score(Stadium *stadium) {
    // if both players have taken nr_kicks and max_extra_kicks penalties -> quit
    if (stadium->penalty_ref_pen_nr_taken > 2 * (PEN_MAX_EXTRA_KICKS + PEN_NR_KICKS))
    {
        if (PEN_RANDOM_WINNER)
        {
            if ( drand(&stadium->seed, 0.0f, 1.0f) < 0.5f)
            {
                // mydiff
                // M_stadium.sendRefereeAudio( "penalty_winner_l" );
                penaltyWinner(stadium, LEFT );
                // std::cerr << "Left team has won the coin toss!" << std::endl;
            }
            else
            {
                // mydiff
                // M_stadium.sendRefereeAudio( "penalty_winner_r" );
                penaltyWinner(stadium, RIGHT );
                // std::cerr << "Right team has won the coin toss!" << std::endl;
            }
        }
        else
        {
            // mydiff
            // M_stadium.sendRefereeAudio( "penalty_draw" );
        }
        //M_stadium.changePlayMode( PM_TimeOver );
        stadium->penalty_ref_timeover = true;
    }
    // if both players have taken more than nr_kicks penalties -> check for winner
    else if ( stadium->penalty_ref_pen_nr_taken > 2 * PEN_NR_KICKS)
    {
        if (stadium->penalty_ref_pen_nr_taken % 2 == 0 && stadium->team_left_pen_point != stadium->team_right_pen_point)
        {
            // mydiff
            // if ( M_stadium.teamLeft().penaltyPoint()
            //      > M_stadium.teamRight().penaltyPoint() )
            // {
            //     M_stadium.sendRefereeAudio( "penalty_winner_l" );
            // }
            // else
            // {
            //     M_stadium.sendRefereeAudio( "penalty_winner_r" );
            // }
            //M_stadium.changePlayMode( PM_TimeOver );
            stadium->penalty_ref_timeover = true;
        }
    }
    // during normal kicks, check whether one team cannot win anymore
    else
    {
        // first calculate how many penalty kick sessions are left
        // and add this to the current number of points of both teams
        // finally, subtract 1 point from the team that has already shot this turn
        int iPenLeft = PEN_NR_KICKS - stadium->penalty_ref_pen_nr_taken/2;
        int iMaxExtraLeft  = stadium->team_left_pen_point + iPenLeft;
        int iMaxExtraRight = stadium->team_right_pen_point + iPenLeft;
        if ( stadium->penalty_ref_pen_nr_taken % 2 == 1 )
        {
            if ( stadium->penalty_ref_cur_pen_taker == LEFT )
            {
                iMaxExtraLeft--;
            }
            else if ( stadium->penalty_ref_cur_pen_taker == RIGHT )
            {
                iMaxExtraRight--;
            }
        }

        if ( iMaxExtraLeft <stadium->team_right_pen_point )
        {
            // mydiff
            // M_stadium.sendRefereeAudio( "penalty_winner_r" );
            changePlayMode(stadium, PM_TimeOver );
        }
        else if ( iMaxExtraRight < stadium->team_left_pen_point)
        {
            // mydiff
            // M_stadium.sendRefereeAudio( "penalty_winner_l" );

            //M_stadium.changePlayMode( PM_TimeOver );
            stadium->penalty_ref_timeover = true;
        }
    }
}

PUF_FN void penalty_ref_penalty_score(Stadium *stadium, Side side ) {
    changePlayMode(stadium, side == RIGHT
                              ? PM_PenaltyScore_Right
                              : PM_PenaltyScore_Left );

    if ( side == RIGHT )
    {
        penaltyScore(stadium, RIGHT, true );
    }
    else
    {
        penaltyScore(stadium, LEFT, true );
    }
    stadium->penalty_ref_pen_nr_taken++;
    penalty_ref_penalty_check_score(stadium);
}

PUF_FN void penalty_ref_penalty_miss(Stadium *stadium, Side side ) {
    changePlayMode(stadium, side == LEFT ? PM_PenaltyMiss_Left : PM_PenaltyMiss_Right);
    stadium->penalty_ref_pen_nr_taken++;

    if ( side == RIGHT )
    {
        penaltyScore(stadium, RIGHT, false );
    }
    else
    {
        penaltyScore(stadium, LEFT, false );
    }

    penalty_ref_penalty_check_score(stadium);
}

PUF_FN void penalty_ref_penalty_foul(Stadium *stadium, const Side side ) {
    // mydiff
    // M_stadium.sendRefereeAudio( ( side == LEFT
    //                               ?  "penalty_foul_l"
    //                               : "penalty_foul_r" ) );

    // if team takes penalty and makes mistake -> miss, otherwise -> score
    if ( side == LEFT && stadium->penalty_ref_cur_pen_taker == LEFT )
    {
        penalty_ref_penalty_miss(stadium, LEFT );
    }
    else if ( side == RIGHT && stadium->penalty_ref_cur_pen_taker == RIGHT )
    {
        penalty_ref_penalty_miss(stadium, RIGHT );
    }
    else if ( side == LEFT )
    {
        penalty_ref_penalty_score(stadium, RIGHT );
    }
    else
    {
        penalty_ref_penalty_score(stadium, LEFT );
    }
}

PUF_FN void penalty_ref_handleTimeout(Stadium *stadium, bool left_move_check, bool right_move_check) {
    const PlayMode pm = stadium->playmode;

    // when setup has finished and still players are positioned incorrectly
    // replace them and go to ready mode.
    if (PEN_COACH_MOVES_PLAYERS
         && ( pm == PM_PenaltySetup_Left
              || pm == PM_PenaltySetup_Right )
         )
    {
        if ( ! left_move_check )
        {
            penalty_ref_penalty_place_all_players(stadium, LEFT );
        }

        if ( ! right_move_check )
        {
            penalty_ref_penalty_place_all_players(stadium, RIGHT );
        }

        left_move_check = right_move_check = true;
    }


    if ( pm == PM_PenaltyMiss_Left
         || pm == PM_PenaltyScore_Left
         || pm == PM_PenaltyMiss_Right
         || pm == PM_PenaltyScore_Right )
    {
        penalty_ref_penalty_init(stadium);
    }
    else if ( left_move_check
              && right_move_check )
    {
        if ( pm == PM_PenaltySetup_Left )
        {
            changePlayMode(stadium, PM_PenaltyReady_Left );
        }
        else if ( pm == PM_PenaltySetup_Right )
        {
            changePlayMode(stadium, PM_PenaltyReady_Right );
        }
        // time elapsed -> missed goal
        else if ( pm == PM_PenaltyTaken_Left
                  || pm == PM_PenaltyReady_Left )
        {
            penalty_ref_penalty_miss(stadium, LEFT );
        }
        else if ( pm == PM_PenaltyTaken_Right
                  || pm == PM_PenaltyReady_Right )
        {
            penalty_ref_penalty_miss(stadium, RIGHT );
        }
    }
    // if incorrect positioned , place them correctly
    else if ( stadium->penalty_ref_cur_pen_taker == LEFT )
    {
        penalty_ref_penalty_foul(stadium, ( left_move_check == false ) ? LEFT : RIGHT );
    }
    else if ( stadium->penalty_ref_cur_pen_taker == RIGHT )
    {
        penalty_ref_penalty_foul(stadium, ( right_move_check == false ) ? RIGHT : LEFT );
    }
}

PUF_FN void penalty_ref_handleTimer(Stadium *stadium, const bool left_move_check, const bool right_move_check) {
    const PlayMode pm = stadium->playmode;

    --stadium->penalty_ref_timer;

    if ( pm == PM_PenaltyScore_Left
         || pm == PM_PenaltyScore_Right
         || pm == PM_PenaltyMiss_Left
         || pm == PM_PenaltyMiss_Right )
    {
        // freeze the ball
        //         M_stadium.ball().moveTo( M_stadium.ball().pos(),
        //                                 //0.0,
        //                                 PVector( 0.0, 0.0 ),
        //                                 PVector( 0.0, 0.0 ) );
        placeBall(stadium, stadium->penalty_ref_cur_pen_taker, stadium->ball_pos_x, stadium->ball_pos_y);

        return;
    }

    if ( left_move_check
         && right_move_check )
    {
        // if ball crossed goalline, process goal and set ball on goalline
        if (ref_crossGoalLine(stadium, stadium->penalty_ref_pen_side, stadium->penalty_ref_prev_ball_pos_x, stadium->penalty_ref_prev_ball_pos_y))
        {
            if ( pm == PM_PenaltyTaken_Left )
            {
                penalty_ref_penalty_score(stadium, LEFT );
            }
            else if ( pm == PM_PenaltyTaken_Right )
            {
                penalty_ref_penalty_score(stadium, RIGHT );
            }
            // freeze the ball at the current position.
            placeBall(stadium, stadium->penalty_ref_pen_side, stadium->ball_pos_x, stadium->ball_pos_y);
        }
        else if ( fabs( stadium->ball_pos_x )
                  > PITCH_LENGTH * 0.5
                  + BALL_SIZE
                  || fabs( stadium->ball_pos_y )
                  > PITCH_WIDTH * 0.5
                  + BALL_SIZE)
        {
            placeBall(stadium, stadium->penalty_ref_pen_side, stadium->ball_pos_x, stadium->ball_pos_y);
            if ( pm == PM_PenaltyTaken_Left )
            {
                penalty_ref_penalty_miss(stadium, LEFT );
            }
            else if ( pm == PM_PenaltyTaken_Right )
            {
                penalty_ref_penalty_miss(stadium, RIGHT );
            }
        }
    }
    // if someone makes foul and we are not in setup -> replace the players
    else if ( pm == PM_PenaltyReady_Left
              || pm == PM_PenaltyReady_Right
              || pm == PM_PenaltyTaken_Left
              || pm == PM_PenaltyTaken_Right )
    {
        if (PEN_COACH_MOVES_PLAYERS)
        {
            if ( left_move_check == false )
            {
                penalty_ref_penalty_place_all_players(stadium, LEFT );
            }
            if ( right_move_check == false )
            {
                penalty_ref_penalty_place_all_players(stadium, RIGHT );
            }
        }
        else if ( stadium->penalty_ref_cur_pen_taker == LEFT )
        {
            penalty_ref_penalty_foul(stadium, ( left_move_check == false ) ? LEFT : RIGHT );
        }
        else if ( stadium->penalty_ref_cur_pen_taker == RIGHT )
        {
            penalty_ref_penalty_foul(stadium, ( right_move_check == false ) ? RIGHT : LEFT );
        }
    }
}

PUF_FN void penalty_ref_analyse(Stadium *stadium) {
    penalty_ref_startPenaltyShootout(stadium);

    if ( ! ref_isPenaltyShootOut( stadium->playmode, NEUTRAL) )
        return;

    if ( stadium->penalty_ref_timeover )
    {
        changePlayMode(stadium, PM_TimeOver );
        return;
    }

    const PlayMode pm = stadium->playmode;

    bool bCheckLeft  = penalty_ref_penalty_check_players(stadium, LEFT );
    bool bCheckRight = penalty_ref_penalty_check_players(stadium, RIGHT );

    // if ready or taken make sure all players keep well-positioned
    if (PEN_COACH_MOVES_PLAYERS
        && (pm == PM_PenaltyReady_Left || pm == PM_PenaltyReady_Right
        || pm == PM_PenaltyTaken_Left || pm == PM_PenaltyTaken_Right )) {
        if ( ! bCheckLeft )
            penalty_ref_penalty_place_all_players(stadium, LEFT );

        if ( ! bCheckRight ) 
            penalty_ref_penalty_place_all_players(stadium, RIGHT );

        bCheckLeft = bCheckRight = true;
    }

    if ( stadium->penalty_ref_timer < 0 )
    {
        // std::cerr << "(PenaltyRef::analyse) timer cannot be negative?" << std::endl;
    }
    else if ( stadium->penalty_ref_timer == 0 )
        penalty_ref_handleTimeout(stadium, bCheckLeft, bCheckRight );
    else // M_timer > 0
        penalty_ref_handleTimer(stadium, bCheckLeft, bCheckRight );

    stadium->penalty_ref_prev_ball_pos_x = stadium->ball_pos_x;
    stadium->penalty_ref_prev_ball_pos_y = stadium->ball_pos_y;
}

PUF_FN void penalty_ref_kickTaken(Stadium *stadium, int kicker_index, const float accel_r) {
    if (!ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    // if in setup it is not allowed to kick the ball
    if (stadium->playmode == PM_PenaltySetup_Left || stadium->playmode == PM_PenaltySetup_Right)
        penalty_ref_penalty_foul(stadium, stadium->players.side[kicker_index]);
    // cannot kick second time after penalty was taken
    else if (!PEN_ALLOW_MULTI_KICKS
        && (stadium->playmode == PM_PenaltyTaken_Left
        || stadium->playmode == PM_PenaltyTaken_Right)
        && stadium->players.side[kicker_index] == stadium->penalty_ref_cur_pen_taker)
        penalty_ref_penalty_foul(stadium, stadium->penalty_ref_cur_pen_taker);
    else if (stadium->playmode == PM_PenaltyReady_Left
        || stadium->playmode == PM_PenaltyTaken_Left
        || stadium->playmode == PM_PenaltyReady_Right
        || stadium->playmode == PM_PenaltyTaken_Right) {
        if ((stadium->playmode == PM_PenaltyReady_Left
            || stadium->playmode == PM_PenaltyReady_Right)
            && stadium->players.side[kicker_index] == stadium->penalty_ref_cur_pen_taker
            && ((LEFT == stadium->penalty_ref_cur_pen_taker
            && contain(stadium->penalty_ref_sLeftPenTaken, stadium->penalty_ref_sLeftPenTaken_size, stadium->players.unum[kicker_index]))
            || (RIGHT == stadium->penalty_ref_cur_pen_taker
            && contain(stadium->penalty_ref_sRightPenTaken, stadium->penalty_ref_sLeftPenTaken_size, stadium->players.unum[kicker_index]))))
            // this kicker has already taken the kick
            penalty_ref_penalty_foul(stadium, stadium->penalty_ref_cur_pen_taker);
        else if (stadium->penalty_ref_last_taker_index != -1
            && stadium->players.side[stadium->penalty_ref_last_taker_index] == stadium->penalty_ref_cur_pen_taker
            && kicker_index != stadium->penalty_ref_last_taker_index)
            // not a taker player in the same team must not kick the ball.
            penalty_ref_penalty_foul(stadium, stadium->penalty_ref_cur_pen_taker);
        else if (stadium->players.side[kicker_index] != stadium->penalty_ref_cur_pen_taker && !stadium->players.goalie[kicker_index])
            // field player in the defending team must not kick the ball.
            penalty_ref_penalty_foul(stadium, (Side)(-stadium->penalty_ref_cur_pen_taker));
        else
            stadium->penalty_ref_last_taker_index = kicker_index;
    }

    // if we were ready for penalty -> change play mode
    if (stadium->playmode == PM_PenaltyReady_Left) {
        // when penalty is taken, add player, multiple copies are deleted

        stadium->penalty_ref_sLeftPenTaken[stadium->penalty_ref_sLeftPenTaken_size++] = stadium->players.unum[kicker_index];
        if (stadium->penalty_ref_sLeftPenTaken_size == NUM_PLAYERS)
            stadium->penalty_ref_sLeftPenTaken_size = 0;
        changePlayMode(stadium, PM_PenaltyTaken_Left);
    }
    else if (stadium->playmode == PM_PenaltyReady_Right ) {
        stadium->penalty_ref_sRightPenTaken[stadium->penalty_ref_sRightPenTaken_size++] = stadium->players.unum[kicker_index];
        if (stadium->penalty_ref_sRightPenTaken_size == NUM_PLAYERS)
            stadium->penalty_ref_sRightPenTaken_size = 0;
        changePlayMode(stadium, PM_PenaltyTaken_Right);
    }
    // if it was not allowed to kick, don't move ball
    else if (stadium->playmode != PM_PenaltyTaken_Left && stadium->playmode != PM_PenaltyTaken_Right)
        placeBall(stadium, stadium->penalty_ref_pen_side, stadium->ball_pos_x, stadium->ball_pos_y);
}

PUF_FN void penalty_ref_tackleTaken(Stadium *stadium, int tackler_index, const double accel_r, const bool foul) {
    if (!ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    const PlayMode pm = stadium->playmode;

    if (pm == PM_PenaltyMiss_Left
        || pm == PM_PenaltyScore_Left
        || pm == PM_PenaltyMiss_Right
        || pm == PM_PenaltyScore_Right)
	    return;

    bool detect_charge = false;
    bool detect_yellow = false;
    bool detect_red = false;

    ref_checkFoul(stadium, tackler_index, foul, &detect_charge, &detect_yellow, &detect_red);

    if ( detect_charge
         || detect_yellow
         || detect_red )
    {
        if ( stadium->players.side[tackler_index] == stadium->penalty_ref_cur_pen_taker )
        {
            penalty_ref_penalty_foul(stadium, stadium->penalty_ref_cur_pen_taker);
        }
        else
        {
            penalty_ref_penalty_foul(stadium, (Side)(-stadium->penalty_ref_cur_pen_taker));
        }
    }
    else
    {
        penalty_ref_kickTaken(stadium, tackler_index, accel_r);
    }
}

#endif

#if REFEREES_ENABLED

PUF_FN void ref_playModeChange(Stadium *stadium, PlayMode pm) {
#if OFFSIDE_REFEREE_ENABLED
    offside_ref_playModeChange(stadium, pm);
#endif
#if FREE_KICK_REFEREE_ENABLED
    free_kick_ref_playModeChange(stadium, pm);
#endif
#if TOUCH_REFEREE_ENABLED
    touch_ref_playModeChange(stadium, pm);
#endif
#if CATCH_REFEREE_ENABLED
    catch_ref_playModeChange(stadium, pm);
#endif
#if FOUL_REFEREE_ENABLED
    foul_ref_playModeChange(stadium, pm);
#endif
#if PENALTY_REFEREE_ENABLED
    penalty_ref_playModeChange(stadium, pm);
#endif
}

PUF_FN void ref_analyse(Stadium *stadium) {
#if TIME_REFEREE_ENABLED
    time_ref_analyse(stadium);
#endif
#if BALL_STUCK_REFEREE_ENABLED
    ball_stuck_ref_analyse(stadium);
#endif
#if OFFSIDE_REFEREE_ENABLED
    offside_ref_analyse(stadium);
#endif
#if FREE_KICK_REFEREE_ENABLED
    free_kick_ref_analyse(stadium);
#endif
#if TOUCH_REFEREE_ENABLED
    touch_ref_analyse(stadium);
#endif
#if CATCH_REFEREE_ENABLED
    catch_ref_analyse(stadium);
#endif
#if FOUL_REFEREE_ENABLED
    foul_ref_analyse(stadium);
#endif
    // mydiff : illegal defense is disabled by defualt. need to check and see if it's also disabled in the tournaments.
    // illegal_defense_ref_analyse(stadium);
    // keepaway_ref_analyse(stadium);
#if PENALTY_REFEREE_ENABLED
    penalty_ref_analyse(stadium);
#endif

}

PUF_FN void ref_ballTouched(Stadium *stadium, int player_index) {
#if OFFSIDE_REFEREE_ENABLED
    offside_ref_ballTouched(stadium, player_index);
#endif
#if FREE_KICK_REFEREE_ENABLED
    free_kick_ref_ballTouched(stadium, player_index);
#endif
#if TOUCH_REFEREE_ENABLED
    touch_ref_ballTouched(stadium, player_index);
#endif
#if CATCH_REFEREE_ENABLED
    catch_ref_ballTouched(stadium, player_index);
#endif
}

PUF_FN void ref_kickTaken(Stadium *stadium, int kicker_index, const float accel_r) {
#if OFFSIDE_REFEREE_ENABLED
    offside_ref_kickTaken(stadium, kicker_index, accel_r);
#endif
#if FREE_KICK_REFEREE_ENABLED
    free_kick_ref_kickTaken(stadium, kicker_index, accel_r);
#endif
#if TOUCH_REFEREE_ENABLED
    touch_ref_kickTaken(stadium, kicker_index, accel_r);
#endif
#if CATCH_REFEREE_ENABLED
    catch_ref_kickTaken(stadium, kicker_index, accel_r);
#endif
#if PENALTY_REFEREE_ENABLED
    penalty_ref_kickTaken(stadium, kicker_index, accel_r);
#endif
}

PUF_FN void ref_failedKickTaken(Stadium *stadium, int kicker_index) {
#if OFFSIDE_REFEREE_ENABLED
    offside_ref_failedKickTaken(stadium, kicker_index);
#endif
}

PUF_FN void ref_tackleTaken(Stadium *stadium, int tackler_index, const float accel_r, const bool foul) {
#if OFFSIDE_REFEREE_ENABLED
    offside_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
#endif
#if FREE_KICK_REFEREE_ENABLED
    free_kick_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
#endif
#if TOUCH_REFEREE_ENABLED
    touch_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
#endif
#if CATCH_REFEREE_ENABLED
    catch_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
#endif
#if FOUL_REFEREE_ENABLED
    foul_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
#endif
#if PENALTY_REFEREE_ENABLED
    penalty_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
#endif
}

PUF_FN void ref_failedTackleTaken(Stadium *stadium, int tackler_index, const bool foul) {
#if OFFSIDE_REFEREE_ENABLED
    offside_ref_failedTackleTaken(stadium, tackler_index, foul);
#endif
}

#endif

PUF_FN void turnMovableObjects(Stadium *stadium, int players_size) {
    for (int i = 0 ; i < players_size ; i++) {
        stadium->players.angle_body_committed[i] = stadium->players.angle_body[i];
        stadium->players.angle_neck_committed[i] = stadium->players.angle_neck[i];
        stadium->players.vel_x[i] = 0.0f;
        stadium->players.vel_y[i] = 0.0f;
        stadium->players.accel_x[i] = 0.0f;
        stadium->players.accel_y[i] = 0.0f;
    }
}

PUF_FN static inline bool intersect(float begin_x, float begin_y, float end_x, float end_y, 
    float circle_center_x, float circle_center_y, float circle_radius, float *inter_x, float *inter_y) {
    if (begin_x == end_x && begin_y == end_y)
        return false;

    float begin_to_end_x = begin_x - end_x, begin_to_end_y = begin_y - end_y;
    float begin_to_circle_center_x = begin_x - circle_center_x, begin_to_circle_center_y = begin_y - circle_center_y;
    float end_to_circle_center_x = end_x - circle_center_x, end_to_circle_center_y = end_y - circle_center_y;
    if (r(begin_to_end_x, begin_to_end_y) < r(begin_to_circle_center_x, begin_to_circle_center_y) - circle_radius)
        // object wont get within circles range
        return false;

    if (circle_center_x == 0.0f && circle_center_y == 0.0f) {
        float dx = end_x - begin_x;
        float dy = end_y - begin_y;
        float dr = sqrtf(dx * dx + dy * dy);
        float D = begin_x * end_y - end_x * begin_y;
        float descrim = circle_radius * circle_radius * dr * dr - D * D;
        if (descrim <= 0.0f)
            // no collision of tagent
            return false;
        else {
            descrim = sqrtf(descrim);

            float x1 = (D * dy + dx * descrim) / (dr * dr);
            float x2 = (D * dy - dx * descrim) / (dr * dr);
            float y1 = (-D * dx + fabsf(dy) * descrim) / (dr * dr);
            float y2 = (-D * dx - fabsf(dy) * descrim) / (dr * dr);
            float first_x, first_y, second_x, second_y;
            if (dy < 0.0f) {
                first_x = x2;
                first_y = y1;
                second_x = x1;
                second_y = y2;
            }
            else {
                first_x = x1;
                first_y = y1;
                second_x = x2;
                second_y = y2;
            }

            if (!between(first_x, first_y, begin_x, begin_y, end_x, end_y) && !between(second_x, second_y, begin_x, begin_y, end_x, end_y)) 
                // intersections are not between the end points
                //                  std::cout << "Coll outside of end points\n"
                return false;

            if (!between(first_x, first_y, begin_x, begin_y, end_x, end_y)) {
                *inter_x = second_x;
                *inter_y = second_y;
                second_x = first_x;
                second_y = first_y;
            }
            else if (!between(second_x, second_y, begin_x, begin_y, end_x, end_y)) {
                *inter_x = first_x;
                *inter_y = first_y;
            }
            else {
                float begin_to_first_x = begin_x - first_x, begin_to_first_y = begin_y - first_y;
                float begin_to_second_x = begin_x - second_x, begin_to_second_y = begin_y - second_y;
                if (r(begin_to_first_x, begin_to_first_y) < r(begin_to_second_x, begin_to_second_y)) {
                    *inter_x = first_x;
                    *inter_y = first_y;
                }
                else {
                    *inter_x = second_x;
                    *inter_y = second_y;
                    second_x = first_x;
                    second_y = first_y;
                }
            }

            if (*inter_x == begin_x && *inter_y == begin_y && !between(second_x, second_y, begin_x, begin_y, end_x, end_y))
                // fake collision.  Object is tagent to the circle and moving away
                return false;
            return true;
        }
    }
    else {
        if (intersect(begin_to_circle_center_x, begin_to_circle_center_y, end_to_circle_center_x, end_to_circle_center_y, 
            0.0f, 0.0f, circle_radius, inter_x, inter_y)) {
            *inter_x += circle_center_x;
            *inter_y += circle_center_y;
            return true;
        }
        return false;
    }
}

PUF_FN void apply_accels(Stadium *stadium, float *vel_x, float *vel_y, float accel_x, float accel_y, float max_speed, float max_accel, float randp) {
    if (accel_x || accel_y) {
        float a2 = r2(accel_x, accel_y);
        float max_a2 = max_accel * max_accel;
        if (a2 > max_a2) {
            float tmp = sqrtf(a2);
            accel_x *= (max_accel / tmp);
            accel_y *= (max_accel / tmp);
        }

        *vel_x += accel_x;
        *vel_y += accel_y;
        float v2 = r2(*vel_x, *vel_y);
        float max_v2 = max_speed * max_speed;
        if (v2 > max_v2) {
            float tmp = sqrtf(v2);
            *vel_x *= (max_speed / tmp);
            *vel_y *= (max_speed / tmp);
        }
    }

    float vel_noise_x, vel_noise_y;
    noise(randp, *vel_x, *vel_y, &stadium->seed, &vel_noise_x, &vel_noise_y);
    *vel_x += vel_noise_x;
    *vel_y += vel_noise_y;
    // note: wind is disabled by default in server params
}

PUF_FN void inc(Stadium *stadium, float *pos_x, float *pos_y, float *vel_x, float *vel_y, 
    float max_speed, float max_accel, float size, float decay, float randp, unsigned int *seed, int player_index) {
    float post_center_x, post_center_y;
    float post_radius;
    nearestPost(*pos_x, *pos_y, size, &post_center_x, &post_center_y, &post_radius);

    float pos_to_post_x = *pos_x - post_center_x, pos_to_post_y = *pos_y - post_center_y;
    float post_radius2 = post_radius * post_radius;
    while (r2(pos_to_post_x, pos_to_post_y) < post_radius2) {
        // then the ball has overlapped the post.  Either it was moved
        // there or "pushed".  Either way, we just move the ball away
        // from the post
        float diff_x = pos_to_post_x, diff_y = pos_to_post_y;
        if ( diff_x == 0.0f && diff_y == 0.0f )
            from_polar(post_radius, drand(seed, -M_PIf, +M_PIf), &diff_x, &diff_y);
        else
            normalize(&diff_x, &diff_y, post_radius);

        *pos_x = post_center_x + diff_x;
        *pos_y = post_center_y + diff_y;

        pos_to_post_x = *pos_x - post_center_x;
        pos_to_post_y = *pos_y - post_center_y;
        while (r2(pos_to_post_x, pos_to_post_y) < post_radius2) {
            // noise keeps it inside the post, move it a bit further out
            normalize(&diff_x, &diff_y, r(diff_x, diff_y) * 1.01f );
            *pos_x = post_center_x + diff_x;
            *pos_y = post_center_y + diff_y;
            pos_to_post_x = *pos_x - post_center_x;
            pos_to_post_y = *pos_y - post_center_y;
        }

        if (*vel_x != 0.0 || *vel_y != 0.0) {
            float pos2center_x = post_center_x - *pos_x, pos2center_y = post_center_y - *pos_y;
            float angle = th(pos2center_x, pos2center_y);
            rotate(vel_x, vel_y, -angle);
            *vel_x = -*vel_x;
            rotate(vel_x, vel_y, angle);
        }

        nearestPost(*pos_x, *pos_y, size, &post_center_x, &post_center_y, &post_radius);
        post_radius2 = post_radius * post_radius;
        if (player_index != -1)
            collidedWithPost(stadium, player_index);
        pos_to_post_x = *pos_x - post_center_x;
        pos_to_post_y = *pos_y - post_center_y;
    }

    float new_pos_x = *pos_x + *vel_x, new_pos_y = *pos_y + *vel_y;
    float second_post_center_x, second_post_center_y;
    float second_post_radius;
    nearestPost(new_pos_x, new_pos_y, size, &second_post_center_x, &second_post_center_y, &second_post_radius);
    float inter_x, inter_y;
    bool second = false;

    while ((*pos_x != new_pos_x || *pos_y != new_pos_y)
        && (intersect(*pos_x, *pos_y, new_pos_x, new_pos_y, post_center_x, post_center_y, post_radius, &inter_x, &inter_y) 
        || ((post_center_x != second_post_center_x || post_center_y != second_post_center_y || post_radius != second_post_radius) 
        && (second = intersect(*pos_x, *pos_y, new_pos_x, new_pos_y, second_post_center_x, second_post_center_y, second_post_radius, &inter_x, &inter_y))))) {
        // handle collision
        *pos_x = inter_x;
        *pos_y = inter_y;

        float rem_x = new_pos_x - *pos_x, rem_y = new_pos_y - *pos_y;
        float coll_2_circle_x, coll_2_circle_y;
        if (second) {
            coll_2_circle_x = second_post_center_x - *pos_x;
            coll_2_circle_y = second_post_center_y - *pos_y;
        }
        else {
            coll_2_circle_x = post_center_x - *pos_x;
            coll_2_circle_y = post_center_y - *pos_y;
        }

        // 2008-05-22 akiyama
        // fixed endless-loop bug.
        // If this small vector is not added to M_pos, intersect() may still
        // return pos() as the intersect point.
        float temp_x, temp_y;
        from_polar(1.0e-4f, th(coll_2_circle_x, coll_2_circle_y) + 180.0f, &temp_x, &temp_y);
        *pos_x += temp_x;
        *pos_y += temp_y;

        float angle = th(coll_2_circle_x, coll_2_circle_y);
        rotate(&rem_x, &rem_y, -angle);
        rem_x = -rem_x;
        rotate(&rem_x, &rem_y, angle);

        new_pos_x = *pos_x + rem_x;
        new_pos_y = *pos_y + rem_y;

        // setup post and second post for next loop
        nearestPost(*pos_x, *pos_y, size, &post_center_x, &post_center_y, &post_radius);
        nearestPost(new_pos_x, new_pos_y, size, &second_post_center_x, &second_post_center_y, &second_post_radius);

        // setup vel so it will decay normally.  The collisions are
        // elastic, so the maginitude does not change, but the heading
        // does
        float temp2_x, temp2_y;
        from_polar(r(*vel_x, *vel_y), th(rem_x, rem_y), &temp2_x, &temp2_y);
        *vel_x = temp2_x;
        *vel_y = temp2_y;
        second = false;
        if (player_index != -1)
            collidedWithPost(stadium, player_index);
    }

    *pos_x = new_pos_x;
    *pos_y = new_pos_y;
    *vel_x *= decay;
    *vel_y *= decay;
}

PUF_FN void inc_ball(Stadium *stadium) {
    apply_accels(stadium, &stadium->ball_vel_x, &stadium->ball_vel_y, stadium->ball_accel_x, stadium->ball_accel_y, 
        stadium->ball_max_speed, stadium->ball_max_accel, stadium->ball_randp);
    inc(stadium, &stadium->ball_pos_x, &stadium->ball_pos_y, &stadium->ball_vel_x, &stadium->ball_vel_y, 
        stadium->ball_max_speed, stadium->ball_max_accel, 
        stadium->ball_size, stadium->ball_decay, stadium->ball_randp, &stadium->seed, -1);
    stadium->ball_accel_x = 0.0f;
    stadium->ball_accel_y = 0.0f;
}

PUF_FN void inc_players(Stadium *stadium, int players_size) {
    for (int i = 0 ; i < players_size ; i++) {
        if (!stadium->players.enable[i]) continue;
        apply_accels(stadium, &stadium->players.vel_x[i], &stadium->players.vel_y[i], stadium->players.accel_x[i], stadium->players.accel_y[i], 
            stadium->players.max_speed[i], stadium->players.max_accel[i], stadium->players.randp[i]);
        updateAngle_player(stadium, i);
        inc(stadium, &stadium->players.pos_x[i], &stadium->players.pos_y[i], 
            &stadium->players.vel_x[i], &stadium->players.vel_y[i], 
            stadium->players.max_speed[i], stadium->players.max_accel[i], 
            stadium->players.size[i], stadium->players.decay[i], stadium->players.randp[i], 
            &stadium->seed, i);
        stadium->players.accel_x[i] = 0.0f;
        stadium->players.accel_y[i] = 0.0f;
    }
}

PUF_FN void collide(float *post_collision_pos_x, float *post_collision_pos_y, int *collision_count, bool *collided, float col_pos_x, float col_pos_y) {
    *post_collision_pos_x += col_pos_x;
    *post_collision_pos_y += col_pos_y;
    *collision_count += 1;
    *collided = true;
}

PUF_FN void calcCollisionPos(float a_x, float a_y, float b_x, float b_y, float a_size, float b_size, 
    float *a_post_collision_pos_x, float *a_post_collision_pos_y, float *b_post_collision_pos_x, float *b_post_collision_pos_y, 
    int *a_collision_count, int *b_collision_count,  bool *a_collided, bool *b_collided, unsigned int *seed) {

    float mid_x = (a_x + b_x) / 2.0f, mid_y = (a_y + b_y) / 2.0f;
    float mid2a_x = a_x - mid_x, mid2a_y = a_y - mid_y;
    float mid2b_x = b_x - mid_x, mid2b_y = b_y - mid_y;

    /* pfr 10/25/01
        This was a really nasty bug. This used to be the condition
        if ( a->pos == b->pos )
        If a->pos and b->pos are approximately equal (but
        not ==, then a->pos - mid and b->pos - mid can both be 0.
        This means that in the else clause below, we call PVector::r(v)
        on two zero vectors, which makes them both (v,0).
        Then, the while statement below is an infinite loop */
    if ((a_x == b_x && a_y == b_y) || (r(mid2a_x, mid2a_y) < EPS && r(mid2b_x, mid2b_y) < EPS)) {
        // if the two objects are directly on top on one and other
        // then they will be separated at a random angle
        double a_ang = drand(seed, -M_PIf, M_PIf);
        double b_ang = normalize_angle(a_ang + M_PIf);
        from_polar(add_eps((a_size + b_size) / 2.0f), a_ang, &mid2a_x, &mid2a_y);
        from_polar(add_eps((a_size + b_size) / 2.0f), b_ang, &mid2b_x, &mid2b_y);
    }
    else {
        normalize(&mid2a_x, &mid2a_y, add_eps((a_size + b_size) / 2.0f));
        normalize(&mid2b_x, &mid2b_y, add_eps((a_size + b_size) / 2.0f));
    }

    float apos_x = mid_x + mid2a_x, apos_y = mid_y + mid2a_y;
    float bpos_x = mid_x + mid2b_x, bpos_y = mid_y + mid2b_y;

    // 0.01% is added to the movement, as sometimes structural noise
    // means that even though mid2a and mid2b should be
    // a->size + b->size apart, they are ever so slightly
    // less.
    int count = 0;
    const float collision_dist2 = (a_size + b_size) * (a_size + b_size);
    while (distance2(apos_x, apos_y, bpos_x, bpos_y) < collision_dist2 && count < 10) {
        normalize(&mid2a_x, &mid2a_y, r(mid2a_x, mid2a_y) * 1.0001 );
        normalize(&mid2b_x, &mid2b_y, r(mid2b_x, mid2b_y) * 1.0001 );
        apos_x = mid_x + mid2a_x;
        apos_y = mid_y + mid2a_y;
        bpos_x = mid_x + mid2b_x;
        bpos_y = mid_y + mid2b_y;
        count += 1;
    }

    collide(a_post_collision_pos_x, a_post_collision_pos_y, a_collision_count, a_collided, apos_x, apos_y);
    collide(b_post_collision_pos_x, b_post_collision_pos_y, b_collision_count, b_collided, bpos_x, bpos_y);
}

PUF_FN void calcBallCollisionPos(Stadium *stadium, int player_index) {
    if (stadium->playmode == PM_PlayOn) {
        calcCollisionPos(stadium->ball_pos_x, stadium->ball_pos_y, stadium->players.pos_x[player_index], stadium->players.pos_y[player_index], 
            stadium->ball_size, stadium->players.size[player_index], 
            &stadium->ball_post_collision_pos_x, &stadium->ball_post_collision_pos_y, 
            &stadium->players.post_collision_pos_x[player_index], &stadium->players.post_collision_pos_y[player_index],
            &stadium->ball_collision_count, &stadium->players.collision_count[player_index], 
            &stadium->ball_collided, &stadium->players.collided[player_index], &stadium->seed);
        return;
    }

    float b2p_x, b2p_y;
    if (stadium->ball_pos_x == stadium->players.pos_x[player_index] && stadium->ball_pos_y == stadium->players.pos_y[player_index] ) {
        double p_ang = drand(&stadium->seed, -M_PIf, M_PIf);
        from_polar(add_eps(stadium->ball_size + stadium->players.size[player_index]), p_ang, &b2p_x, &b2p_y);
    }
    else {
        b2p_x = stadium->players.pos_x[player_index] - stadium->ball_pos_x;
        b2p_y = stadium->players.pos_y[player_index] - stadium->ball_pos_y;
        normalize(&b2p_x, &b2p_y, add_eps(stadium->ball_size + stadium->players.size[player_index]));
    }

    collide(&stadium->ball_post_collision_pos_x, &stadium->ball_post_collision_pos_y, &stadium->ball_collision_count, 
        &stadium->ball_collided, stadium->ball_pos_x, stadium->ball_pos_y);
    collide(&stadium->players.post_collision_pos_x[player_index], &stadium->players.post_collision_pos_y[player_index], &stadium->players.collision_count[player_index], 
        &stadium->players.collided[player_index], stadium->ball_pos_x + b2p_x, stadium->ball_pos_y + b2p_y);
}

PUF_FN void moveToCollisionPos(float *pos_x, float *pos_y, float *post_collision_pos_x, float *post_collision_pos_y, int *collision_count) {
    if (*collision_count > 0) {
        *pos_x = (*post_collision_pos_x) / (*collision_count);
        *pos_y = (*post_collision_pos_y) / (*collision_count);
    }

    *post_collision_pos_x = 0.0f;
    *post_collision_pos_y = 0.0f;
    *collision_count = 0;
}

PUF_FN void updateCollisionVel(float *vel_x, float *vel_y, bool *collided) {
    if (*collided) {
        *vel_x *= 0.1f;
        *vel_y *= 0.1f;
        *collided = false;
    }
}

PUF_FN void collisions(Stadium *stadium) {
    bool col = false;
    int max_loop = 10;

    do {
        col = false;
        clearCollision(&stadium->ball_post_collision_pos_x, &stadium->ball_post_collision_pos_y, &stadium->ball_collision_count);
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;
            clearCollision(&stadium->players.post_collision_pos_x[i], &stadium->players.post_collision_pos_y[i], &stadium->players.collision_count[i]);
        }

        // check ball to player
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;
            if (i == stadium->ball_catcher_index) continue;
            float col_r = stadium->ball_size + stadium->players.size[i];
            if (distance2(stadium->ball_pos_x, stadium->ball_pos_y, stadium->players.pos_x[i], stadium->players.pos_y[i]) < col_r * col_r) {
                col = true;
                collidedWithBall(stadium, i);
                #if REFEREES_ENABLED
                ref_ballTouched(stadium, i);
                #endif
                calcBallCollisionPos(stadium, i);
            }
        }

        // check player to player
        for ( int i = 0; i < NUM_PLAYERS - 1; ++i ) {
            if (!stadium->players.enable[i]) continue;
            for ( int j = i + 1; j < NUM_PLAYERS; ++j ) {
                if (!stadium->players.enable[j]) continue;
                float col_r = stadium->players.size[i] + stadium->players.size[j];
                if (distance2(stadium->players.pos_x[i], stadium->players.pos_y[i], stadium->players.pos_x[j], stadium->players.pos_y[j]) 
                    < col_r * col_r) {
                    col = true;
                    collidedWithPlayer(stadium, i);
                    collidedWithPlayer(stadium, j);
                    calcCollisionPos(stadium->players.pos_x[i], stadium->players.pos_y[i], stadium->players.pos_x[j], stadium->players.pos_y[j], 
                        stadium->players.size[i], stadium->players.size[j], 
                        &stadium->players.post_collision_pos_x[i], &stadium->players.post_collision_pos_y[i], 
                        &stadium->players.post_collision_pos_x[j], &stadium->players.post_collision_pos_y[j],
                        &stadium->players.collision_count[i], &stadium->players.collision_count[j], 
                        &stadium->players.collided[i], &stadium->players.collided[j], &stadium->seed);
                }
            }
        }

        moveToCollisionPos(&stadium->ball_pos_x, &stadium->ball_pos_y, 
            &stadium->ball_post_collision_pos_x, &stadium->ball_post_collision_pos_y, 
            &stadium->ball_collision_count);
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            if (!stadium->players.enable[i]) continue;
            moveToCollisionPos(&stadium->players.pos_x[i], &stadium->players.pos_y[i], 
                &stadium->players.post_collision_pos_x[i], &stadium->players.post_collision_pos_y[i],
                &stadium->players.collision_count[i]);
        }

        --max_loop;
    }
    while (col && max_loop > 0);

    updateCollisionVel(&stadium->ball_vel_x, &stadium->ball_vel_y, &stadium->ball_collided);
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;
        updateCollisionVel(&stadium->players.vel_x[i], &stadium->players.vel_y[i], &stadium->players.collided[i]);
    }
}

PUF_FN void incMovableObjects(Stadium *stadium, int players_size) {
    inc_ball(stadium);
    inc_players(stadium, players_size);

    collisions(stadium);

    if (stadium->ball_catcher_index != -1) {
        // keeps the caught ball infront of the player
        float rpos_x, rpos_y;
        from_polar(stadium->players.size[stadium->ball_catcher_index] + BALL_SIZE, stadium->players.angle_body_committed[stadium->ball_catcher_index], &rpos_x, &rpos_y);
        stadium->ball_pos_x = stadium->players.pos_x[stadium->ball_catcher_index] + rpos_x;
        stadium->ball_pos_y = stadium->players.pos_y[stadium->ball_catcher_index] + rpos_y;
    }
}

PUF_FN void stadium_step(Stadium *stadium) {
    apply_legs_effects(stadium);
    reset_command_flags(stadium);

    if (stadium->playmode == PM_BeforeKickOff) {
        turnMovableObjects(stadium, NUM_PLAYERS);
        stadium->stoppage_time += 1;
        #if REFEREES_ENABLED
        ref_analyse(stadium);
        #endif
    }
    else if (stadium->playmode == PM_AfterGoal_Right
        || stadium->playmode == PM_AfterGoal_Left
        || stadium->playmode == PM_OffSide_Right
        || stadium->playmode == PM_OffSide_Left
        || stadium->playmode == PM_Illegal_Defense_Left
        || stadium->playmode == PM_Illegal_Defense_Right
        || stadium->playmode == PM_Foul_Charge_Right
        || stadium->playmode == PM_Foul_Charge_Left
        || stadium->playmode == PM_Foul_Push_Right
        || stadium->playmode == PM_Foul_Push_Left
        || stadium->playmode == PM_Back_Pass_Right
        || stadium->playmode == PM_Back_Pass_Left
        || stadium->playmode == PM_Free_Kick_Fault_Right
        || stadium->playmode == PM_Free_Kick_Fault_Left
        || stadium->playmode == PM_CatchFault_Right
        || stadium->playmode == PM_CatchFault_Left) {
        PlayMode pm = stadium->playmode;
        stadium->ball_catcher_index = -1;
        incMovableObjects(stadium, NUM_PLAYERS);
        stadium->stoppage_time += 1;
        #if REFEREES_ENABLED
        ref_analyse(stadium);
        #endif
        // mydiff
        placePlayersInField(stadium);
        
        if ( pm != stadium->playmode )
        {
            stadium->time += 1;
            stadium->stoppage_time = 0;
        }
    }
    else if (stadium->playmode != PM_BeforeKickOff && stadium->playmode != PM_TimeOver) {
        incMovableObjects(stadium, NUM_PLAYERS);
        stadium->time += 1;
        stadium->stoppage_time = 0;
        #if REFEREES_ENABLED
        ref_analyse(stadium);
        #endif
        // mydiff
        placePlayersInField(stadium);
    }
    else if (stadium->playmode == PM_TimeOver) {
        stadium->stoppage_time += 1;
    }

    //
    // update stamina etc, reset player states
    //
    update_stamina_and_capacities_and_reset_states(stadium);
}

PUF_FN void stadium_moveBall(Stadium* stadium, float x, float y, float velx, float vely) {
    stadium->ball_catcher_index = -1;
    stadium->ball_pos_x = x;
    stadium->ball_pos_y = y;
    stadium->ball_vel_x = velx;
    stadium->ball_vel_y = vely;
    stadium->ball_accel_x = 0.0f;
    stadium->ball_accel_y = 0.0f;
}

PUF_FN void stadium_movePlayer(Stadium* stadium, int player_index, double x, double y, double ang, double velx, double vely) {
    ang = Deg2Rad(clamp(ang, MIN_MOMENT, MAX_MOMENT));
    stadium->players.pos_x[player_index] = x;
    stadium->players.pos_y[player_index] = y;
    stadium->players.angle_body[player_index] = ang;
    stadium->players.angle_body_committed[player_index] = ang;
    stadium->players.vel_x[player_index] = velx;
    stadium->players.vel_y[player_index] = vely;
    collisions(stadium);
}

PUF_FN void recoverAll(Stadium *stadium, int player_index) {
    stadium->players.stamina[player_index] = STAMINA_MAX;
    stadium->players.recovery[player_index] = 1.0f;
    stadium->players.effort[player_index] = stadium->players.player_type[player_index].effort_max;
    recoverStaminaCapacity(stadium, player_index);
    stadium->players.consumed_stamina[player_index] = 0.0f;
    stadium->players.hear_capacity_from_teammate[player_index] = HEAR_MAX;
    stadium->players.hear_capacity_from_opponent[player_index] = HEAR_MAX;
}

PUF_FN void stadium_recoveryPlayers(Stadium* stadium) {
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players.enable[i]) continue;
        recoverAll(stadium, i);
    }
}

PUF_FN void setPlayerType(Stadium *stadium, int player_index, const int id) {
    const MyHeteroPlayer *type = &stadium->player_types[id];
    if (!type)
        return;

    // stadium->players.player_type_id[player_index] = id;
    stadium->players.player_type[player_index] = *type;
    stadium->players.max_speed[player_index] = stadium->players.player_type[player_index].player_speed_max;
    stadium->players.decay[player_index] = stadium->players.player_type[player_index].player_decay;
    stadium->players.size[player_index] = stadium->players.player_type[player_index].player_size;
    stadium->players.kick_rand[player_index] = stadium->players.player_type[player_index].kick_rand;

    // mydiff : actuator noise is disabled by default
    // if ( ServerParam::instance().teamActuatorNoise() )
    // {
    //     stadium->players.kick_rand[player_index] *= team()->kickRandFactorTeam();
    // }
}

PUF_FN void hetero_player_setDefault(MyHeteroPlayer *player_type) {
    player_type->player_speed_max = PLAYER_SPEED_MAX;
    player_type->stamina_inc_max = STAMINA_INC_MAX;
    player_type->player_decay = PLAYER_DECAY;
    player_type->inertia_moment = INERTIA_MOMENT;
    player_type->dash_power_rate = DASH_POWER_RATE;
    player_type->player_size = PLAYER_SIZE;
    player_type->kickable_margin = KICKABLE_MARGIN;
    player_type->kick_rand = KICK_RAND;
    player_type->extra_stamina = EXTRA_STAMINA;
    player_type->effort_max = EFFORT_INIT;
    player_type->effort_min = EFFORT_MIN;
    player_type->kick_power_rate = KICK_POWER_RATE;
    player_type->foul_detect_probability = FOUL_DETECT_PROBABILITY;
    // mydiff
    // player_type->catchable_area_l_stretch = 1.0f;
    // setDefaultObservationParams();
    // setDefaultGaussianObservationParams();
}

PUF_FN float hetero_player_delta(const float min, const float max, unsigned int *seed) {
    if ( min == max )
        return min;

    return drand(seed, min, max);
}

PUF_FN void hetero_player_init(MyHeteroPlayer *player_type, unsigned int *seed) {
    const int MAX_TRIAL = 1000;
    int trial = 0;
    while ( ++trial <= MAX_TRIAL )
    {
        // trade-off player_speed_max with stamina_inc_max (actually unused)
        float tmp_delta = hetero_player_delta(PLAYER_SPEED_MAX_DELTA_MIN, PLAYER_SPEED_MAX_DELTA_MAX, seed );
        player_type->player_speed_max = PLAYER_SPEED_MAX + tmp_delta;
        if ( player_type->player_speed_max <= 0.0 ) continue;
        player_type->stamina_inc_max = STAMINA_INC_MAX + tmp_delta * STAMINA_INC_MAX_DELTA_FACTOR;
        if ( player_type->stamina_inc_max <= 0.0 ) continue;

        // trade-off player_decay with inertia_moment
        tmp_delta = hetero_player_delta(PLAYER_DECAY_DELTA_MIN, PLAYER_DECAY_DELTA_MAX, seed);
        player_type->player_decay = PLAYER_DECAY + tmp_delta;
        if ( player_type->player_decay <= 0.0 ) continue;
        player_type->inertia_moment = INERTIA_MOMENT + tmp_delta * INERTIA_MOMENT_DELTA_FACTOR;
        if ( player_type->inertia_moment < 0.0 ) continue;

        // trade-off dash_power_rate with player_size (actually unused)
        tmp_delta = hetero_player_delta(DASH_POWER_RATE_DELTA_MIN, DASH_POWER_RATE_DELTA_MAX, seed);
        player_type->dash_power_rate = DASH_POWER_RATE + tmp_delta;
        if ( player_type->dash_power_rate <= 0.0 ) continue;
        player_type->player_size = PLAYER_SIZE + tmp_delta * PLAYER_SIZE_DELTA_FACTOR;
        if ( player_type->player_size <= 0.0 ) continue;

        // trade-off stamina_inc_max with dash_power_rate
        tmp_delta = hetero_player_delta(NEW_DASH_POWER_RATE_DELTA_MIN, NEW_DASH_POWER_RATE_DELTA_MAX, seed );
        player_type->dash_power_rate = DASH_POWER_RATE + tmp_delta;
        if ( player_type->dash_power_rate <= 0.0 ) continue;
        player_type->stamina_inc_max = STAMINA_INC_MAX + tmp_delta * NEW_STAMINA_INC_MAX_DELTA_FACTOR;
        if ( player_type->stamina_inc_max <= 0.0 ) continue;

        // trade-off kickable_margin with kick_rand
        tmp_delta = hetero_player_delta(KICKABLE_MARGIN_DELTA_MIN, KICKABLE_MARGIN_DELTA_MAX, seed );
        player_type->kickable_margin = KICKABLE_MARGIN + tmp_delta;
        if ( player_type->kickable_margin <= 0.0 ) continue;
        player_type->kick_rand = KICK_RAND + tmp_delta * KICK_RAND_DELTA_FACTOR;
        if ( player_type->kick_rand < 0.0 ) continue;

        // trade-off extra_stamina with effort_{min,max}
        tmp_delta = hetero_player_delta(EXTRA_STAMINA_DELTA_MIN, EXTRA_STAMINA_DELTA_MAX, seed );
        player_type->extra_stamina = EXTRA_STAMINA + tmp_delta;
        if ( player_type->extra_stamina < 0.0 ) continue;
        player_type->effort_max = EFFORT_INIT + tmp_delta * EFFORT_MAX_DELTA_FACTOR;
        player_type->effort_min = EFFORT_MIN  + tmp_delta * EFFORT_MIN_DELTA_FACTOR;
        if ( player_type->effort_max <= 0.0 ) continue;
        if ( player_type->effort_min <= 0.0 ) continue;

        // v14
        // trade-off kick_power_rate with foul_detect_probability
        tmp_delta = hetero_player_delta(KICK_POWER_RATE_DELTA_MIN, KICK_POWER_RATE_DELTA_MAX, seed );
        player_type->kick_power_rate = KICK_POWER_RATE + tmp_delta;
        player_type->foul_detect_probability = FOUL_DETECT_PROBABILITY + tmp_delta * FOUL_DETECT_PROBABILITY_DELTA_FACTOR;

        // mydiff
        // // trade-off catchable_area_l with catch probability
        // tmp_delta = hetero_player_delta( PP.catchAreaLengthStretchMin(), PP.catchAreaLengthStretchMax(), seed );
        // player_type->catchable_area_l_stretch = tmp_delta;
        // setDefaultObservationParams();
        // setDefaultGaussianObservationParams();

        //
        float real_speed_max = ( MAX_POWER * player_type->dash_power_rate * player_type->effort_max ) / ( 1.0 - player_type->player_decay );
        if ( PLAYER_SPEED_MAX_MIN - EPS < real_speed_max && real_speed_max < player_type->player_speed_max + EPS )
            break;
    }

    if ( trial > MAX_TRIAL )
    {
        // printf("HeteroPlayer set default parameters.\n");
        hetero_player_setDefault(player_type);
    }
}

PUF_FN void reset_player(Stadium *stadium, int player_index) {
    stadium->players.pos_x[player_index] = -(stadium->players.unum[player_index] * 3 * stadium->players.side[player_index]);
    stadium->players.pos_y[player_index] = -PITCH_WIDTH / 2.0f - 3.0f;
    stadium->players.vel_x[player_index] = 0.0f;
    stadium->players.vel_y[player_index] = 0.0f;
    stadium->players.accel_x[player_index] = 0.0f;
    stadium->players.accel_y[player_index] = 0.0f;
    stadium->players.size[player_index] = 1.0f;
    
    stadium->players.post_collision_pos_x[player_index] = 0.0f;
    stadium->players.post_collision_pos_y[player_index] = 0.0f;
    stadium->players.collision_count[player_index] = 0;
    stadium->players.collided[player_index] = false;

    stadium->players.state[player_index] = STATE_DISABLE;
    stadium->players.stamina[player_index] = STAMINA_MAX;
    stadium->players.recovery[player_index] = RECOVER_INIT;
    stadium->players.effort[player_index] = EFFORT_INIT;
    stadium->players.stamina_capacity[player_index] = STAMINA_CAPACITY;
    
    stadium->players.angle_body[player_index] = 0.0f;
    stadium->players.angle_body_committed[player_index] = 0.0f;
    stadium->players.angle_neck[player_index] = 0.0f;
    stadium->players.angle_neck_committed[player_index] = 0.0f;

    stadium->players.ball_collide[player_index] = false;
    stadium->players.player_collide[player_index] = false;
    stadium->players.post_collide[player_index] = false;

    stadium->players.command_done[player_index] = false;
    stadium->players.turn_neck_done[player_index] = false;
    stadium->players.done_received[player_index] = false;

    stadium->players.goalie_catch_ban[player_index] = 0;
    stadium->players.kick_cycles[player_index] = 0;
    stadium->players.dash_cycles[player_index] = 0;
    stadium->players.tackle_cycles[player_index] = 0;
    stadium->players.foul_cycles[player_index] = 0;
    stadium->players.dash_count[player_index] = 0;

    stadium->players.hear_capacity_from_teammate[player_index] = HEAR_MAX;
    stadium->players.hear_capacity_from_opponent[player_index] = HEAR_MAX;

    stadium->players.max_speed[player_index] = PLAYER_SPEED_MAX;
    stadium->players.max_accel[player_index] = PLAYER_ACCEL_MAX;
    setPlayerType(stadium, player_index, 0);
    recoverAll(stadium, player_index);

    stadium->players.state[player_index] = STATE_STAND;
    stadium->players.enable[player_index] = true;
    if (stadium->players.goalie[player_index])
        stadium->players.state[player_index] |= STATE_GOALIE;
    stadium->players.angle_body_committed[player_index] = stadium->players.side[player_index] == LEFT ? TEAM_L_DIRECTION : TEAM_R_DIRECTION;
    stadium->players.randp[player_index] = PLAYER_RAND;
}

PUF_FN void stadium_reset(Stadium *stadium) {
    stadium->time = 0;
    stadium->stoppage_time = 0;
    stadium->last_playon_start = -1;
    stadium->playmode = PM_BeforeKickOff;
    stadium->kick_off_side = LEFT;

    for (int i = 0 ; i < NUM_PLAYERS ; i++)
        reset_player(stadium, i);

    stadium->ball_catcher_index = -1;

#if FREE_KICK_REFEREE_ENABLED
    stadium->free_kick_ref_kick_taker_index = -1;
#endif
#if TOUCH_REFEREE_ENABLED
    stadium->touch_ref_last_indirect_kicker_index = -1;
    stadium->touch_ref_last_touched_index = -1;
#endif
#if CATCH_REFEREE_ENABLED
    stadium->catch_ref_before_last_back_passer_index = -1;
    stadium->catch_ref_last_back_passer_index = -1;
#endif
#if PENALTY_REFEREE_ENABLED
    stadium->penalty_ref_last_taker_index = -1;
#endif

    stadium->ball_pos_x = 0.0f;
    stadium->ball_pos_y = 0.0f;
    stadium->ball_vel_x = 0.0f;
    stadium->ball_vel_y = 0.0f;
    stadium->ball_accel_x = 0.0f;
    stadium->ball_accel_y = 0.0f;
    stadium->ball_size = BALL_SIZE;
    stadium->ball_decay = BALL_DECAY;
    stadium->ball_randp = BALL_RAND;
    stadium->ball_max_speed = BALL_SPEED_MAX;
    stadium->ball_max_accel = BALL_ACCEL_MAX;
    stadium->ball_post_collision_pos_x = 0.0f;
    stadium->ball_post_collision_pos_y = 0.0f;
    stadium->ball_collision_count = 0;
    stadium->ball_collided = false;

    stadium->team_left_points = 0;
    stadium->team_right_points = 0;
    stadium->team_left_pen_taken = 0;
    stadium->team_right_pen_taken = 0;
    stadium->team_left_pen_point = 0;
    stadium->team_right_pen_point = 0;
    stadium->team_left_pen_won = false;
    stadium->team_right_pen_won = false;

    changePlayMode(stadium, PM_BeforeKickOff);
}

PUF_FN void stadium_init(Stadium *stadium, unsigned int seed) {
    stadium->seed = seed;

    hetero_player_setDefault(&stadium->player_types[0]);
    for (int i = 1; i < PLAYER_TYPES; i++) {
        hetero_player_init(&stadium->player_types[i], &stadium->seed);
    }

    for (int i = 0 ; i < LEFT_PLAYERS_COUNT ; i++) {
        stadium->players.side[i] = LEFT;
        stadium->players.goalie[i] = false;
        stadium->players.unum[i] = i + 1;
    }

    for (int i = 0 ; i < RIGHT_PLAYERS_COUNT ; i++) {
        stadium->players.side[LEFT_PLAYERS_COUNT + i] = RIGHT;
        stadium->players.goalie[LEFT_PLAYERS_COUNT + i] = false;
        stadium->players.unum[LEFT_PLAYERS_COUNT + i] = i + 1;
    }
    stadium->team_left_enabled = LEFT_PLAYERS_COUNT > 0;
    stadium->team_right_enabled = RIGHT_PLAYERS_COUNT > 0;
}

#endif // GAMEPLAY_H