#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <float.h>
#include <string.h>
#include "stadium_backup.h"

const float TWO_PIF = 2.0f * M_PIf;
const float DEG2RAD_COEF = (M_PIf/180.0f);

void kickTaken(MyStadiumV2 *stadium, int kicker_index, const MyPVector *accel);
void tackleTaken(MyStadiumV2 *stadium, int tackler_index, const MyPVector *accel, const bool foul);
void failedTackleTaken(MyStadiumV2 *stadium, int tackler_index, const bool foul);
void collisions(MyStadiumV2 *stadium);
#if REFEREES_ENABLED
void ref_playModeChange(MyStadiumV2 *stadium, PlayMode pm);
bool ref_isPenaltyShootOut(const PlayMode pm, const Side side);
void ref_kickTaken(MyStadiumV2 *stadium, int kicker_index, const float accel_r);
void ref_failedKickTaken(MyStadiumV2 *stadium, int kicker_index);
void ref_tackleTaken(MyStadiumV2 *stadium, int tackler_index, const float accel_r, const bool foul);
void ref_failedTackleTaken(MyStadiumV2 *stadium, int tackler_index, const bool foul);
#endif

#pragma region base_utilities

static inline float r(const MyPVector *vec) {
    return sqrtf(vec->x * vec->x + vec->y * vec->y);
}

static inline float r2(MyPVector *vec) {
    return vec->x * vec->x + vec->y * vec->y;
}

static inline float th(MyPVector *vec) {
    return ((vec->x == 0.0f) && (vec->y == 0.0f) ? 0.0f : atan2f(vec->y, vec->x));
}

static inline float distance(const MyPVector *vec, const MyPVector* orig) {
    MyPVector diff = {vec->x - orig->x, vec->y - orig->y};
    return r(&diff);
}

static inline float distance2(const MyPVector *vec, const MyPVector* orig) {
    MyPVector diff = {vec->x - orig->x, vec->y - orig->y};
    return r2(&diff);
}

static inline void normalize(MyPVector *vec, const float l) {
    float vec_r = r(vec);
    float coef = (l / fmaxf(vec_r, EPS));
    vec->x *= coef;
    vec->y *= coef;
}

static inline float add_eps(float x) {
    return nextafterf(x, HUGE_VALF);
}

static inline float subtract_eps(float x) {
    return nextafterf(x, -HUGE_VALF);
}

static inline void rotate(MyPVector *vec, const float ang) {
    float c = cosf(ang);
    float s = sinf(ang);
    float new_x = vec->x * c - vec->y * s;
    float new_y = vec->x * s + vec->y * c;
    vec->x = new_x;
    vec->y = new_y;
}

static inline float drand(unsigned int *seed, float low, float high) {
    if (low > high) {
        float temp = low;
        low = high;
        high = temp;
    }

    if (high - low < 1.0e-10f)
        return (low + high) * 0.5f;

    return low + ((float)rand_r(seed) / (float)RAND_MAX) * (high - low);
}

static inline float brand(unsigned int *seed, float prob) {
    return (float)rand_r(seed) / (RAND_MAX + 1.0) < prob;
}

static inline float normalize_angle(float ang) {
    if (fabsf(ang) > TWO_PIF)
        ang = fmodf(ang, TWO_PIF);
    if (ang < -M_PIf)
        ang += TWO_PIF;
    if (ang > M_PIf)
        ang -= TWO_PIF;
    return ang;
}

static inline float Deg2Rad(const float a) {
    return a * DEG2RAD_COEF;
}

static inline float clamp(float x, float low, float high) {
    if (x < low)
        return low;
    if (x > high)
        return high;
    return x;
}

static inline void from_polar(float r, float ang, MyPVector *out) {
    out->x = r * cosf(ang);
    out->y = r * sinf(ang);
}

static inline bool between(const MyPVector* vec, const MyPVector* begin, const MyPVector* end) {
    if (begin->x > end->x) {
        return between(vec, end, begin);
    }

    if (begin->x <= vec->x && vec->x <= end->x) {
        if (begin->y < end->y)
            return begin->y <= vec->y && vec->y <= end->y;
        else
            return begin->y >= vec->y && vec->y >= end->y;
    }

    return false;
}

#pragma endregion

static inline float normalize_dash_power(const float p) {
    return clamp(p, MIN_DASH_POWER, MAX_DASH_POWER);
}

static inline float normalize_dash_angle(const float d) {
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

static inline float NormalizeMoment(const float p) {
    return Deg2Rad(clamp(p, MIN_MOMENT, MAX_MOMENT));
}

static inline float NormalizeNeckMoment(const float p) {
    return Deg2Rad(clamp(p, MIN_NECK_MOMENT, MAX_NECK_MOMENT));
}

static inline float NormalizeNeckAngle(const float p) {
    return clamp(p, MIN_NECK_ANGLE, MAX_NECK_ANGLE);
}

static inline float NormalizeKickPower(const float p) {
    return clamp(p, MIN_POWER, MAX_POWER);
}

void calcDashAccel(const float consumed_stamina, float leg_dash_power, float leg_dash_dir, MyPlayerV2 *player, MyPVector *out) {
    const float power = normalize_dash_power(leg_dash_power < 0.0f ? -consumed_stamina : consumed_stamina * 2.0f);
    const float unnormalized_dir_rate = fabsf(leg_dash_dir) > 90.0f 
        ? BACK_DASH_RATE - ((BACK_DASH_RATE - SIDE_DASH_RATE)) * (1.0f - (fabsf(leg_dash_dir) - 90.0f) / 90.0f) 
        : SIDE_DASH_RATE + ((1.0f - SIDE_DASH_RATE) * (1.0f - fabsf(leg_dash_dir) / 90.0f));
    const float dir_rate = clamp(unnormalized_dir_rate , 0.0f, 1.0f);
    float accel_magnitude = fabsf(player->effort * power * dir_rate * player->player_type.dash_power_rate);
    if (player->pos.y < 0.0f) {
        accel_magnitude /= (player->side == LEFT ? SLOWNESS_ON_TOP_FOR_LEFT : SLOWNESS_ON_TOP_FOR_RIGHT);
    }
    MyPVector accel;
    from_polar(accel_magnitude, normalize_angle(player->angle_body_committed + Deg2Rad(leg_dash_dir)), &accel);
    if (power < 0.0f) {
        accel.x = -accel.x;
        accel.y = -accel.y;
    }
    out->x = accel.x;
    out->y = accel.y;
}

void applyDashEffect(MyPlayerV2 *player, unsigned int *seed) {
    if (player->left_leg_command_type != NONE && player->right_leg_command_type != NONE)
        return;

    player->dash_count += 1;
    float left_power = player->left_leg_dash_power;
    float right_power = player->right_leg_dash_power;
    float left_consumed_stamina = (left_power < 0.0f ? -left_power : left_power * 0.5);
    float right_consumed_stamina = (right_power < 0.0f ? -right_power : right_power * 0.5);
    float consumed_stamina = left_consumed_stamina + right_consumed_stamina;
    if (consumed_stamina < 1.0e-5f)
        return;

    consumed_stamina = fminf(consumed_stamina, player->stamina + player->player_type.extra_stamina);
    left_consumed_stamina = consumed_stamina * left_consumed_stamina / ( left_consumed_stamina + right_consumed_stamina );
    right_consumed_stamina = consumed_stamina * right_consumed_stamina / ( left_consumed_stamina + right_consumed_stamina );
    MyPVector left_accel, right_accel;
    calcDashAccel(left_consumed_stamina, player->left_leg_dash_power, 
        player->left_leg_dash_dir, player, &left_accel);
    calcDashAccel(right_consumed_stamina, player->right_leg_dash_power, 
        player->right_leg_dash_dir, player, &right_accel);
    MyPVector body_unit;
    from_polar(1.0f, player->angle_body_committed, &body_unit);
    MyPVector vel_l = {
        .x = player->vel.x + left_accel.x,
        .y = player->vel.y + left_accel.y
    };
    MyPVector vel_r = {
        .x = player->vel.x + right_accel.x,
        .y = player->vel.y + right_accel.y
    };

    const float vel_l_body = body_unit.x * vel_l.x + body_unit.y * vel_l.y;
    const float vel_r_body = body_unit.x * vel_r.x + body_unit.y * vel_r.y;
    MyPVector new_vel = {
        .x = (vel_r.x + vel_l.x) / 2.0f,
        .y = (vel_r.y + vel_l.y) / 2.0f
    };
    player->accel.x += new_vel.x - player->vel.x;
    player->accel.y += new_vel.y - player->vel.y;

    if (player->left_leg_command_type == DASH && player->right_leg_command_type == DASH) {
        float omega = (vel_l_body - vel_r_body) / ( player->player_type.player_size * 2.0f );
        player->angle_body = normalize_angle(player->angle_body_committed + 
            (1.0f + drand(seed, -player->randp, player->randp)) * omega);
    }

    player->stamina = fmaxf(0.0f, player->stamina - consumed_stamina);
}

void applyLegsEffect(MyPlayerV2 *player, unsigned int *seed) {
    applyDashEffect(player, seed);
}

void resetCommandFlags(MyPlayerV2 *player) {
    if (player->kick_cycles >= 0)
        player->kick_cycles -= 1;

    if (player->dash_cycles >= 0)
        player->dash_cycles -= 1;

    if (player->tackle_cycles > 0)
        player->tackle_cycles -= 1;

    if (player->foul_cycles > 0)
        player->foul_cycles -= 1;

    if ( player->kick_cycles <= 0
         && player->tackle_cycles == 0
         && player->foul_cycles == 0 )
        player->command_done = false;

    player->turn_neck_done = false;
    player->done_received = false;
    player->left_leg_command_type = NONE;
    player->right_leg_command_type = NONE;
    player->left_leg_dash_power = 0.0f;
    player->right_leg_dash_power = 0.0f;
    player->left_leg_dash_dir = 0.0f;
    player->right_leg_dash_dir = 0.0f;
}

void _turn(MyPlayerV2 *player) {
    player->angle_body_committed = player->angle_body;
    player->angle_neck_committed = player->angle_neck;
    player->vel.x = 0.0f;
    player->vel.y = 0.0f;
    player->accel.x = 0.0f;
    player->accel.y = 0.0f;
}

void updateAngle_player(MyPlayerV2 *player) {
    player->angle_body_committed = player->angle_body;
    player->angle_neck_committed = player->angle_neck;
}

void collidedWithPost(MyPlayerV2* player) {
    player->state |= STATE_POST_COLLIDE;
    player->post_collide = true;
}

void collidedWithBall(MyPlayerV2* player) {
    player->state |= (STATE_BALL_TO_PLAYER | STATE_BALL_COLLIDE);
    player->ball_collide = true;
}

void collidedWithPlayer(MyPlayerV2* player) {
    player->state |= STATE_PLAYER_COLLIDE;
    player->player_collide = true;
}

void clearCollision(MyPVector *post_collision_pos, int *collision_count) {
    post_collision_pos->x = 0.0f;
    post_collision_pos->y = 0.0f;
    *collision_count = 0;
}

void updateStamina(MyPlayerV2 *player) {
    if (player->stamina <= RECOVER_DEC_THR * STAMINA_MAX) {
        if (player->recovery > RECOVER_MIN)
            player->recovery -= RECOVER_DEC;

        if (player->recovery < RECOVER_MIN)
            player->recovery = RECOVER_MIN;
    }

    if (player->stamina <= EFFORT_DEC_THR * STAMINA_MAX) {
        if (player->effort > player->player_type.effort_min)
            player->effort -= EFFORT_DEC;

        if (player->effort < player->player_type.effort_min)
            player->effort = player->player_type.effort_min;
    }

    if (player->stamina >= EFFORT_INC_THR * STAMINA_MAX) {
        if (player->effort < player->player_type.effort_max) {
            player->effort += EFFORT_INC;
            if (player->effort > player->player_type.effort_max)
                player->effort = player->player_type.effort_max;
        }
    }

    float stamina_inc = fminf(player->recovery * player->player_type.stamina_inc_max, STAMINA_MAX - player->stamina);
    if (STAMINA_CAPACITY >= 0.0f) {
        if (stamina_inc > player->stamina_capacity)
            stamina_inc = player->stamina_capacity;
    }

    player->stamina += stamina_inc;
    if (player->stamina > STAMINA_MAX)
        player->stamina = STAMINA_MAX;

    if (STAMINA_CAPACITY >= 0.0f) {
        player->stamina_capacity -= stamina_inc;
        if (player->stamina_capacity < 0.0f)
            player->stamina_capacity = 0.0f;
    }
}

void updateCapacity(MyPlayerV2 *player) {
    player->hear_capacity_from_teammate += HEAR_INC;
    if (player->hear_capacity_from_teammate > (int)HEAR_MAX)
        player->hear_capacity_from_teammate = HEAR_MAX;

    player->hear_capacity_from_opponent += HEAR_INC;
    if (player->hear_capacity_from_opponent > (int)HEAR_MAX)
        player->hear_capacity_from_opponent = HEAR_MAX;

    if (player->goalie_catch_ban > 0 )
        player->goalie_catch_ban -= 1;
}

void resetState(MyPlayerV2 *player) {
    int state = (STATE_STAND | STATE_GOALIE | STATE_DISCARD | STATE_YELLOW_CARD | STATE_RED_CARD);
    if (player->kick_cycles > 0)
        state |= (STATE_KICK | STATE_KICK_FAULT);

    if (player->tackle_cycles > 0)
        state |= (STATE_TACKLE | STATE_TACKLE_FAULT);

    if (player->foul_cycles > 0)
        state |= STATE_FOUL_CHARGED;

    player->state &= state;
}

#pragma region player_commands

void dashLeftLeg(MyPlayerV2 *player, double power, double dir) {
    if (player->left_leg_command_type != NONE)
        return;

    player->left_leg_dash_power = normalize_dash_power( power );
    player->left_leg_dash_dir = normalize_dash_angle( dir );
    player->left_leg_command_type = DASH;
    player->dash_cycles = 1;
    player->command_done = true;
}

void dashRightLeg(MyPlayerV2 *player, double power, double dir) {
    if (player->right_leg_command_type != NONE)
        return;

    player->right_leg_dash_power = normalize_dash_power( power );
    player->right_leg_dash_dir = normalize_dash_angle( dir );
    player->right_leg_command_type = DASH;
    player->dash_cycles = 1;
    player->command_done = true;
}

void dash(MyPlayerV2 *player, double power, double dir) {
    if (!player->command_done) {
        dashLeftLeg(player, power, dir);
        dashRightLeg(player, power, dir);
    }
}

void turn(MyPlayerV2 *player, float moment, unsigned int *seed) {
    if (!player->command_done) {
        if (player->left_leg_command_type == NONE)
            player->left_leg_command_type = TURN;
        
        if (player->right_leg_command_type == NONE)
            player->right_leg_command_type = TURN;

        player->angle_body = normalize_angle(player->angle_body_committed
            + (1.0f + drand(seed, -player->randp, player->randp))
            * NormalizeMoment(moment)
            / (1.0f + player->player_type.inertia_moment * r(&player->vel)));
        player->turn_count += 1;
        player->command_done = true;
    }
}

void turn_neck(MyPlayerV2 *player, double moment) {
    if (!player->turn_neck_done) {
        player->angle_neck = NormalizeNeckAngle(player->angle_neck_committed + NormalizeNeckMoment(moment));
        player->turn_neck_count += 1;
        player->turn_neck_done = true;
    }
}

float kickableArea(MyPlayerV2 *player) {
    return player->player_type.player_size + BALL_SIZE + player->player_type.kickable_margin;
}

bool ballKickable(MyStadiumV2 *stadium, MyPlayerV2 *player) {
    return distance2(&player->pos, &stadium->ball.pos) <= powf(kickableArea(player), 2.0f);
}

float angleFromBody(const MyPlayerV2 *player, const MyPVector *obj_pos) {
    MyPVector diff = {obj_pos->x - player->pos.x, obj_pos->y - player->pos.y};
    return normalize_angle(th(&diff) - player->angle_body_committed);
}

void kick(MyStadiumV2 *stadium, int player_index, double power, double dir) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if (player->command_done)
        return;

    player->command_done = true;
    player->kick_cycles = 1;
    power = NormalizeKickPower( power );
    dir = NormalizeMoment( dir );
    player->state |= KICK;

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
        player->state |= STATE_KICK_FAULT;
        return;
    }

    if (!ballKickable(stadium, player)) {
        player->state |= STATE_KICK_FAULT;
        #if REFEREES_ENABLED
        ref_failedKickTaken(stadium, player_index);
        #endif
        return;
    }

    float dir_diff = fabsf(angleFromBody(player, &stadium->ball.pos));
    MyPVector rpos = {stadium->ball.pos.x - player->pos.x, stadium->ball.pos.y - player->pos.y};
    float dist_ball = r(&rpos) - player->player_type.player_size - BALL_SIZE;
    float eff_power = power * player->player_type.kick_power_rate * 
        (1.0f - 0.25f * dir_diff / M_PIf - 0.25f * dist_ball / player->player_type.kickable_margin);
    MyPVector accel;
    from_polar(eff_power, dir + player->angle_body_committed, &accel);

    // [0.5, 1.0]
    float pos_rate = 0.5f + 0.25f * (dir_diff / M_PIf + dist_ball / player->player_type.kickable_margin);
    // [0.5, 1.0]
    float speed_rate = 0.5f + 0.5f * (r(&stadium->ball.vel) / (BALL_SPEED_MAX * BALL_DECAY));
    // [0, 2*kick_rand]
    float max_rand = player->kick_rand * (power / MAX_POWER) * (pos_rate + speed_rate);
    MyPVector kick_noise;
    from_polar(drand(stadium->seed, 0.0f, max_rand), 
        drand(stadium->seed, -M_PIf, M_PIf), &kick_noise);
    accel.x += kick_noise.x;
    accel.y += kick_noise.y;
    kickTaken(stadium, player_index, &accel);
    player->kick_count += 1;
}

void tackle(MyStadiumV2 *stadium, int player_index, double power_or_angle, bool foul) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if (player->command_done)
        return;

    if (player->left_leg_command_type == NONE)
        player->left_leg_command_type = TACKLE;

    if (player->right_leg_command_type == NONE)
        player->right_leg_command_type = TACKLE;

    player->command_done = true;
    player->tackle_cycles = TACKLE_CYCLES;
    player->tackle_count += 1;
    MyPVector player_2_ball = {stadium->ball.pos.x - player->pos.x, stadium->ball.pos.y - player->pos.y};
    rotate(&player_2_ball, -player->angle_body_committed);

    float tackle_dist = (player_2_ball.x > 0.0f ? TACKLE_DIST : TACKLE_BACK_DIST);

    if (fabsf( tackle_dist ) <= 1.0e-5f) {
        player->state |= STATE_TACKLE_FAULT;
        return;
    }

    float exponent = TACKLE_EXPONENT;

    // 2009-10-22 akiyama: foul option
    if ( foul )
    {
        foul = false;
        for (int i = 0 ; i < NUM_PLAYERS ; i++)
        {
            MyPlayerV2 *p = &stadium->players[i];
            if (p->enable
                && p->side != player->side
                && ballKickable(stadium, p))
            {
                foul = true;
                exponent = FOUL_EXPONENT;
                break;
            }
        }
    }

    // tackle failure probability
    float prob = (powf(fabsf( player_2_ball.x ) / tackle_dist, exponent ) + 
        powf(fabsf( player_2_ball.y ) / TACKLE_WIDTH, exponent));

    if (prob < 1.0f) {
        if (brand(stadium->seed, 1 - prob)) {
            player->state |= TACKLE;
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
                player->tackle_cycles = 0;

            float power_rate = 1.0f;
            MyPVector accel = {0.0f, 0.0f};
            // 2008-02-07 akiyama
            // new tackle model based on the Thomas Gabel's proposal
            float angle = NormalizeMoment( power_or_angle );
            float eff_power = (MAX_BACK_TACKLE_POWER + ((MAX_TACKLE_POWER - MAX_BACK_TACKLE_POWER) * 
                ( 1.0f - ( fabsf( angle ) / M_PIf )))) * TACKLE_POWER_RATE;
            eff_power *= 1.0f - 0.5f * (fabsf(th(&player_2_ball)) / M_PIf);
            from_polar(eff_power, angle + player->angle_body_committed, &accel);

            // akiyama 2008-01-30
            // new kick noise
            // [0.5, 1]
            float pos_rate = 0.5f + 0.5f * (1.0f - prob);
            // [0.5, 1]
            float speed_rate = 0.5f + 0.5f * (r(&stadium->ball.vel) / (BALL_SPEED_MAX * BALL_DECAY));
            // [0, 2*tackle_rand]
            // tackle_rand = kick_rand * server::tackle_rand_factor
            float max_rand = player->kick_rand * TACKLE_RAND_FACTOR * power_rate * (pos_rate + speed_rate);
            MyPVector tackle_noise;
            from_polar(drand(stadium->seed, 0.0f, max_rand), 
                drand(stadium->seed, -M_PIf, M_PIf ), &tackle_noise);
            accel.x += tackle_noise.x;
            accel.y += tackle_noise.y;
            tackleTaken(stadium, player_index, &accel, foul);
        }
        else
        {
            failedTackleTaken(stadium, player_index, foul);
            player->state |= (STATE_TACKLE | STATE_TACKLE_FAULT);
        }
    }
    else
    {
        failedTackleTaken(stadium, player_index, foul);
        player->state |= STATE_TACKLE_FAULT;
    }
}

#pragma endregion

void noise(float randp, MyPVector *vel, unsigned int *seed, MyPVector *out) {
    float maxrnd = randp * r(vel);
    from_polar(drand(seed, 0.0f, maxrnd), drand(seed, -M_PIf, M_PIf), out);
}

void nearestPost(const MyPVector* pos, const float size, MyPVector* center, float* radius) {
    MyPVector nearest_gpost;
    if (pos->y > 0) {
        if (pos->x > 0) {
            nearest_gpost.x = PITCH_LENGTH * 0.5f - GOAL_POST_RADIUS;
            nearest_gpost.y = GOAL_WIDTH * 0.5f + GOAL_POST_RADIUS;
        }
        else {
            nearest_gpost.x = -PITCH_LENGTH * 0.5f + GOAL_POST_RADIUS;
            nearest_gpost.y = GOAL_WIDTH * 0.5f + GOAL_POST_RADIUS;
        }
    }
    else {
        if ( pos->x > 0 ) {
            nearest_gpost.x = PITCH_LENGTH * 0.5f - GOAL_POST_RADIUS;
            nearest_gpost.y = -GOAL_WIDTH * 0.5f - GOAL_POST_RADIUS;
        }
        else {
            nearest_gpost.x = -PITCH_LENGTH * 0.5f + GOAL_POST_RADIUS;
            nearest_gpost.y = -GOAL_WIDTH * 0.5f - GOAL_POST_RADIUS;
        }
    }

    center->x = nearest_gpost.x;
    center->y = nearest_gpost.y;
    *radius = GOAL_POST_RADIUS + size;
}

static inline void nearestEdgeCircle(const MyPVector *center, const float radius, const MyPVector *p, MyPVector *out) {
    MyPVector dif = {p->x - center->x, p->y - center->y};
    if (dif.x == 0.0f && dif.y == 0.0f) {
        dif.x = EPS;
        dif.y = EPS;
    }

    normalize(&dif, radius);
    out->x = center->x + dif.x;
    out->y = center->y + dif.y;
}

static inline void nearestHEdge(const float l, const float r, const float t, const float b, const MyPVector * p, MyPVector * out) {
    out->x = fminf(fmaxf( p->x, l), r);
    out->y = fabsf( p->y - t) < fabsf( p->y - b) ? t : b;
}

static inline void nearestVEdge(const float l, const float r, const float t, const float b, const MyPVector * p, MyPVector * out) {
    out->x = fabsf( p->x - l) < fabsf( p->x - r) ? l : r;
    out->y = fminf(fmaxf( p->y, t), b);
}

static inline void nearestEdge(const float l, const float r, const float t, const float b, const MyPVector * p, MyPVector * out) {
    if (fminf(fabsf(p->x - l), fabsf(p->x - r)) < fminf(fabsf(p->y - t), fabsf(p->y - b)))
        nearestVEdge(l, r, t, b, p, out);
    else
        nearestHEdge(l, r, t, b, p, out);
}

static inline bool inArea(const float l, const float r, const float t, const float b, const MyPVector *p) {
    return (p->x >= l) && (p->x <= r) && (p->y <= t) && (p->y >= b);
}

void placePlayersInField(MyStadiumV2 *stadium) {
    static const float pitch_right = PITCH_LENGTH / 2.0f + PITCH_MARGIN;
    static const float pitch_top = PITCH_WIDTH / 2.0f + PITCH_MARGIN;
    static const float pitch_left = -pitch_right;
    static const float pitch_bottom = -pitch_top;

    for (int i = 0; i < NUM_PLAYERS; i++) {
        MyPlayerV2 *player = &stadium->players[i];
        if (!player->enable) continue;
        if (!inArea(pitch_left, pitch_right, pitch_top, pitch_bottom, &player->pos)) {
            MyPVector new_pos;
            nearestEdge(pitch_left, pitch_right, pitch_top, pitch_bottom, &player->pos, &new_pos);
            player->pos.x = new_pos.x;
            player->pos.y = new_pos.y;
        }
    }
}

bool contain(const int *array, const int array_size, const int value) {
    for (int i = 0 ; i < array_size ; i++)
        if (array[i] == value)
            return true;

    return false;
}

void score(MyStadiumV2 *stadium, const Side side) {
    if (side == LEFT && stadium->team_left_enabled)
        stadium->team_left_points += 1;

    if (side == RIGHT && stadium->team_right_enabled)
        stadium->team_right_points += 1;
}

void penaltyScore(MyStadiumV2 *stadium, const Side side, const bool scored) {
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

void penaltyWinner(MyStadiumV2 *stadium, const Side side) {
    if (side == LEFT)
        stadium->team_left_pen_won = true;

    if (side == RIGHT)
        stadium->team_right_pen_won = true;
}

void changePlayMode(MyStadiumV2 *stadium, const PlayMode pm) {
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

void recoverStaminaCapacity(MyPlayerV2 *player) {
    player->stamina_capacity = STAMINA_CAPACITY;
}

void placeBall(MyStadiumV2 *stadium, const Side kick_off_side, const MyPVector *pos) {
    stadium->ball.pos.x = pos->x;
    stadium->ball.pos.y = pos->y;
    stadium->ball.vel.x = 0.0f;
    stadium->ball.vel.y = 0.0f;
    stadium->ball.accel.x = 0.0f;
    stadium->ball.accel.y = 0.0f;
    stadium->kick_off_side = kick_off_side;
}

void callHalfTime(MyStadiumV2 *stadium, const Side kick_off_side, const int half_time_count) {
    stadium->ball_catcher_index = -1;
    MyPVector pos = {0.0f, 0.0f};
    placeBall(stadium, kick_off_side, &pos);
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
            MyPlayerV2 *player = &stadium->players[i];
            if (!player->enable) continue;
            recoverStaminaCapacity(player);
        }
    }

    // mydiff
    // M_weather.halfTime();
}

void kickTaken(MyStadiumV2 *stadium, int kicker_index, const MyPVector *accel) {
    stadium->ball_catcher_index = -1;
    stadium->ball.accel.x += accel->x;
    stadium->ball.accel.y += accel->y;

    #if REFEREES_ENABLED
    const float accel_r = r(accel);
    ref_kickTaken(stadium, kicker_index, accel_r);
    #endif
}

void tackleTaken(MyStadiumV2 *stadium, int tackler_index, const MyPVector *accel, const bool foul) {
    stadium->ball_catcher_index = -1;
    stadium->ball.accel.x += accel->x;
    stadium->ball.accel.y += accel->y;

    #if REFEREES_ENABLED
    const float accel_r = r(accel);
    ref_tackleTaken(stadium, tackler_index, accel_r, foul);
    #endif
}

void failedTackleTaken(MyStadiumV2 *stadium, int tackler_index, const bool foul) {
    #if REFEREES_ENABLED
    ref_failedTackleTaken(stadium, tackler_index, foul);
    #endif
}

// MyPlayerV2* getPlayer(MyStadiumV2 *stadium, const Side side, const int unum ) {
//     if ( side == LEFT )
//     {
//         return &stadium->players[unum - 1];
//     }
//     else if ( side == RIGHT )
//     {
//         return &stadium->players[MAX_PLAYER + unum - 1];
//     }

//     return NULL;
// }

void setPlayerState(MyStadiumV2 *stadium, int player_index, const int state) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if (!player->enable)
        return;
    player->state |= state;
}

void punishFoulPlay(MyStadiumV2 *stadium, int player_index) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if (!player->enable)
        return;
    player->foul_count += 1;
}

void player_disable(MyStadiumV2 *stadium, int player_index) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if (player->goalie && stadium->ball_catcher_index == player_index)
        stadium->ball_catcher_index = -1;

    player->enable = false;
    int card = player->state;
    card &= (STATE_YELLOW_CARD | STATE_RED_CARD);
    player->state = STATE_DISABLE;
    player->state |= card;
    player->pos.x = -(player->unum * 3 * player->side);
    player->pos.y = -PITCH_WIDTH / 2.0f - 3.0f;
    player->vel.x = 0.0f;
    player->vel.y = 0.0f;
    player->accel.x = 0.0f;
    player->accel.y = 0.0f;

    // todo : need to figure out how to handle this
    // option 1: move the player to the end of array and change the players_size to active_players_size and reduce it by one.
    // cons: have to shift all the players after the disabled one to the left and then update all the indexes and pointers
    // to the players array (e.g. touch_ref_last_indirect_kicker_index)
    // option 2: have a active_players (initially copied from players array) and then remove the disabled player from it. 
    // cons: do the pointers and indices point to active_players array or players array?
    // cons: how does this work if we want to use SOA? (maybe add active and inactive arrays?)
}

void player_discard(MyStadiumV2 *stadium, int player_index) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if (player->state & STATE_STAND ) {
        player_disable(stadium, player_index);
        if (!(player->state & STATE_DISCARD))
            player->state |= STATE_DISCARD;
        else
            player->state &= ~STATE_DISCARD;
    }
}

void yellowCard(MyStadiumV2 *stadium, int player_index) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if (!player->enable)
        return;
    if (player->card_count == 1) {
        player_discard(stadium, player_index);
        player->state &= ~STATE_YELLOW_CARD;
        player->state |= STATE_RED_CARD;
        player->card_count = 2;
    }
    else {
        player->state |= STATE_YELLOW_CARD;
        ++player->card_count;
    }
}

void redCard(MyStadiumV2 *stadium, int player_index) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if (!player->enable)
        return;
    player_discard(stadium, player_index);
    player->state &= ~STATE_YELLOW_CARD;
    player->state |= STATE_RED_CARD;
    player->card_count = 2;   
}

#if REFEREES_ENABLED

void ref_placeBallAndChangePlayMode(MyStadiumV2 *stadium, const PlayMode pm, const Side kick_off_side, const MyPVector *pos) {
    placeBall(stadium, kick_off_side, pos);
    if (ref_isPenaltyShootOut(stadium->playmode, NEUTRAL) && (pm == PM_PlayOn || pm ==  PM_Drop_Ball)) {
        ; // never change pm to play_on in penalty mode
    }
    else
        changePlayMode(stadium, pm);
}

bool ref_inPenaltyArea(const Side side, const MyPVector *pos) {
    if (side != RIGHT) {
        // according to FIFA the ball is catchable if it is at
        // least partly within the penalty area, thus we add ball size
        static const float pen_left = -PITCH_LENGTH/2 + PENALTY_AREA_LENGTH/2.0 - (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
        static const float pen_right = -PITCH_LENGTH/2 + PENALTY_AREA_LENGTH/2.0 + (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
        static const float pen_top = (PENALTY_AREA_WIDTH + BALL_SIZE * 2) / 2;
        static const float pen_bottom = -pen_top;
        if (inArea(pen_left, pen_right, pen_top, pen_bottom, pos))
            return true;
    }

    if (side != LEFT) {
        // according to FIFA the ball is catchable if it is at
        // least partly within the penalty area, thus we add ball size
        static const float pen_left = +PITCH_LENGTH/2 - PENALTY_AREA_LENGTH/2.0 - (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
        static const float pen_right = +PITCH_LENGTH/2 - PENALTY_AREA_LENGTH/2.0 + (PENALTY_AREA_LENGTH + BALL_SIZE * 2) / 2;
        static const float pen_top = (PENALTY_AREA_WIDTH + BALL_SIZE * 2) / 2;
        static const float pen_bottom = -pen_top;
        if (inArea(pen_left, pen_right, pen_top, pen_bottom, pos))
            return true;
    }

    return false;
}

void ref_checkFoul(MyStadiumV2 *stadium, int tackler_index, const bool foul, 
    bool * detect_charge, bool * detect_yellow, bool * detect_red) {
    const MyPlayerV2 *tackler = &stadium->players[tackler_index];
    bool foul_charge = false;
    bool yellow_card = false;
    bool red_card = false;

    // 2011-05-14 akiyama
    // added red card probability
    const float ball_dist2 = distance2(&tackler->pos, &stadium->ball.pos);
    MyPVector diff = {stadium->ball.pos.x - tackler->pos.x, stadium->ball.pos.y - tackler->pos.y};
    const float ball_angle = th(&diff);
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        MyPlayerV2 *p = &stadium->players[i];
        if (!p->enable) continue;
        if (p->side == tackler->side)
            continue;

        if (!ballKickable(stadium, p))
            continue; // no kickable

        bool pre_check = false;
        if (foul) {
            p->foul_cycles = FOUL_CYCLES;
            p->command_done = true;
            p->state |= STATE_FOUL_CHARGED;
            if (brand(stadium->seed, tackler->player_type.foul_detect_probability)) {
                pre_check = true;
                foul_charge = true;
            }
        }

        if (!(p->dash_cycles >= 0))
            continue; // no dashing

        MyPVector player_rel = {p->pos.x - tackler->pos.x, p->pos.y - tackler->pos.y};
        if (r2(&player_rel) > ball_dist2)
            continue; // further than ball

        rotate(&player_rel, -ball_angle);
        if (player_rel.x < 0.0 || fabsf(player_rel.y) > p->size + tackler->size)
            continue;

        float body_diff = fabsf(normalize_angle(p->angle_body_committed - ball_angle));
        if (body_diff > M_PIf * 0.5f)
            continue;

        if (foul) {
            if (pre_check) {
                yellow_card = true;
                if (brand(stadium->seed, RED_CARD_PROBABILITY)) {
                    yellow_card = false;
                    red_card = true;
                }
            }
        }
        else {
            if (brand(stadium->seed, tackler->player_type.foul_detect_probability)) {
                foul_charge = true;
                if (brand(stadium->seed, RED_CARD_PROBABILITY))
                    yellow_card = true;
            }
        }
    }

    *detect_charge = foul_charge;
    *detect_yellow = yellow_card;
    *detect_red = red_card;
}

void ref_clearPlayersFromBall(MyStadiumV2 *stadium, const Side side) {
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
            && ref_inPenaltyArea(LEFT, &stadium->ball.pos))
        || ((pm == PM_Foul_Charge_Right
            || pm == PM_Foul_Push_Right)
            && ref_inPenaltyArea(RIGHT, &stadium->ball.pos)))
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
        = (fabsf(stadium->ball.pos.x) > max_x - clear_dist && fabsf(stadium->ball.pos.y) > max_y - clear_dist);

    for (int loop = 0; loop < 10; ++loop) {
        bool exist = false;
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            MyPlayerV2 *p = &stadium->players[i];
            if (!p->enable) continue;
            if (side == NEUTRAL || p->side == side) {
                if (indirect && fabsf(p->pos.x) >= PITCH_LENGTH * 0.5f)
                    // defender is allowed to stand on the goal line.
                    continue;

                MyPVector clear_area_center = stadium->ball.pos;
                float clear_area_radius = clear_dist + p->size;
                if (distance(&clear_area_center, &p->pos) <= clear_area_radius) {
                    MyPVector expand_clear_area_center = stadium->ball.pos;
                    float expand_clear_area_radius = clear_dist + p->size + 1.0e-5f;
                    MyPVector new_pos;
                    nearestEdgeCircle(&expand_clear_area_center, expand_clear_area_radius, &p->pos, &new_pos);
                    if (ball_at_corner && fabsf(new_pos.x) > PITCH_LENGTH * 0.5f && fabsf(new_pos.y) > PITCH_WIDTH * 0.5f) {
                        new_pos.x -= stadium->ball.pos.x;
                        new_pos.y -= stadium->ball.pos.y;
                        rotate(&new_pos, M_PIf);
                        new_pos.x += stadium->ball.pos.x;
                        new_pos.y += stadium->ball.pos.y;
                    }

                    if (indirect && fabsf(new_pos.x) > PITCH_LENGTH * 0.5f) {
                        float tangent = (new_pos.y - stadium->ball.pos.y) / (new_pos.x - stadium->ball.pos.x);
                        new_pos.x = PITCH_LENGTH * 0.5f * (new_pos.x > 0.0f ? 1.0f : -1.0f);
                        new_pos.y = stadium->ball.pos.y + tangent * (new_pos.x - stadium->ball.pos.x);
                    }

                    if (fabsf(new_pos.x) > max_x) {
                        float r = clear_dist + p->size;
                        float theta = acosf((max_x - fabsf(stadium->ball.pos.x)) / r);
                        float tmp_y = fabsf(r * sinf(theta));
                        new_pos.x = new_pos.x < 0.0f ? -max_x : +max_x;
                        new_pos.y = stadium->ball.pos.y + 
                            (new_pos.y < stadium->ball.pos.y ? - tmp_y - 1.0e-5f : + tmp_y + 1.0e-5f);
                    }

                    if (fabsf(new_pos.y) > max_y) {
                        float r = clear_dist + p->size;
                        float theta = acosf((max_y - fabsf(stadium->ball.pos.y)) / r);
                        float tmp_x = fabsf(r * sinf(theta));
                        new_pos.x = stadium->ball.pos.x + 
                            (new_pos.x < stadium->ball.pos.x ? - tmp_x - 1.0e-5f : + tmp_x + 1.0e-5f);
                        new_pos.y = new_pos.y < 0.0 ? -max_y : +max_y;
                    }

                    if (fabsf(new_pos.x) > max_x || fabsf(new_pos.y) > max_y) {
                        new_pos.x -= stadium->ball.pos.x;
                        new_pos.y -= stadium->ball.pos.y;
                        rotate(&new_pos, M_PIf);
                        new_pos.x += stadium->ball.pos.x;
                        new_pos.y += stadium->ball.pos.y;
                    }

                    p->pos.x = new_pos.x;
                    p->pos.y = new_pos.y;
                    p->vel.x = 0.0f;
                    p->vel.y = 0.0f;
                    p->accel.x = 0.0f;
                    p->accel.y = 0.0f;
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

bool ref_isPenaltyShootOut(const PlayMode pm, const Side side) {
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

void offside_ref_playModeChange(MyStadiumV2 *stadium, PlayMode pm) {
    if (pm != PM_PlayOn)
        stadium->offside_ref_offside_candidates_size = 0;
}

bool free_kick_ref_goalKick(PlayMode pm) {
    return (pm == PM_GoalKick_Right || pm == PM_GoalKick_Left);
}

bool free_kick_ref_freeKick(PlayMode pm) {
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

void free_kick_ref_playModeChange(MyStadiumV2 *stadium, PlayMode pm) {
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

bool touch_ref_indirectFreeKick(const PlayMode pm){
    switch(pm) {
    case PM_IndFreeKick_Right:
    case PM_IndFreeKick_Left:
        return true;
    default:
        return false;
    }
}

void touch_ref_playModeChange(MyStadiumV2 *stadium, PlayMode pm) {
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

void catch_ref_playModeChange(MyStadiumV2 *stadium, PlayMode pm) {
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

void foul_ref_playModeChange(MyStadiumV2 *stadium, PlayMode pm) {
    if (pm == PM_Foul_Charge_Left
        || pm == PM_Foul_Charge_Right
        || pm == PM_Foul_Push_Left
        || pm == PM_Foul_Push_Right) {
        stadium->foul_ref_after_foul_time = 0;
    }
}

void penalty_ref_playModeChange(MyStadiumV2 *stadium, PlayMode pm) {
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

void ref_playModeChange(MyStadiumV2 *stadium, PlayMode pm) {
    offside_ref_playModeChange(stadium, pm);
    free_kick_ref_playModeChange(stadium, pm);
    touch_ref_playModeChange(stadium, pm);
    catch_ref_playModeChange(stadium, pm);
    foul_ref_playModeChange(stadium, pm);
    penalty_ref_playModeChange(stadium, pm);
}

void ref_placePlayersInTheirField(MyStadiumV2 *stadium) {
    const bool kick_off_offside = (KICK_OFF_OFFSIDE && (stadium->playmode == PM_KickOff_Left || stadium->playmode == PM_KickOff_Right));
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        MyPlayerV2 *player = &stadium->players[i];
        if (!player->enable) continue;
        switch (player->side) {
        case LEFT:
            if (player->pos.x > 0) {
                if (kick_off_offside) {
                    player->pos.x = -player->size;
                    player->pos.y = player->pos.y;
                }
                else {
                    player->pos.x = drand(stadium->seed, -PITCH_LENGTH/2.0f, 0.0f);
                    player->pos.y = drand(stadium->seed, -PITCH_WIDTH/2.0f, PITCH_WIDTH/2.0f);
                }
            }
            break;
        case RIGHT:
            if (player->pos.x < 0) {
                if (kick_off_offside) {
                    player->pos.x = player->size;
                    player->pos.y = player->pos.y;
                }
                else {
                    player->pos.x = drand(stadium->seed, 0.0f, PITCH_LENGTH/2.0f);
                    player->pos.y = drand(stadium->seed, -PITCH_WIDTH/2.0f, PITCH_WIDTH/2.0f);
                }
            }
            break;
        case NEUTRAL:
        default:
            break;
        }

        if (player->side != stadium->kick_off_side) {
            MyPVector expand_c_center = {0.0f, 0.0f};
            float expand_c_radius = KICK_OFF_CLEAR_DISTANCE + player->size;
            if (distance(&expand_c_center, &player->pos) <= expand_c_radius)
                nearestEdgeCircle(&expand_c_center, expand_c_radius, &player->pos, &player->pos);
        }
    }
}

void ref_truncateToPitch(MyPVector *pos) {
    pos->x = fminf( pos->x, +PITCH_LENGTH * 0.5f);
    pos->x = fmaxf( pos->x, -PITCH_LENGTH * 0.5f);
    pos->y = fminf( pos->y, +PITCH_WIDTH * 0.5f);
    pos->y = fmaxf( pos->y, -PITCH_WIDTH * 0.5f);
}

void ref_moveOutOfPenalty(const Side side, MyPVector *pos) {
    if (side != RIGHT) {
        if (pos->x <= (-PITCH_LENGTH * 0.5f +PENALTY_AREA_LENGTH) && fabsf(pos->y) <= PENALTY_AREA_WIDTH * 0.5f) {
            pos->x = add_eps(-PITCH_LENGTH * 0.5f + PENALTY_AREA_LENGTH);
            if (pos->y > 0)
                pos->y = add_eps(+PENALTY_AREA_WIDTH * 0.5f);
            else
                pos->y = subtract_eps(-PENALTY_AREA_WIDTH * 0.5f);
        }
    }

    if (side != LEFT) {
        if (pos->x >= (PITCH_LENGTH * 0.5 - PENALTY_AREA_LENGTH) && fabsf(pos->y) <= PENALTY_AREA_WIDTH * 0.5) {
            pos->x = subtract_eps(PITCH_LENGTH * 0.5f - PENALTY_AREA_LENGTH);
            if(pos->y > 0)
                pos->y = add_eps(+PENALTY_AREA_WIDTH * 0.5f);
            else
                pos->y = subtract_eps(-PENALTY_AREA_WIDTH * 0.5f);
        }
    }
}

void ref_moveOutOfGoalArea(const Side side, MyPVector *pos) {
    if ( side != RIGHT )
    {
        if ( pos->x <= ( - PITCH_LENGTH*0.5
                             + GOAL_AREA_LENGTH )
             && fabsf( pos->y ) <= GOAL_AREA_WIDTH *0.5 )
        {
            pos->x = add_eps(-PITCH_LENGTH * 0.5f + GOAL_AREA_LENGTH);
        }
    }

    if ( side != LEFT )
    {
        if ( pos->x >= ( PITCH_LENGTH*0.5
                             - GOAL_AREA_LENGTH )
             && fabsf(pos->y ) <= GOAL_AREA_WIDTH *0.5 )
        {
            pos->x = subtract_eps(PITCH_LENGTH * 0.5f - GOAL_AREA_LENGTH);
        }
    }
}

void ref_awardDropBall(MyStadiumV2 *stadium, MyPVector *pos) {
    stadium->ball_catcher_index = -1;
    ref_truncateToPitch(pos);
    ref_moveOutOfPenalty(NEUTRAL, pos);

    ref_placeBallAndChangePlayMode(stadium, PM_Drop_Ball, NEUTRAL, pos);
    placePlayersInField(stadium);

    if (!ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        changePlayMode(stadium, PM_PlayOn);
}

void ref_awardFreeKick(MyStadiumV2 *stadium, const Side side, MyPVector *pos) {
    ref_truncateToPitch(pos);
    ref_moveOutOfPenalty((Side)(-side), pos);

    if (side == LEFT)
        ref_placeBallAndChangePlayMode(stadium, PM_FreeKick_Left, LEFT, pos);
    else if(side == RIGHT)
        ref_placeBallAndChangePlayMode(stadium, PM_FreeKick_Right, RIGHT, pos);
}

void ref_awardGoalKick(MyStadiumV2 *stadium, const Side side, MyPVector *pos) {
    if (pos->y > 0.0f)
        pos->y = GOAL_AREA_WIDTH * 0.5f;
    else
        pos->y = -GOAL_AREA_WIDTH * 0.5f;

    stadium->ball_catcher_index = -1;
    if (side == LEFT) {
        pos->x = - PITCH_LENGTH * 0.5f + GOAL_AREA_LENGTH;
        ref_placeBallAndChangePlayMode(stadium, PM_GoalKick_Left, LEFT, pos);
    }
    else {
        pos->x = PITCH_LENGTH * 0.5f - GOAL_AREA_LENGTH;
        ref_placeBallAndChangePlayMode(stadium, PM_GoalKick_Right, RIGHT, pos);
    }
}

void time_ref_analyse(MyStadiumV2 *stadium) {
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

void ball_stuck_ref_analyse(MyStadiumV2 *stadium) {
    if (BALL_STUCK_AREA <= 0.0f || DROP_BALL_TIME <= 0)
        return;

    if (stadium->playmode != PM_PlayOn) {
        stadium->ball_stuck_ref_last_ball_pos.x = stadium->ball.pos.x;
        stadium->ball_stuck_ref_last_ball_pos.y = stadium->ball.pos.y;
        stadium->ball_stuck_ref_counter = 0;
        return;
    }

    if (distance2(&stadium->ball.pos, &stadium->ball_stuck_ref_last_ball_pos) <= powf(BALL_STUCK_AREA, 2.0f)) {
        stadium->ball_stuck_ref_counter += 1;
        if (stadium->ball_stuck_ref_counter >= DROP_BALL_TIME) {
            stadium->ball_stuck_ref_last_ball_pos.x = stadium->ball.pos.x;
            stadium->ball_stuck_ref_last_ball_pos.y = stadium->ball.pos.y;
            stadium->ball_stuck_ref_counter = 0;
            ref_awardDropBall(stadium, &stadium->ball.pos);
        }
    }
    else {
        stadium->ball_stuck_ref_last_ball_pos.x = stadium->ball.pos.x;
        stadium->ball_stuck_ref_last_ball_pos.y = stadium->ball.pos.y;
        stadium->ball_stuck_ref_counter = 0;
    }
}

void offside_ref_checkPlayerAfterOffside(MyStadiumV2 *stadium) {
    Side offsideside = NEUTRAL;
    MyPVector center = {0.0f, 0.0f}, size = {0.0f, 0.0f};
    if (stadium->playmode == PM_OffSide_Right) {
        center.x = +PITCH_LENGTH / 4 + stadium->offside_ref_offside_pos.x / 2;
        size.x = PITCH_LENGTH / 2 - stadium->offside_ref_offside_pos.x;
        size.y = PITCH_WIDTH;
        offsideside = RIGHT;
    }
    else if (stadium->playmode == PM_OffSide_Left) {
        center.x = -PITCH_LENGTH / 4 + stadium->offside_ref_offside_pos.x / 2;
        size.x = PITCH_LENGTH / 2 + stadium->offside_ref_offside_pos.x;
        size.y = PITCH_WIDTH;
        offsideside = LEFT;
    }
    else
        return;

    const MyPVector c_center = stadium->offside_ref_offside_pos;
    const float c_radius = 2.5f;
    const float left = center.x - size.x / 2.0f;
    const float right = center.x + size.x / 2.0f;
    const float top = center.y + size.y / 2.0f;
    const float bottom = center.y - size.y / 2.0f;
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        MyPlayerV2 *p = &stadium->players[i];
        if (!p->enable) continue;
        if ( p->side != offsideside ) continue;

        if (distance(&c_center, &p->pos) <= c_radius)
            nearestEdgeCircle(&c_center, c_radius, &p->pos, &p->pos);

        if (!inArea(left, right, top, bottom, &p->pos)) {
            MyPVector new_pos;
            nearestVEdge(left, right, top, bottom, &p->pos, &new_pos);
            if (stadium->playmode == PM_OffSide_Right )
                new_pos.x += OFFSIDE_KICK_MARGIN;
            else
                new_pos.x -= OFFSIDE_KICK_MARGIN;
            p->pos.x = new_pos.x;
            p->pos.y = new_pos.y;
        }
    }
}

void offside_ref_analyse(MyStadiumV2 *stadium) {
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

void free_kick_ref_placePlayersForGoalkick(MyStadiumV2 *stadium) {
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
        MyPlayerV2 *p = &stadium->players[i];
        if (!p->enable) continue;
        if (p->side == oppside) {
            const float size = p->size;
            float expand_area_left = p_area_left - size;
            float expand_area_right = p_area_right + size;
            float expand_area_top = p_area_top - size;
            float expand_area_bottom = p_area_bottom + size;

            if (inArea(expand_area_left, expand_area_right, expand_area_top, expand_area_bottom, &p->pos)) {
                MyPVector new_pos;
                nearestEdge(expand_area_left, expand_area_right, expand_area_top, expand_area_bottom, 
                    &p->pos, &new_pos);
                if (new_pos.x * oppside >= PITCH_LENGTH / 2.0f)
                    new_pos.x = (PITCH_LENGTH / 2.0f - PENALTY_AREA_LENGTH - size) * oppside;

                p->pos.x = new_pos.x;
                p->pos.y = new_pos.y;
            }
        }
    }
}

void free_kick_ref_analyse(MyStadiumV2 *stadium) {
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
        if (!ref_inPenaltyArea(NEUTRAL, &stadium->ball.pos))
            changePlayMode(stadium, PM_PlayOn);
        else {
            if (stadium->free_kick_ref_kick_taken && PROPER_GOAL_KICKS) {
                if (r(&stadium->ball.vel) < STOPPED_BALL_VEL) {
                    if (stadium->free_kick_ref_goal_kick_count >= MAX_GOAL_KICKS)
                        ref_awardFreeKick(stadium, (Side)(-stadium->players[stadium->free_kick_ref_kick_taker_index].side), &stadium->ball.pos);
                    else
                        ref_awardGoalKick(stadium, stadium->players[stadium->free_kick_ref_kick_taker_index].side, &stadium->ball.pos);
                }
            }
            else {
                if (stadium->free_kick_ref_timer > -1)
                    stadium->free_kick_ref_timer--;

                if (stadium->free_kick_ref_timer == 0)
                    ref_awardDropBall(stadium, &stadium->ball.pos);
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
        if (stadium->ball.vel.x != 0.0f || stadium->ball.vel.y != 0.0f)
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
            ref_awardDropBall(stadium, &stadium->ball.pos);
    }
}

bool ref_crossGoalLine(MyStadiumV2 *stadium, const Side side, const MyPVector *prev_ball_pos) {
    if (prev_ball_pos->x == stadium->ball.pos.x)
        // ball cannot have crossed gline
        return false;

    if (fabsf(stadium->ball.pos.x) <= PITCH_LENGTH * 0.5f + BALL_SIZE)
        // ball hasn't crossed gline
        return false;

    if (fabsf( prev_ball_pos->x) > PITCH_LENGTH * 0.5f + BALL_SIZE)
        // ball already over the gline
        return false;

    if ((side * stadium->ball.pos.x) >= 0.0f)
        //ball in wrong half
        return false;

    if (fabsf(prev_ball_pos->y) > (GOAL_WIDTH * 0.5f + GOAL_POST_RADIUS)
        && fabsf(prev_ball_pos->x) > PITCH_LENGTH * 0.5f)
        // then the only goal that could have been scored would be
        // from going behind the goal post.  I'm pretty sure that
        // isn't possible anyway, but just in case this function acts
        // as a double check
        return false;

    float delta_x = stadium->ball.pos.x - prev_ball_pos->x;
    float delta_y = stadium->ball.pos.y - prev_ball_pos->y;

    // we already checked above that ball.pos.x != prev_ball_pos.x, so delta_x cannot be zero.
    float gradient = delta_y / delta_x;
    float offset = prev_ball_pos->y - gradient * prev_ball_pos->x;

    // determine y for x = ServerParam::PITCH_LENGTH*0.5 + ServerParam::instance().ballSize() * -side
    float x = (PITCH_LENGTH * 0.5f + BALL_SIZE) * -side;
    float y_intercept = gradient * x + offset;

    return fabsf(y_intercept) <= (GOAL_WIDTH * 0.5f + GOAL_POST_RADIUS);
}

bool touch_ref_checkGoal(MyStadiumV2 *stadium) {
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

    if (fabsf(stadium->ball.pos.x) <= PITCH_LENGTH * 0.5 + BALL_SIZE)
        return false;

    if ((stadium->ball_catcher_index == -1 || stadium->players[stadium->ball_catcher_index].side == LEFT)
        && ref_crossGoalLine(stadium, LEFT, &stadium->touch_ref_prev_ball_pos)
        && !ref_isPenaltyShootOut(stadium->playmode, NEUTRAL)) {
        score(stadium, RIGHT);
        // mydiff
        // announceGoal( M_stadium.teamRight() );
        stadium->touch_ref_after_goal_time = 0;
        placeBall(stadium, LEFT, &stadium->ball.pos);
        if (HALF_TIME >= 0 && GOLDEN_GOAL && stadium->time >= HALF_TIME * NR_NORMAL_HALFS)
            changePlayMode(stadium, PM_TimeOver);
        else
            changePlayMode(stadium, PM_AfterGoal_Right);
        return true;
    }
    else if ((stadium->ball_catcher_index == -1 || stadium->players[stadium->ball_catcher_index].side == RIGHT)
        && ref_crossGoalLine(stadium, RIGHT, &stadium->touch_ref_prev_ball_pos)
        && ! ref_isPenaltyShootOut(stadium->playmode, NEUTRAL)) {
        score(stadium, LEFT);
        // mydiff
        // announceGoal( M_stadium.teamLeft() );
        stadium->touch_ref_after_goal_time = 0;
        placeBall(stadium, RIGHT, &stadium->ball.pos);
        if (HALF_TIME >= 0 && GOLDEN_GOAL && stadium->time >= HALF_TIME * NR_NORMAL_HALFS)
            changePlayMode(stadium, PM_TimeOver);
        else
            changePlayMode(stadium, PM_AfterGoal_Left);
        return true;
    }

    return false;
}

void ref_awardCornerKick(MyStadiumV2 *stadium, const Side side, MyPVector *pos) {
    stadium->ball_catcher_index = -1;
    if (pos->y > 0)
        pos->y = PITCH_WIDTH * 0.5f - CORNER_KICK_MARGIN;
    else
        pos->y = -PITCH_WIDTH * 0.5f + CORNER_KICK_MARGIN;

    if (side == LEFT) {
        pos->x = PITCH_LENGTH * 0.5f - CORNER_KICK_MARGIN;
        ref_placeBallAndChangePlayMode(stadium, PM_CornerKick_Left, LEFT, pos);
    }
    else {
        pos->x = -PITCH_LENGTH * 0.5f + CORNER_KICK_MARGIN;
        ref_placeBallAndChangePlayMode(stadium, PM_CornerKick_Right, RIGHT, pos);
    }
}

void ref_awardKickIn(MyStadiumV2 *stadium, const Side side, MyPVector *pos) {
    stadium->ball_catcher_index = -1;
    ref_truncateToPitch(pos);
    if (side == LEFT)
        ref_placeBallAndChangePlayMode(stadium, PM_KickIn_Left, LEFT, pos);
    else
        ref_placeBallAndChangePlayMode(stadium, PM_KickIn_Right, RIGHT, pos);
}

void touch_ref_analyse(MyStadiumV2 *stadium) {
    if (ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    if (stadium->playmode == PM_AfterGoal_Left ) {
        if ( ++stadium->touch_ref_after_goal_time > AFTER_GOAL_WAIT ) {
            MyPVector pos = {0.0f, 0.0f};
            ref_placeBallAndChangePlayMode(stadium, PM_KickOff_Right, RIGHT, &pos);
            ref_placePlayersInTheirField(stadium);
        }
        return;
    }

    if (stadium->playmode == PM_AfterGoal_Right ) {
        if ( ++stadium->touch_ref_after_goal_time > AFTER_GOAL_WAIT ) {
            MyPVector pos = {0.0f, 0.0f};
            ref_placeBallAndChangePlayMode(stadium, PM_KickOff_Left, LEFT, &pos);
            ref_placePlayersInTheirField(stadium);
        }
        return;
    }

    if (touch_ref_checkGoal(stadium))
        return;

    if (stadium->playmode != PM_AfterGoal_Left
        && stadium->playmode != PM_AfterGoal_Right
        && stadium->playmode != PM_TimeOver) {
        if (fabsf(stadium->ball.pos.x) > PITCH_LENGTH * 0.5f + BALL_SIZE) {
            // check for goal kick or corner kick
            Side side = NEUTRAL;
            if (stadium->touch_ref_last_touched_index != -1)
                side = stadium->players[stadium->touch_ref_last_touched_index].side;

            if (stadium->ball.pos.x > PITCH_LENGTH * 0.5f + BALL_SIZE) {
                if (side == RIGHT)
                    ref_awardCornerKick(stadium, LEFT, &stadium->ball.pos);
                else if (stadium->ball_catcher_index == -1)
                    ref_awardGoalKick(stadium, RIGHT, &stadium->ball.pos);
                else
                    // the ball is caught and the goalie must have
                    // moved to a position beyond the opponents goal
                    // line.  Let the catch ref clean up the mess
                    return;
            }
            else if (stadium->ball.pos.x < PITCH_LENGTH * 0.5f - BALL_SIZE) {
                if (side == LEFT)
                    ref_awardCornerKick(stadium, RIGHT, &stadium->ball.pos);
                else if (stadium->ball_catcher_index == -1)
                    ref_awardGoalKick(stadium, LEFT, &stadium->ball.pos);
                else
                    // the ball is caught and the goalie must have
                    // moved to a position beyond the opponents goal
                    // line.  Let the catch ref clean up the mess
                    return;
            }
        }
        else if (fabsf(stadium->ball.pos.y) > PITCH_WIDTH * 0.5f + BALL_SIZE) {
            // check for kick in.
            Side side = NEUTRAL;
            if (stadium->touch_ref_last_touched_index != -1)
                side = stadium->players[stadium->touch_ref_last_touched_index].side;

            if (side == NEUTRAL)
                // somethings gone wrong but give a drop ball
                ref_awardDropBall(stadium, &stadium->ball.pos);
            else
                ref_awardKickIn(stadium, (Side)(-side), &stadium->ball.pos);
        }
    }

    stadium->touch_ref_prev_ball_pos = stadium->ball.pos;
}

void catch_ref_callCatchFault(MyStadiumV2 *stadium, Side side, MyPVector *pos ) {
    ref_truncateToPitch( pos );
    //pos = moveIntoPenalty( side, pos );
    ref_moveOutOfPenalty( side, pos );

    stadium->ball_catcher_index = -1;

    if ( side == LEFT )
    {
        ref_placeBallAndChangePlayMode(stadium, PM_CatchFault_Left, RIGHT, pos );
    }
    else if ( side == RIGHT )
    {
        ref_placeBallAndChangePlayMode(stadium, PM_CatchFault_Right, LEFT, pos );
    }

    stadium->catch_ref_after_catch_fault_time = 0;
}

void catch_ref_analyse(MyStadiumV2 *stadium) {
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


    if ( stadium->ball_catcher_index != -1
         && pm != PM_AfterGoal_Left
         && pm != PM_AfterGoal_Right
         && pm != PM_TimeOver
         && ! ref_inPenaltyArea( stadium->players[stadium->ball_catcher_index].side,
                             &stadium->ball.pos ) )
    {
        catch_ref_callCatchFault(stadium, stadium->players[stadium->ball_catcher_index].side,
                        &stadium->ball.pos );
    }
}

void foul_ref_analyse(MyStadiumV2 *stadium) {
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
            if ( ref_inPenaltyArea( LEFT, &stadium->ball.pos ) )
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
            if ( ref_inPenaltyArea( RIGHT, &stadium->ball.pos ) )
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

void penalty_ref_penalty_init(MyStadiumV2 *stadium) {
    // change the play mode such that the other side can take the penalty
    // and place the ball at the penalty spot
    stadium->penalty_ref_cur_pen_taker = ( stadium->penalty_ref_cur_pen_taker == LEFT
                        ? RIGHT
                        : LEFT );
    PlayMode pm = ( stadium->penalty_ref_cur_pen_taker == LEFT
                    ? PM_PenaltySetup_Left
                    : PM_PenaltySetup_Right );
    MyPVector pos = {- stadium->penalty_ref_pen_side * ( PITCH_LENGTH/2 - PEN_DIST_X), 0.0};
    ref_placeBallAndChangePlayMode(stadium, pm, NEUTRAL, &pos);
}

void penalty_ref_startPenaltyShootout(MyStadiumV2 *stadium) {
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
        if ( drand(stadium->seed, 0, 1 ) < 0.5 )       // choose random side of the playfield
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
        stadium->penalty_ref_cur_pen_taker = ( drand(stadium->seed, 0, 1 ) < 0.5 ) ? LEFT : RIGHT;

        // place the goalkeeper of the opposite field close to the penalty goal
        // otherwise it is hard to get there before pen_setup_wait cycles
        Side side = ( stadium->penalty_ref_pen_side == LEFT ) ? RIGHT : LEFT;
        for (int i = 0 ; i < NUM_PLAYERS ; i++)
        {
            MyPlayerV2 *p = &stadium->players[i];
            if (p->enable && p->side == side && p->goalie )
            {
                p->pos.x = -stadium->penalty_ref_pen_side * (PITCH_LENGTH/2-10);
                p->pos.y = 10;
            }
        }

        penalty_ref_penalty_init(stadium);
        stadium->penalty_ref_first_time = false;
    }
}

bool penalty_ref_penalty_check_players(MyStadiumV2 *stadium, const Side side ) {
    PlayMode pm = stadium->playmode;
    int     iOutsideCircle = 0;
    bool    bCheck         = true;
    MyPVector posGoalie;
    //int     iPlayerOutside = -1, iGoalieNr=-1;
    const MyPlayerV2 * outside_player = NULL;
    const MyPlayerV2 * goalie = NULL;

    if ( pm == PM_PenaltyMiss_Left  || pm == PM_PenaltyMiss_Right
         || pm == PM_PenaltyScore_Left || pm == PM_PenaltyScore_Right )
    {
        return true;
    }

    // for all players from side 'side' get the goalie pos and count how many
    // players are outside the center circle.
    for (int i = 0 ; i < NUM_PLAYERS ; i++)
    {
        MyPlayerV2 *p = &stadium->players[i];
        if (!p->enable) continue;

        if ( p->side == side )
        {
            if ( p->goalie )
            {
                goalie = p;
                posGoalie = p->pos;
                continue;
            }

            MyPVector c_center = {0.0, 0.0 };
            float c_radius = KICK_OFF_CLEAR_DISTANCE - p->size;
            if ( ! (distance(&c_center, &p->pos) <= c_radius) )
            {
                iOutsideCircle++;
                outside_player = p;
            }
        }
    }

    if ( ! goalie )
    {
        return false;
    }

    // if the 'side' equals the one that takes the penalty shoot out
    if ( side == stadium->penalty_ref_cur_pen_taker )
    {
        // in case that goalie takes penalty kick
        // or taker goes into the center circle
        if ( iOutsideCircle == 0 )
        {
            if ( pm == PM_PenaltySetup_Left || pm == PM_PenaltySetup_Right )
            {
                if ( distance(&goalie->pos, &stadium->ball.pos) > 2.0 )
                {
                    // bCheck = false;
                }
                else
                {
                    outside_player = goalie;
                }
            }
        }
        // if goalie not outside field, check fails
        else if ( fabsf( posGoalie.x ) < PITCH_LENGTH/2.0 - 1.5
                  || fabsf( posGoalie.y ) < PENALTY_AREA_WIDTH/2.0 - 1.5 )
        {
            bCheck = false;
        }
        // only one should be outside the circle -> player that takes penalty
        else if ( iOutsideCircle > 1 )
        {
            bCheck = false;
        }
        // in setup, player outside circle should be close to ball
        else if ( ( pm == PM_PenaltySetup_Left || pm == PM_PenaltySetup_Right )
                  && iOutsideCircle == 1 )
        {
            if ( outside_player
                 && distance(&outside_player->pos, &stadium->ball.pos ) > 2.0 )
            {
                bCheck = false;
            }
        }
    }
    else //other team
    {
        // goalie does not stand in front of goal line
        if ( stadium->playmode != PM_PenaltyTaken_Left
             && stadium->playmode != PM_PenaltyTaken_Right )
        {
            if ( fabsf( posGoalie.x )
                 < PITCH_LENGTH/2.0 - PEN_MAX_GOALIE_DIST_X
                 || fabsf( posGoalie.y )
                 > GOAL_WIDTH*0.5 )
            {
                bCheck = false;
            }
        }
        // when receiving the penalty every player should be in center circle
        if ( iOutsideCircle != 0 )
        {
            bCheck = false;
        }
    }

    if (bCheck && outside_player) {
        // if in setup and already in set -> check fails
        if ((side == LEFT && stadium->playmode == PM_PenaltySetup_Left
            && contain(stadium->penalty_ref_sLeftPenTaken, stadium->penalty_ref_sLeftPenTaken_size, 
            outside_player->unum))
            || (side == RIGHT && stadium->playmode == PM_PenaltySetup_Right
            && contain(stadium->penalty_ref_sRightPenTaken, stadium->penalty_ref_sRightPenTaken_size, 
            outside_player->unum)))
            bCheck = false;
    }

    return bCheck;
}

const MyPlayerV2* penalty_ref_getCandidateTaker(MyStadiumV2 *stadium) {
    const int *sPenTaken = stadium->penalty_ref_cur_pen_taker == LEFT
        ? stadium->penalty_ref_sLeftPenTaken
        : stadium->penalty_ref_sRightPenTaken;

    const int sPenTaken_size = stadium->penalty_ref_cur_pen_taker == LEFT
        ? stadium->penalty_ref_sLeftPenTaken_size
        : stadium->penalty_ref_sRightPenTaken_size;

    const MyPlayerV2 * candidate = NULL;
    const MyPlayerV2 * goalie = NULL;
    float min_dist2 = FLT_MAX;

    // first find the closest player to the ball
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        MyPlayerV2 *p = &stadium->players[i];
        if (!p->enable) continue;
        if (p->side != stadium->penalty_ref_cur_pen_taker) 
            continue;

        if (contain(sPenTaken, sPenTaken_size, p->unum))
            // players that have already taken a kick cannot be
            // counted as a potential kicker.
            continue;

        if (p->goalie) {
            goalie = p;
            continue;
        }

        float d2 = distance2(&p->pos, &stadium->ball.pos);
        if(d2 < min_dist2) {
            min_dist2 = d2;
            candidate = p;
        }
    }

    if (!candidate)
        return goalie;

    return candidate;
}

void penalty_ref_placeTakerTeamPlayers(MyStadiumV2 *stadium) {
    const bool bPenTaken = ( stadium->playmode == PM_PenaltyTaken_Right
                             || stadium->playmode == PM_PenaltyTaken_Left );

    const MyPlayerV2 * taker = ( stadium->penalty_ref_last_taker_index != -1
                             ? &stadium->players[stadium->penalty_ref_last_taker_index]
                             : penalty_ref_getCandidateTaker(stadium) );

    const MyPVector goalie_wait_pos_b = {-stadium->penalty_ref_pen_side * ( PITCH_LENGTH / 2.0f + 2.0f ), +25.0 };
    const MyPVector goalie_wait_pos_t = {-stadium->penalty_ref_pen_side * ( PITCH_LENGTH / 2.0f + 2.0f ), -25.0 };

    // then replace the players from the specified side
    for (int i = 0 ; i < NUM_PLAYERS ; i++)
    {
        MyPlayerV2 *p = &stadium->players[i];
        if (!p->enable) continue;

        if ( p->side != stadium->penalty_ref_cur_pen_taker ) continue;

        if ( p == taker )
        {
            if ( ! bPenTaken
                 && distance(&taker->pos, &stadium->ball.pos ) > 2.0 )
            {
                MyPVector c_center = stadium->ball.pos;
                float c_radius = 2.0f;
                nearestEdgeCircle(&c_center, c_radius, &taker->pos, &p->pos);
            }
        }
        else
        {
            if ( p->goalie )
            {
                MyPVector c_center = p->pos.y > 0.0 ? goalie_wait_pos_b : goalie_wait_pos_t;
                float c_radius = 2.0f;
                if ( ! (distance(&c_center, &p->pos ) <= c_radius))
                {
                    nearestEdgeCircle(&c_center, c_radius, &p->pos, &p->pos);
                }
            }
            else // not goalie
            {
                MyPVector c_center = {0.0, 0.0 };
                float c_radius = KICK_OFF_CLEAR_DISTANCE - p->size;
                if ( ! (distance(&c_center, &p->pos ) <= c_radius) )
                {
                    nearestEdgeCircle(&c_center, c_radius, &p->pos, &p->pos);
                }
            }
        }
    }
}

void penalty_ref_placeOtherTeamPlayers(MyStadiumV2 *stadium) {
    const bool bPenTaken = ( stadium->playmode == PM_PenaltyTaken_Right
                             || stadium->playmode == PM_PenaltyTaken_Left );
    const double goalie_line
        = ( stadium->penalty_ref_pen_side == LEFT
            ? - PITCH_LENGTH/2.0 + PEN_MAX_GOALIE_DIST_X
            : + PITCH_LENGTH/2.0 - PEN_MAX_GOALIE_DIST_X );

    for (int i = 0 ; i < NUM_PLAYERS ; i++)
    {
        MyPlayerV2 *p = &stadium->players[i];
        if (!p->enable) continue;

        if ( p->side == stadium->penalty_ref_cur_pen_taker ) continue;

        // only move goalie in case the penalty has not been started yet.
        if ( p->goalie )
        {
            if ( ! bPenTaken )
            {
                if ( stadium->penalty_ref_pen_side == LEFT )
                {
                    if ( p->pos.x - goalie_line > 0.0 )
                    {
                        p->pos.x = goalie_line - 1.5;
                        p->pos.y =  0.0;
                    }
                }
                else
                {
                    if ( p->pos.x - goalie_line < 0.0 )
                    {
                        p->pos.x = goalie_line + 1.5;
                        p->pos.y =  0.0;
                    }
                }
            }
        }
        else // not goalie
        {
            MyPVector c_center = {0.0, 0.0};
            float c_radius = KICK_OFF_CLEAR_DISTANCE - p->size;
            
            if ( ! (distance(&c_center, &p->pos ) <= c_radius))
            {
                // place other players in circle in penalty area
                //p->moveTo( PVector::fromPolar( 6.5, Deg2Rad( i*15 ) ) );
                nearestEdgeCircle(&c_center, c_radius, &p->pos, &p->pos);
            }
        }
    }
}

void penalty_ref_penalty_place_all_players(MyStadiumV2 *stadium, const Side side ) {
    if (side == stadium->penalty_ref_cur_pen_taker)
        penalty_ref_placeTakerTeamPlayers(stadium);
    else // other team
        penalty_ref_placeOtherTeamPlayers(stadium);
}

void penalty_ref_penalty_check_score(MyStadiumV2 *stadium) {
    // if both players have taken nr_kicks and max_extra_kicks penalties -> quit
    if (stadium->penalty_ref_pen_nr_taken > 2 * (PEN_MAX_EXTRA_KICKS + PEN_NR_KICKS))
    {
        if (PEN_RANDOM_WINNER)
        {
            if ( drand(stadium->seed, 0, 1 ) < 0.5 )
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

void penalty_ref_penalty_score(MyStadiumV2 *stadium, Side side ) {
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

void penalty_ref_penalty_miss(MyStadiumV2 *stadium, Side side ) {
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

void penalty_ref_penalty_foul(MyStadiumV2 *stadium, const Side side ) {
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

void penalty_ref_handleTimeout(MyStadiumV2 *stadium, bool left_move_check, bool right_move_check) {
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

void penalty_ref_handleTimer(MyStadiumV2 *stadium, const bool left_move_check, const bool right_move_check) {
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
        placeBall(stadium, stadium->penalty_ref_cur_pen_taker, &stadium->ball.pos);

        return;
    }

    if ( left_move_check
         && right_move_check )
    {
        // if ball crossed goalline, process goal and set ball on goalline
        if ( ref_crossGoalLine(stadium, stadium->penalty_ref_pen_side, &stadium->penalty_ref_prev_ball_pos ) )
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
            placeBall(stadium, stadium->penalty_ref_pen_side, &stadium->ball.pos);
        }
        else if ( fabs( stadium->ball.pos.x )
                  > PITCH_LENGTH * 0.5
                  + BALL_SIZE
                  || fabs( stadium->ball.pos.y )
                  > PITCH_WIDTH * 0.5
                  + BALL_SIZE)
        {
            placeBall(stadium, stadium->penalty_ref_pen_side, &stadium->ball.pos);
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

void penalty_ref_analyse(MyStadiumV2 *stadium) {
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

    stadium->penalty_ref_prev_ball_pos = stadium->ball.pos;
}

void ref_analyse(MyStadiumV2 *stadium) {
    time_ref_analyse(stadium);
    ball_stuck_ref_analyse(stadium);
    offside_ref_analyse(stadium);
    free_kick_ref_analyse(stadium);
    touch_ref_analyse(stadium);
    catch_ref_analyse(stadium);
    foul_ref_analyse(stadium);
    // mydiff : illegal defense is disabled by defualt. need to check and see if it's also disabled in the tournaments.
    // illegal_defense_ref_analyse(stadium);
    // keepaway_ref_analyse(stadium);
    penalty_ref_analyse(stadium);
}

void offside_ref_callOffside(MyStadiumV2 *stadium) {
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

    MyPVector pos;

    if (stadium->offside_ref_offside_pos.x > PITCH_LENGTH/2.0
        || inArea(g_r_l, g_r_r, g_r_t, g_r_b, &stadium->offside_ref_offside_pos )) {
        pos.x = + PITCH_LENGTH/2.0 - GOAL_AREA_LENGTH;
        pos.y = ( stadium->offside_ref_offside_pos.y > 0 ? 1 : -1 ) * GOAL_AREA_WIDTH/2.0;
    }
    else if ( stadium->offside_ref_offside_pos.x < - PITCH_LENGTH/2.0
        || inArea(g_l_l, g_l_r, g_l_t, g_l_b, &stadium->offside_ref_offside_pos ) ) {
        pos.x = - PITCH_LENGTH/2.0 + GOAL_AREA_LENGTH;
        pos.y = ( stadium->offside_ref_offside_pos.y > 0 ? 1 : -1 ) * GOAL_AREA_WIDTH/2.0;
    }
    else if ( !inArea(pt_l, pt_r, pt_t, pt_b, &stadium->offside_ref_offside_pos ) )
        nearestEdge(pt_l, pt_r, pt_t, pt_b, &stadium->offside_ref_offside_pos, &pos);
    else
        pos = stadium->offside_ref_offside_pos;

    if ( stadium->offside_ref_last_kicker_side == LEFT )
    {
        ref_placeBallAndChangePlayMode(stadium, PM_OffSide_Left, RIGHT, &pos );
    }
    else
    {
        ref_placeBallAndChangePlayMode(stadium, PM_OffSide_Right, LEFT, &pos );
    }

    stadium->ball_catcher_index = -1;
    stadium->offside_ref_offside_candidates_size = 0;

    placePlayersInField(stadium);
    //clearPlayersFromBall( M_last_kicker_side );

    stadium->offside_ref_after_offside_time = 0;
}

void offside_ref_setOffsideMark(MyStadiumV2 *stadium, int kicker_index, const float accel_r) {
    const MyPlayerV2 *kicker = &stadium->players[kicker_index];
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
        stadium->offside_ref_last_kicker_side = kicker->side;
        stadium->offside_ref_last_kick_accel_r = accel_r;
    }

    for (int i = 0 ; i < stadium->offside_ref_offside_candidates_size ; i++) {
        const Candidate *c = &stadium->offside_ref_offside_candidates[i];
        if (c->player_index == kicker_index) {
            stadium->offside_ref_offside_pos = c->pos;
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

    switch ( kicker->side) {
    case LEFT:
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            MyPlayerV2 *p = &stadium->players[i];
            if (!p->enable) continue;

            if ( p->side == RIGHT )
            {
                if ( p->pos.x > second )
                {
                    second = p->pos.x;
                    if ( second > first )
                    {
                        float temp = first;
                        first = second;
                        second = temp;
                    }
                }
            }
        }

        if ( second > stadium->ball.pos.x )
        {
            offside_line = second;
        }
        else
        {
            offside_line = stadium->ball.pos.x;
        }

        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            MyPlayerV2 *p = &stadium->players[i];
            if (!p->enable) continue;

            if ( p->side == LEFT
                 && p->pos.x > offside_line
                 && p->unum != kicker->unum ) {
                int new_candidate_index = stadium->offside_ref_offside_candidates_size;
                Candidate *new_candidate = &stadium->offside_ref_offside_candidates[new_candidate_index];
                new_candidate->player_index = i;
                new_candidate->pos.x = offside_line;
                new_candidate->pos.y = p->pos.y;
                stadium->offside_ref_offside_candidates_size += 1;
            }
        }
        break;

    case RIGHT:
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            MyPlayerV2 *p = &stadium->players[i];
            if (!p->enable) continue;

            if ( p->side == LEFT )
            {
                if ( p->pos.x < second )
                {
                    second = p->pos.x;
                    if ( second < first )
                    {
                        float temp = first;
                        first = second;
                        second = temp;
                    }
                }
            }
        }

        if ( second < stadium->ball.pos.x )
        {
            offside_line = second;
        }
        else
        {
            offside_line = stadium->ball.pos.x;
        }

        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            MyPlayerV2 *p = &stadium->players[i];
            if (!p->enable) continue;

            if ( p->side == RIGHT
                 && p->pos.x < offside_line
                 && p->unum != kicker->unum )
            {
                int new_candidate_index = stadium->offside_ref_offside_candidates_size;
                Candidate *new_candidate = &stadium->offside_ref_offside_candidates[new_candidate_index];
                new_candidate->player_index = i;
                new_candidate->pos.x = offside_line;
                new_candidate->pos.y = p->pos.y;
                stadium->offside_ref_offside_candidates_size += 1;
            }
        }
        break;
    case NEUTRAL:
    default:
        break;
    }
}

void offside_ref_ballTouched(MyStadiumV2 *stadium, int player_index) {
    offside_ref_setOffsideMark(stadium, player_index, 0.0f);
}

void free_kick_ref_callFreeKickFault(MyStadiumV2 *stadium, Side side, MyPVector *pos ) {
    ref_truncateToPitch( pos );
    ref_moveOutOfPenalty( side, pos );

    stadium->ball_catcher_index = -1;

    if ( side == LEFT )
    {
        ref_placeBallAndChangePlayMode(stadium, PM_Free_Kick_Fault_Left, RIGHT, pos );
    }
    else if ( side == RIGHT )
    {
        ref_placeBallAndChangePlayMode(stadium, PM_Free_Kick_Fault_Right, LEFT, pos );
    }

    stadium->free_kick_ref_after_free_kick_fault_time = 0;
}

void free_kick_ref_ballTouched(MyStadiumV2 *stadium, int player_index) {
    MyPlayerV2 *player = &stadium->players[player_index];
    if ( ( stadium->playmode == PM_GoalKick_Left
           && player->side != LEFT )
         || ( stadium->playmode == PM_GoalKick_Right
              && player->side != RIGHT )
         )
    {
        // opponent player kicks tha ball while ball is in penalty area.
        ref_awardGoalKick(stadium, (Side)( - player->side ), &stadium->ball.pos );
        stadium->free_kick_ref_goal_kick_count = -1;
        stadium->free_kick_ref_kick_taken = false;
        return;
    }

    if ( stadium->free_kick_ref_kick_taken )
    {
        if ( player_index == stadium->free_kick_ref_kick_taker_index && FREE_KICK_FAULTS)
        {
            if ( stadium->players[stadium->free_kick_ref_kick_taker_index].dash_count > stadium->free_kick_ref_kick_taker_dashes )
            {
                setPlayerState(stadium, stadium->free_kick_ref_kick_taker_index, STATE_FREE_KICK_FAULT);
                free_kick_ref_callFreeKickFault(stadium, stadium->players[stadium->free_kick_ref_kick_taker_index].side,
                                   &stadium->ball.pos );
            }
            /// else do nothing yet as the player just colided with the ball instead of dashing into it
        }
        else
        {
            stadium->free_kick_ref_kick_taken = false;
        }
    }
}

void touch_ref_ballTouched(MyStadiumV2 *stadium, int kicker_index) {
    if ( fabsf( stadium->ball.pos.x )
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

void catch_ref_ballTouched(MyStadiumV2 *stadium, int player_index) {
    MyPlayerV2 *player = &stadium->players[player_index];
    // If ball is not kicked, back pass violation is never taken.

    if ( player->side == LEFT )
    {
        stadium->catch_ref_team_l_touched = true;
    }
    else if ( player->side == RIGHT )
    {
        stadium->catch_ref_team_r_touched = true;
    }

    if ( stadium->catch_ref_team_l_touched && stadium->catch_ref_team_r_touched )
    {
        stadium->catch_ref_before_last_back_passer_index = -1;
        stadium->catch_ref_last_back_passer_index = -1;
    }
}

void ref_ballTouched(MyStadiumV2 *stadium, int player_index) {
    offside_ref_ballTouched(stadium, player_index);
    free_kick_ref_ballTouched(stadium, player_index);
    touch_ref_ballTouched(stadium, player_index);
    catch_ref_ballTouched(stadium, player_index);
}

void offside_ref_checkIntentionalAction(MyStadiumV2 *stadium, int kicker_index) {
    if (!USE_OFFSIDE)
        return;

    for (int i = 0 ; i < stadium->offside_ref_offside_candidates_size ; i++) {
        const Candidate *c = &stadium->offside_ref_offside_candidates[i];
        if (c->player_index == kicker_index && distance2(&stadium->players[c->player_index].pos, &stadium->ball.pos) < powf(OFFSIDE_ACTIVE_AREA_SIZE, 2.0f)) {
            stadium->offside_ref_offside_pos = c->pos;
            offside_ref_callOffside(stadium);
        }
    }
}

void offside_ref_failedKickTaken(MyStadiumV2 *stadium, int kicker_index) {
    offside_ref_checkIntentionalAction(stadium, kicker_index);
}

void ref_failedKickTaken(MyStadiumV2 *stadium, int kicker_index) {
    offside_ref_failedKickTaken(stadium, kicker_index);
}

void offside_ref_kickTaken(MyStadiumV2 *stadium, int kicker_index, const float accel_r) {
    offside_ref_setOffsideMark(stadium, kicker_index, accel_r);
}

void free_kick_ref_kickTaken(MyStadiumV2 *stadium, int kicker_index, const float accel_r) {
    const MyPlayerV2 *kicker = &stadium->players[kicker_index];
    if (ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    if (free_kick_ref_goalKick(stadium->playmode)) {
        if ((stadium->playmode == PM_GoalKick_Left && kicker->side != LEFT)
            || (stadium->playmode == PM_GoalKick_Right && kicker->side != RIGHT)) {
            // opponent player kicks tha ball while ball is in penalty areas.
            ref_awardGoalKick(stadium, (Side)(-kicker->side), &stadium->ball.pos);
            stadium->free_kick_ref_goal_kick_count = -1;
            stadium->free_kick_ref_kick_taken = false;
            return;
        }

        if (stadium->free_kick_ref_kick_taken) {
            // ball was not kicked directly into play (i.e. out of penalty area
            // without touching another player
            if (kicker_index != stadium->free_kick_ref_kick_taker_index) {
                if (PROPER_GOAL_KICKS)
                    ref_awardGoalKick(stadium, stadium->players[stadium->free_kick_ref_kick_taker_index].side, &stadium->ball.pos);
            }
            else if (stadium->free_kick_ref_kick_taker_dashes != kicker->dash_count) {
                if (FREE_KICK_FAULTS) {
                    setPlayerState(stadium, kicker_index, STATE_FREE_KICK_FAULT);
                    free_kick_ref_callFreeKickFault(stadium, kicker->side, &stadium->ball.pos);
                }
                else if (PROPER_GOAL_KICKS) {
                    ref_awardGoalKick(stadium, kicker->side, &stadium->ball.pos);
                }
            }
            // else it's part of a compound kick
        }
        //else
        //{
        stadium->free_kick_ref_kick_taken = true;
        stadium->free_kick_ref_kick_taker_index = kicker_index;
        stadium->free_kick_ref_kick_taker_dashes = kicker->dash_count;
        //}
    }
    else if (stadium->playmode == PM_OffSide_Left || stadium->playmode == PM_OffSide_Right) {
        // do nothing
    }
    else if (stadium->playmode != PM_PlayOn) {
        stadium->free_kick_ref_kick_taken = true;
        stadium->free_kick_ref_kick_taker_index = kicker_index;
        stadium->free_kick_ref_kick_taker_dashes = kicker->dash_count;
        changePlayMode(stadium, PM_PlayOn);
    }
    else // PM_PlayOn
    {
        if (stadium->free_kick_ref_kick_taken) {
            if (kicker_index == stadium->free_kick_ref_kick_taker_index && FREE_KICK_FAULTS) {
                if (kicker->dash_count > stadium->free_kick_ref_kick_taker_dashes) {
                    setPlayerState(stadium, kicker_index, STATE_FREE_KICK_FAULT);
                    free_kick_ref_callFreeKickFault(stadium, kicker->side, &stadium->ball.pos);
                }
            }
            else {
                stadium->free_kick_ref_kick_taken = false;
            }
        }
    }
}

void touch_ref_kickTaken(MyStadiumV2 *stadium, int kicker_index, const float accel_r) {
    if (fabsf(stadium->ball.pos.x) <= PITCH_LENGTH * 0.5f + BALL_SIZE) {
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

void catch_ref_kickTaken(MyStadiumV2 *stadium, int kicker_index, const float accel_r) {
    const MyPlayerV2 *kicker = &stadium->players[kicker_index];
    if (kicker->side == LEFT)
        stadium->catch_ref_team_l_touched = true;
    else if (kicker->side == RIGHT)
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

void penalty_ref_kickTaken(MyStadiumV2 *stadium, int kicker_index, const float accel_r) {
    const MyPlayerV2 *kicker = &stadium->players[kicker_index];
    if (!ref_isPenaltyShootOut(stadium->playmode, NEUTRAL))
        return;

    // if in setup it is not allowed to kick the ball
    if (stadium->playmode == PM_PenaltySetup_Left || stadium->playmode == PM_PenaltySetup_Right)
        penalty_ref_penalty_foul(stadium, kicker->side);
    // cannot kick second time after penalty was taken
    else if (!PEN_ALLOW_MULTI_KICKS
        && (stadium->playmode == PM_PenaltyTaken_Left
        || stadium->playmode == PM_PenaltyTaken_Right)
        && kicker->side == stadium->penalty_ref_cur_pen_taker)
        penalty_ref_penalty_foul(stadium, stadium->penalty_ref_cur_pen_taker);
    else if (stadium->playmode == PM_PenaltyReady_Left
        || stadium->playmode == PM_PenaltyTaken_Left
        || stadium->playmode == PM_PenaltyReady_Right
        || stadium->playmode == PM_PenaltyTaken_Right) {
        if ((stadium->playmode == PM_PenaltyReady_Left
            || stadium->playmode == PM_PenaltyReady_Right)
            && kicker->side == stadium->penalty_ref_cur_pen_taker
            && ((LEFT == stadium->penalty_ref_cur_pen_taker
            && contain(stadium->penalty_ref_sLeftPenTaken, stadium->penalty_ref_sLeftPenTaken_size, kicker->unum))
            || (RIGHT == stadium->penalty_ref_cur_pen_taker
            && contain(stadium->penalty_ref_sRightPenTaken, stadium->penalty_ref_sLeftPenTaken_size, kicker->unum))))
            // this kicker has already taken the kick
            penalty_ref_penalty_foul(stadium, stadium->penalty_ref_cur_pen_taker);
        else if (stadium->penalty_ref_last_taker_index != -1
            && stadium->players[stadium->penalty_ref_last_taker_index].side == stadium->penalty_ref_cur_pen_taker
            && kicker_index != stadium->penalty_ref_last_taker_index)
            // not a taker player in the same team must not kick the ball.
            penalty_ref_penalty_foul(stadium, stadium->penalty_ref_cur_pen_taker);
        else if (kicker->side != stadium->penalty_ref_cur_pen_taker && !kicker->goalie)
            // field player in the defending team must not kick the ball.
            penalty_ref_penalty_foul(stadium, (Side)(-stadium->penalty_ref_cur_pen_taker));
        else
            stadium->penalty_ref_last_taker_index = kicker_index;
    }

    // if we were ready for penalty -> change play mode
    if (stadium->playmode == PM_PenaltyReady_Left) {
        // when penalty is taken, add player, multiple copies are deleted

        stadium->penalty_ref_sLeftPenTaken[stadium->penalty_ref_sLeftPenTaken_size++] = kicker->unum;
        if (stadium->penalty_ref_sLeftPenTaken_size == NUM_PLAYERS)
            stadium->penalty_ref_sLeftPenTaken_size = 0;
        changePlayMode(stadium, PM_PenaltyTaken_Left);
    }
    else if (stadium->playmode == PM_PenaltyReady_Right ) {
        stadium->penalty_ref_sRightPenTaken[stadium->penalty_ref_sRightPenTaken_size++] = kicker->unum;
        if (stadium->penalty_ref_sRightPenTaken_size == NUM_PLAYERS)
            stadium->penalty_ref_sRightPenTaken_size = 0;
        changePlayMode(stadium, PM_PenaltyTaken_Right);
    }
    // if it was not allowed to kick, don't move ball
    else if (stadium->playmode != PM_PenaltyTaken_Left && stadium->playmode != PM_PenaltyTaken_Right)
        placeBall(stadium, stadium->penalty_ref_pen_side, &stadium->ball.pos);
}

void ref_kickTaken(MyStadiumV2 *stadium, int kicker_index, const float accel_r) {
    offside_ref_kickTaken(stadium, kicker_index, accel_r);
    free_kick_ref_kickTaken(stadium, kicker_index, accel_r);
    touch_ref_kickTaken(stadium, kicker_index, accel_r);
    catch_ref_kickTaken(stadium, kicker_index, accel_r);
    penalty_ref_kickTaken(stadium, kicker_index, accel_r);
}

void offside_ref_tackleTaken(MyStadiumV2 *stadium, int tackler_index, const double accel_r, const bool foul) {
    offside_ref_setOffsideMark(stadium, tackler_index, accel_r );
}

void free_kick_ref_tackleTaken(MyStadiumV2 *stadium, int tackler_index, const double accel_r, const bool foul) {
    free_kick_ref_kickTaken(stadium, tackler_index, accel_r );
}

void touch_ref_tackleTaken(MyStadiumV2 *stadium, int tackler_index, const double accel_r, const bool foul) {
    touch_ref_kickTaken(stadium, tackler_index, accel_r );
}

void catch_ref_tackleTaken(MyStadiumV2 *stadium, int tackler_index, const double accel_r, const bool foul) {
    catch_ref_kickTaken(stadium, tackler_index, accel_r );
}

void foul_ref_callFoul(MyStadiumV2 *stadium, int tackler_index) {
    const MyPlayerV2 *tackler = &stadium->players[tackler_index];
    MyPVector pos = tackler->pos;
    ref_truncateToPitch(&pos);
    ref_moveOutOfGoalArea(NEUTRAL, &pos);
    stadium->ball_catcher_index = -1;
    if (tackler->side == LEFT) {
        ref_moveOutOfPenalty(RIGHT, &pos);
        ref_placeBallAndChangePlayMode(stadium, PM_Foul_Charge_Left, RIGHT, &pos);
    }
    else if (tackler->side == RIGHT) {
        ref_moveOutOfPenalty(LEFT, &pos);
        ref_placeBallAndChangePlayMode(stadium, PM_Foul_Charge_Right, LEFT, &pos);
    }

    punishFoulPlay(stadium, tackler_index);
    stadium->foul_ref_after_foul_time = 0;
}

void foul_ref_callYellowCard(MyStadiumV2 *stadium, int tackler_index) {
    foul_ref_callFoul(stadium, tackler_index);
    yellowCard(stadium, tackler_index);
}

void foul_ref_callRedCard(MyStadiumV2 *stadium, int tackler_index) {
    foul_ref_callFoul(stadium, tackler_index);
    redCard(stadium, tackler_index);
}

void foul_ref_tackleTaken(MyStadiumV2 *stadium, int tackler_index, const double accel_r, const bool foul) {
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

void penalty_ref_tackleTaken(MyStadiumV2 *stadium, int tackler_index, const double accel_r, const bool foul) {
    const MyPlayerV2 *tackler = &stadium->players[tackler_index];
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
        if ( tackler->side == stadium->penalty_ref_cur_pen_taker )
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

void ref_tackleTaken(MyStadiumV2 *stadium, int tackler_index, const float accel_r, const bool foul) {
    offside_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
    free_kick_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
    touch_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
    catch_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
    foul_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
    penalty_ref_tackleTaken(stadium, tackler_index, accel_r, foul);
}

void offside_ref_failedTackleTaken(MyStadiumV2 *stadium, int tackler_index, const bool foul) {
    offside_ref_checkIntentionalAction(stadium, tackler_index );
}

void ref_failedTackleTaken(MyStadiumV2 *stadium, int tackler_index, const bool foul) {
    offside_ref_failedTackleTaken(stadium, tackler_index, foul);
}

#endif

void turnMovableObjects(MyPlayerV2 *players, int players_size) {
    for (int i = 0 ; i < players_size ; i++) {
        MyPlayerV2 *player = &(players[i]);
        _turn(player);
    }
}

static inline bool intersect(const MyPVector* begin, const MyPVector* end, const MyPVector *circle_center, const float circle_radius, MyPVector* inter) {
    if (begin->x == end->x && begin->y == end->y)
        return false;

    MyPVector begin_to_end = {begin->x - end->x, begin->y - end->y};
    MyPVector begin_to_circle_center = {begin->x - circle_center->x, begin->y - circle_center->y};
    MyPVector end_to_circle_center = {end->x - circle_center->x, end->y - circle_center->y};
    if (r(&begin_to_end) < r(&begin_to_circle_center) - circle_radius)
        // object wont get within circles range
        return false;

    if (circle_center->x == 0.0f && circle_center->y == 0.0f) {
        float dx = end->x - begin->x;
        float dy = end->y - begin->y;
        float dr = sqrtf(dx * dx + dy * dy);
        float D = begin->x * end->y - end->x * begin->y;
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
            MyPVector first, second;
            if (dy < 0.0f) {
                first.x = x2;
                first.y = y1;
                second.x = x1;
                second.y = y2;
            }
            else {
                first.x = x1;
                first.y = y1;
                second.x = x2;
                second.y = y2;
            }

            if (!between(&first, begin, end) && !between(&second, begin, end)) 
                // intersections are not between the end points
                //                  std::cout << "Coll outside of end points\n"
                return false;

            if (!between(&first, begin, end)) {
                *inter = second;
                second = first;
            }
            else if (!between(&second, begin, end))
                *inter = first;
            else {
                MyPVector begin_to_first = {begin->x - first.x, begin->y - first.y};
                MyPVector begin_to_second = {begin->x - second.x, begin->y - second.y};
                if (r(&begin_to_first) < r(&begin_to_second))
                    *inter = first;
                else {
                    *inter = second;
                    second = first;
                }
            }

            if (inter->x == begin->x && inter->y == begin->y && !between(&second, begin, end))
                // fake collision.  Object is tagent to the circle and moving away
                return false;
            return true;
        }
    }
    else {
        MyPVector temp_center = {0.0f, 0.0f};
        if (intersect(&begin_to_circle_center, &end_to_circle_center, &temp_center, circle_radius, inter)) {
            inter->x += circle_center->x;
            inter->y += circle_center->y;
            return true;
        }
        return false;
    }
}

void inc(MyPVector *pos, MyPVector *vel, MyPVector *accel, float max_speed, float max_accel, float size, float decay, 
    float randp, unsigned int *seed, MyPlayerV2 *player) {
    if (accel->x || accel->y) {
        float tmp = r(accel);
        if ( tmp > max_accel ) {
            accel->x *= ( max_accel / tmp );
            accel->y *= ( max_accel / tmp );
        }

        vel->x += accel->x;
        vel->y += accel->y;
        tmp = r(vel);
        if ( tmp > max_speed ) {
            vel->x *= ( max_speed / tmp );
            vel->y *= ( max_speed / tmp );
        }
    }

    if (player)
        updateAngle_player(player);

    MyPVector vel_noise;
    noise(randp, vel, seed, &vel_noise);
    vel->x += vel_noise.x;
    vel->y += vel_noise.y;
    // note: wind is disabled by default in server params

    MyPVector post_center;
    float post_radius;
    nearestPost(pos, size, &post_center, &post_radius);

    MyPVector pos_to_post = {pos->x - post_center.x, pos->y - post_center.y};
    while (r(&pos_to_post) < post_radius) {
        //          std::cout << "In post\n";
        // then the ball has overlapped the post.  Either it was moved
        // there or "pushed".  Either way, we just move the ball away
        // from the post
        MyPVector diff = pos_to_post;
        if ( diff.x == 0.0f && diff.y == 0.0f )
            from_polar(post_radius, drand(seed, -M_PIf, +M_PIf), &diff);
        else
            normalize(&diff, post_radius);

        pos->x = post_center.x + diff.x;
        pos->y = post_center.y + diff.y;

        pos_to_post.x = pos->x - post_center.x;
        pos_to_post.y = pos->y - post_center.y;
        while (r(&pos_to_post) < post_radius) {
            // noise keeps it inside the post, move it a bit further out
            normalize(&diff, r(&diff) * 1.01f );
            pos->x = post_center.x + diff.x;
            pos->y = post_center.y + diff.y;
            pos_to_post.x = pos->x - post_center.x;
            pos_to_post.y = pos->y - post_center.y;
        }

        if (vel->x != 0.0 || vel->y != 0.0) {
            MyPVector pos2center = {.x = post_center.x - pos->x, .y = post_center.y - pos->y};
            MyPVector pos2center_neg = {.x = -pos2center.x, .y = -pos2center.y};
            rotate(vel, th(&pos2center_neg));
            vel->x = -vel->x;
            rotate(vel, th(&pos2center));
        }

        nearestPost(pos, size, &post_center, &post_radius);
        if (player)
            collidedWithPost(player);
        pos_to_post.x = pos->x - post_center.x;
        pos_to_post.y = pos->y - post_center.y;
    }

    MyPVector new_pos = {.x = pos->x + vel->x, .y = pos->y + vel->y};
    MyPVector second_post_center;
    float second_post_radius;
    nearestPost(&new_pos, size, &second_post_center, &second_post_radius);
    MyPVector inter;
    bool second = false;

    while ((pos->x != new_pos.x || pos->y != new_pos.y)
        && (intersect(pos, &new_pos, &post_center, post_radius, &inter) 
            || ((post_center.x != second_post_center.x || post_center.y != second_post_center.y || post_radius != second_post_radius) 
            && (second = intersect(pos, &new_pos, &second_post_center, second_post_radius, &inter))))) {
        // handle collision
        *pos = inter;

        MyPVector rem = {new_pos.x - pos->x, new_pos.y - pos->y};
        MyPVector coll_2_circle;
        if (second) {
            coll_2_circle.x = second_post_center.x - pos->x;
            coll_2_circle.y = second_post_center.y - pos->y;
        }
        else {
            coll_2_circle.x = post_center.x - pos->x;
            coll_2_circle.y = post_center.y - pos->y;
        }

        // 2008-05-22 akiyama
        // fixed endless-loop bug.
        // If this small vector is not added to M_pos, intersect() may still
        // return pos() as the intersect point.
        MyPVector temp;
        from_polar(1.0f, th(&coll_2_circle) + 180.0f, &temp);
        pos->x = nextafterf(pos->x, pos->x + temp.x);
        pos->y = nextafterf(pos->y, pos->y + temp.y);

        MyPVector coll_2_circle_neg = {-coll_2_circle.x, -coll_2_circle.y};
        rotate(&rem, th(&coll_2_circle_neg));
        rem.x = -rem.x;
        rotate(&rem, th(&coll_2_circle));

        new_pos.x = pos->x + rem.x;
        new_pos.y = pos->y + rem.y;

        // setup post and second post for next loop
        nearestPost(pos, size, &post_center, &post_radius);
        nearestPost(&new_pos, size, &second_post_center, &second_post_radius);

        // setup vel so it will decay normally.  The collisions are
        // elastic, so the maginitude does not change, but the heading
        // does
        MyPVector temp2;
        from_polar(r(vel), th(&rem), &temp2);
        *vel = temp2;

        second = false;

        if (player)
            collidedWithPost(player);
    }

    *pos = new_pos;
    vel->x *= decay;
    vel->y *= decay;
    accel->x *= 0.0f;
    accel->y *= 0.0f;
}

void collide(MyPVector *post_collision_pos, int *collision_count, bool *collided, const MyPVector *col_pos) {
    post_collision_pos->x = post_collision_pos->x + col_pos->x;
    post_collision_pos->y = post_collision_pos->y + col_pos->y;
    *collision_count += 1;
    *collided = true;
}

void calcCollisionPos(MyPVector *a_pos, MyPVector *b_pos, float a_size, float b_size, 
    MyPVector *a_post_collision_pos, MyPVector *b_post_collision_pos, int *a_collision_count, int *b_collision_count, 
    bool *a_collided, bool *b_collided, unsigned int *seed) {
    if (!a_pos || !b_pos)
        return;

    MyPVector mid = {(a_pos->x + b_pos->x) / 2.0f, (a_pos->y + b_pos->y) / 2.0f};
    MyPVector mid2a = {a_pos->x - mid.x, a_pos->y - mid.y};
    MyPVector mid2b = {b_pos->x - mid.x, b_pos->y - mid.y};

    /* pfr 10/25/01
        This was a really nasty bug. This used to be the condition
        if ( a->pos == b->pos )
        If a->pos and b->pos are approximately equal (but
        not ==, then a->pos - mid and b->pos - mid can both be 0.
        This means that in the else clause below, we call PVector::r(v)
        on two zero vectors, which makes them both (v,0).
        Then, the while statement below is an infinite loop */
    if (a_pos == b_pos || (r(&mid2a) < EPS && r(&mid2b) < EPS)) {
        // if the two objects are directly on top on one and other
        // then they will be separated at a random angle
        double a_ang = drand(seed, -M_PIf, M_PIf);
        double b_ang = normalize_angle(a_ang + M_PIf);
        from_polar(add_eps((a_size + b_size) / 2.0f), a_ang, &mid2a);
        from_polar(add_eps((a_size + b_size) / 2.0f), b_ang, &mid2b);
    }
    else {
        normalize(&mid2a, add_eps((a_size + b_size) / 2.0f));
        normalize(&mid2b, add_eps((a_size + b_size) / 2.0f));
    }

    MyPVector apos = {mid.x + mid2a.x, mid.y + mid2a.y};
    MyPVector bpos = {mid.x + mid2b.x, mid.y + mid2b.y};

    // 0.01% is added to the movement, as sometimes structural noise
    // means that even though mid2a and mid2b should be
    // a->size + b->size apart, they are ever so slightly
    // less.
    int count = 0;
    const double collision_dist2 = powf(a_size + b_size, 2);
    while (distance2(&apos, &bpos) < collision_dist2 && count < 10) {
        normalize(&mid2a, r(&mid2a) * 1.0001 );
        normalize(&mid2b, r(&mid2b) * 1.0001 );
        apos.x = mid.x + mid2a.x;
        apos.y = mid.y + mid2a.y;
        bpos.x = mid.x + mid2b.x;
        bpos.y = mid.y + mid2b.y;
        count += 1;
    }

    collide(a_post_collision_pos, a_collision_count, a_collided, &apos);
    collide(b_post_collision_pos, b_collision_count, b_collided, &bpos);
}

void calcBallCollisionPos(MyPlayerV2 *player, MyBall *ball, PlayMode playmode, unsigned int *seed) {
    if (playmode == PM_PlayOn) {
        calcCollisionPos(&ball->pos, &player->pos, ball->size, player->size, 
            &ball->post_collision_pos, &player->post_collision_pos, 
            &ball->collision_count, &player->collision_count, 
            &ball->collided, &player->collided, seed);
        return;
    }

    MyPVector b2p;
    if ( ball->pos.x == player->pos.x && ball->pos.y == player->pos.y ) {
        double p_ang = drand(seed, -M_PIf, M_PIf);
        from_polar(add_eps(ball->size + player->size), p_ang, &b2p);
    }
    else {
        b2p.x = player->pos.x - ball->pos.x;
        b2p.y = player->pos.y - ball->pos.y;
        normalize(&b2p, add_eps(ball->size + player->size));
    }

    collide(&ball->post_collision_pos, &ball->collision_count, &ball->collided, &ball->pos);
    MyPVector temp = {ball->pos.x + b2p.x, ball->pos.y + b2p.y};
    collide(&player->post_collision_pos, &player->collision_count, &player->collided, &temp);
}

void moveToCollisionPos(MyPVector *pos, MyPVector *post_collision_pos, int *collision_count) {
    if (*collision_count > 0) {
        post_collision_pos->x /= *collision_count;
        post_collision_pos->y /= *collision_count;
        *pos = *post_collision_pos;
    }

    post_collision_pos->x = 0.0f;
    post_collision_pos->y = 0.0f;
    *collision_count = 0;
}

void updateCollisionVel(MyPVector *vel, bool *collided) {
    if (*collided) {
        vel->x *= 0.1f;
        vel->y *= 0.1f;
        *collided = false;
    }
}

void collisions(MyStadiumV2 *stadium) {
    bool col = false;
    int max_loop = 10;

    do {
        col = false;
        clearCollision(&stadium->ball.post_collision_pos, &stadium->ball.collision_count);
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            MyPlayerV2 *player = &stadium->players[i];
            if (!player->enable) continue;
            clearCollision(&player->post_collision_pos, &player->collision_count);
        }

        // check ball to player
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            MyPlayerV2 *player = &stadium->players[i];
            if (!player->enable) continue;
            if (i != stadium->ball_catcher_index && 
                distance2(&stadium->ball.pos, &player->pos) < powf(stadium->ball.size + player->size, 2)) {
                col = true;
                collidedWithBall(player);
                #if REFEREES_ENABLED
                ref_ballTouched(stadium, i);
                #endif
                calcBallCollisionPos(player, &stadium->ball, stadium->playmode, stadium->seed);
            }
        }

        // check player to player
        for ( int i = 0; i < NUM_PLAYERS - 1; ++i ) {
            MyPlayerV2 *player_i = &stadium->players[i];
            if (!player_i->enable) continue;
            for ( int j = i + 1; j < NUM_PLAYERS; ++j ) {
                MyPlayerV2 *player_j = &stadium->players[j];
                if (!player_j->enable) continue;
                if (distance2(&player_i->pos, &player_j->pos) < powf(player_i->size + player_j->size, 2)) {
                    col = true;
                    collidedWithPlayer(player_i);
                    collidedWithPlayer(player_j);
                    calcCollisionPos(&player_i->pos, &player_j->pos, player_i->size, player_j->size, 
                        &player_i->post_collision_pos, &player_j->post_collision_pos, 
                        &player_i->collision_count, &player_j->collision_count, 
                        &player_i->collided, &player_j->collided, stadium->seed);
                }
            }
        }

        moveToCollisionPos(&stadium->ball.pos, &stadium->ball.post_collision_pos, 
            &stadium->ball.collision_count);
        for (int i = 0 ; i < NUM_PLAYERS ; i++) {
            MyPlayerV2 *player = &stadium->players[i];
            if (!player->enable) continue;
            moveToCollisionPos(&player->pos, &player->post_collision_pos, 
                &player->collision_count);
        }

        --max_loop;
    }
    while (col && max_loop > 0);

    updateCollisionVel(&stadium->ball.vel, &stadium->ball.collided);
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        MyPlayerV2 *player = &stadium->players[i];
        if (!player->enable) continue;
        updateCollisionVel(&player->vel, &player->collided);
    }
}

void incMovableObjects(MyStadiumV2 *stadium, MyPlayerV2 *players, int players_size, MyBall *ball, int ball_catcher_index, 
    unsigned int* seed, PlayMode playmode) {
    inc(&ball->pos, &ball->vel, &ball->accel, ball->max_speed, ball->max_accel, ball->size, ball->decay, ball->randp, seed, NULL);
    for (int i = 0 ; i < players_size ; i++) {
        MyPlayerV2 *player = &(players[i]);
        if (!player->enable) continue;
        inc(&player->pos, &player->vel, &player->accel, player->max_speed, player->max_accel, player->size, player->decay, player->randp, seed, NULL);
    }

    collisions(stadium);

    if (ball_catcher_index != -1) {
        MyPlayerV2 *ball_catcher = &players[ball_catcher_index];
        // keeps the caught ball infront of the player
        MyPVector rpos;
        from_polar(ball_catcher->size + BALL_SIZE, ball_catcher->angle_body_committed, &rpos);
        ball->pos.x = ball_catcher->pos.x + rpos.x;
        ball->pos.y = ball_catcher->pos.y + rpos.y;
    }
}

void stadium_step(MyStadiumV2 *stadium) {
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        MyPlayerV2 *player = &(stadium->players[i]);
        if (!player->enable) continue;
        applyLegsEffect(player, stadium->seed);
        resetCommandFlags(player);
    }

    if (stadium->playmode == PM_BeforeKickOff) {
        turnMovableObjects(stadium->players, NUM_PLAYERS);
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
        incMovableObjects(stadium, stadium->players, NUM_PLAYERS, &stadium->ball, 
            stadium->ball_catcher_index, stadium->seed, stadium->playmode);
        stadium->stoppage_time += 1;
        #if REFEREES_ENABLED
        ref_analyse(stadium);
        #endif
        if ( pm != stadium->playmode )
        {
            stadium->time += 1;
            stadium->stoppage_time = 0;
        }
    }
    else if (stadium->playmode != PM_BeforeKickOff && stadium->playmode != PM_TimeOver) {
        incMovableObjects(stadium, stadium->players, NUM_PLAYERS, &stadium->ball, 
            stadium->ball_catcher_index, stadium->seed, stadium->playmode);
        stadium->time += 1;
        stadium->stoppage_time = 0;
        #if REFEREES_ENABLED
        ref_analyse(stadium);
        #endif
    }
    else if (stadium->playmode == PM_TimeOver) {
        stadium->stoppage_time += 1;
    }

    //
    // update stamina etc
    //
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        MyPlayerV2 *player = &stadium->players[i];
        if (!player->enable) continue;
        updateStamina(player);
        updateCapacity(player);
    }

    //
    // reset player state
    //
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        MyPlayerV2 *player = &stadium->players[i];
        if (!player->enable) continue;
        resetState(player);
    }
}

void stadium_moveBall(MyStadiumV2* stadium, float x, float y, float velx, float vely) {
    stadium->ball_catcher_index = -1;
    stadium->ball.pos.x = x;
    stadium->ball.pos.y = y;
    stadium->ball.vel.x = velx;
    stadium->ball.vel.y = vely;
    stadium->ball.accel.x = 0.0f;
    stadium->ball.accel.y = 0.0f;
}

void stadium_movePlayer(MyStadiumV2* stadium, MyPlayerV2* player, double x, double y, double ang, double velx, double vely) {
    ang = Deg2Rad(clamp(MIN_MOMENT, ang, MAX_MOMENT));
    player->pos.x = x;
    player->pos.y = y;
    player->angle_body = ang;
    player->angle_body_committed = ang;
    player->vel.x = velx;
    player->vel.y = vely;
    collisions(stadium);
}

void recoverAll(MyPlayerV2 *player) {
    player->stamina = STAMINA_MAX;
    player->recovery = 1.0f;
    player->effort = player->player_type.effort_max;
    recoverStaminaCapacity(player);
    player->consumed_stamina = 0.0f;
    player->hear_capacity_from_teammate = HEAR_MAX;
    player->hear_capacity_from_opponent = HEAR_MAX;
}

void stadium_recoveryPlayers(MyStadiumV2* stadium) {
    for (int i = 0 ; i < NUM_PLAYERS ; i++) {
        if (!stadium->players[i].enable) continue;
        recoverAll(&stadium->players[i]);
    }
}

void setPlayerType(MyStadiumV2 *stadium, MyPlayerV2 *player, const int id) {
    const MyHeteroPlayer *type = &stadium->player_types[id];
    if (!type)
        return;

    // player->player_type_id = id;
    player->player_type = *type;
    player->max_speed = player->player_type.player_speed_max;
    player->decay = player->player_type.player_decay;
    player->size = player->player_type.player_size;
    player->kick_rand = player->player_type.kick_rand;

    // mydiff : actuator noise is disabled by default
    // if ( ServerParam::instance().teamActuatorNoise() )
    // {
    //     player->kick_rand *= team()->kickRandFactorTeam();
    // }
}

void hetero_player_setDefault(MyHeteroPlayer *player_type) {
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

float hetero_player_delta(const float min, const float max, unsigned int *seed) {
    if ( min == max )
        return min;

    return drand(seed, min, max);
}

void hetero_player_init(MyHeteroPlayer *player_type, unsigned int *seed) {
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

void reset_player(MyStadiumV2 *stadium, MyPlayerV2 *player) {
    player->pos.x = -(player->unum * 3 * player->side);
    player->pos.y = -PITCH_WIDTH / 2.0f - 3.0f;
    player->vel.x = 0.0f;
    player->vel.y = 0.0f;
    player->accel.x = 0.0f;
    player->accel.y = 0.0f;
    player->size = 1.0f;
    
    player->post_collision_pos.x = 0.0f;
    player->post_collision_pos.y = 0.0f;
    player->collision_count = 0;
    player->collided = false;

    player->state = STATE_DISABLE;
    player->stamina = STAMINA_MAX;
    player->recovery = RECOVER_INIT;
    player->effort = EFFORT_INIT;
    player->stamina_capacity = STAMINA_CAPACITY;
    
    player->angle_body = 0.0f;
    player->angle_body_committed = 0.0f;
    player->angle_neck = 0.0f;
    player->angle_neck_committed = 0.0f;

    player->ball_collide = false;
    player->player_collide = false;
    player->post_collide = false;

    player->command_done = false;
    player->turn_neck_done = false;
    player->done_received = false;

    player->goalie_catch_ban = 0;
    player->kick_cycles = 0;
    player->dash_cycles = 0;
    player->tackle_cycles = 0;
    player->foul_cycles = 0;
    player->dash_count = 0;

    player->hear_capacity_from_teammate = HEAR_MAX;
    player->hear_capacity_from_opponent = HEAR_MAX;

    player->max_speed = PLAYER_SPEED_MAX;
    player->max_accel = PLAYER_ACCEL_MAX;
    setPlayerType(stadium, player, 0);
    recoverAll(player);

    player->state = STATE_STAND;
    player->enable = true;
    if (player->goalie)
        player->state |= STATE_GOALIE;
    player->angle_body_committed = player->side == LEFT ? TEAM_L_DIRECTION : TEAM_R_DIRECTION;
    player->randp = PLAYER_RAND;
}

void stadium_reset(MyStadiumV2 *stadium) {
    stadium->time = 0;
    stadium->stoppage_time = 0;
    stadium->last_playon_start = -1;
    stadium->playmode = PM_BeforeKickOff;
    stadium->kick_off_side = LEFT;

    for (int i = 0 ; i < NUM_PLAYERS ; i++)
        reset_player(stadium, &stadium->players[i]);

    stadium->ball_catcher_index = -1;
#if REFEREES_ENABLED
    stadium->free_kick_ref_kick_taker_index = -1;
    stadium->touch_ref_last_indirect_kicker_index = -1;
    stadium->touch_ref_last_touched_index = -1;
    stadium->catch_ref_before_last_back_passer_index = -1;
    stadium->catch_ref_last_back_passer_index = -1;
    stadium->penalty_ref_last_taker_index = -1;
#endif
    stadium->ball.pos.x = 0.0f;
    stadium->ball.pos.y = 0.0f;
    stadium->ball.vel.x = 0.0f;
    stadium->ball.vel.y = 0.0f;
    stadium->ball.accel.x = 0.0f;
    stadium->ball.accel.y = 0.0f;
    stadium->ball.size = BALL_SIZE;
    stadium->ball.decay = BALL_DECAY;
    stadium->ball.randp = BALL_RAND;
    stadium->ball.max_speed = BALL_SPEED_MAX;
    stadium->ball.max_accel = BALL_ACCEL_MAX;
    stadium->ball.post_collision_pos.x = 0.0f;
    stadium->ball.post_collision_pos.y = 0.0f;
    stadium->ball.collision_count = 0;
    stadium->ball.collided = false;

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

void stadium_init(MyStadiumV2 *stadium, unsigned int *seed) {
    stadium->seed = seed;

    hetero_player_setDefault(&stadium->player_types[0]);
    for (int i = 1; i < PLAYER_TYPES; i++) {
        hetero_player_init(&stadium->player_types[i], stadium->seed);
    }

    memset(stadium->players, 0, sizeof(stadium->players));
    for (int i = 0 ; i < LEFT_PLAYERS_COUNT ; i++) {
        MyPlayerV2 *player = &stadium->players[i];
        player->side = LEFT;
        player->goalie = false;
        player->unum = i + 1;
    }
    for (int i = 0 ; i < RIGHT_PLAYERS_COUNT ; i++) {
        MyPlayerV2 *player = &stadium->players[LEFT_PLAYERS_COUNT + i];
        player->side = RIGHT;
        player->goalie = false;
        player->unum = i + 1;
    }
    stadium->team_left_enabled = LEFT_PLAYERS_COUNT > 0;
    stadium->team_right_enabled = RIGHT_PLAYERS_COUNT > 0;
}

#endif // GAMEPLAY_H