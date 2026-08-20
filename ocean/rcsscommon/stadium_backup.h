#ifndef STADIUM_H
#define STADIUM_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#pragma region constants

#ifndef M_PIf
#define M_PIf (float)M_PI
#endif

#ifndef REFEREES_ENABLED
#define REFEREES_ENABLED 1
#endif

#ifndef LEFT_PLAYERS_COUNT
#define LEFT_PLAYERS_COUNT 11
#endif

#ifndef RIGHT_PLAYERS_COUNT
#define RIGHT_PLAYERS_COUNT 11
#endif

#define NUM_PLAYERS (LEFT_PLAYERS_COUNT + RIGHT_PLAYERS_COUNT)

#define MIN_DASH_POWER 0
#define MAX_DASH_POWER 100
#define MIN_DASH_ANGLE -180
#define MAX_DASH_ANGLE 180
#define DASH_ANGLE_STEP 1
#define DASH_POWER_RATE 0.006f
#define DASH_POWER_RATE_DELTA_MIN 0
#define DASH_POWER_RATE_DELTA_MAX 0
#define NEW_DASH_POWER_RATE_DELTA_MIN -0.0012f
#define NEW_DASH_POWER_RATE_DELTA_MAX 0.0008f
#define BACK_DASH_RATE 0.7f
#define SIDE_DASH_RATE 0.4f
#define SLOWNESS_ON_TOP_FOR_LEFT 1
#define SLOWNESS_ON_TOP_FOR_RIGHT 1
#define MIN_MOMENT -180
#define MAX_MOMENT 180
#define MIN_NECK_MOMENT -180
#define MAX_NECK_MOMENT 180
#define MIN_NECK_ANGLE -90
#define MAX_NECK_ANGLE 90
#define MIN_POWER -100
#define MAX_POWER 100
#define TACKLE_CYCLES 10
#define TACKLE_DIST 2
#define TACKLE_BACK_DIST 0
#define TACKLE_EXPONENT 6
#define FOUL_EXPONENT 10
#define TACKLE_WIDTH 1.25f
#define MAX_TACKLE_POWER 100
#define MAX_BACK_TACKLE_POWER 0
#define TACKLE_POWER_RATE 0.027f
#define TACKLE_RAND_FACTOR 2
#define RED_CARD_PROBABILITY 0
#define FOUL_CYCLES 5
#define TEAM_L_DIRECTION 0
#define TEAM_R_DIRECTION M_PIf

#define PITCH_LENGTH 105.0f
#define PITCH_WIDTH 68.0f
#define PITCH_MARGIN 5.0f
#define GOAL_POST_RADIUS 0.06f
#define GOAL_WIDTH 14.02f
#define GOAL_AREA_LENGTH 5.5f
#define GOAL_AREA_WIDTH 18.32f
#define PENALTY_AREA_LENGTH 16.5f
#define PENALTY_AREA_WIDTH 40.32f
#define CORNER_KICK_MARGIN 1
#define PEN_DIST_X 42.5f

#define EPS 1.0e-10f
#define BALL_SIZE 0.085f
#define BALL_DECAY 0.94f
#define BALL_RAND 0.05f
#define BALL_SPEED_MAX 3
#define BALL_ACCEL_MAX 2.7f
#define STOPPED_BALL_VEL 0.01f
#define PLAYER_TYPES 18
#define PLAYER_RAND 0.1f
#define PLAYER_SIZE 0.3f
#define PLAYER_SIZE_DELTA_FACTOR -100
#define PLAYER_SPEED_MAX 1.05f
#define PLAYER_SPEED_MAX_MIN 0.75f
#define PLAYER_SPEED_MAX_DELTA_MIN 0
#define PLAYER_SPEED_MAX_DELTA_MAX 0
#define PLAYER_ACCEL_MAX 1
#define PLAYER_DECAY 0.4f
#define PLAYER_DECAY_DELTA_MIN -0.1f
#define PLAYER_DECAY_DELTA_MAX 0.1f
#define INERTIA_MOMENT 5
#define INERTIA_MOMENT_DELTA_FACTOR 25
#define KICKABLE_MARGIN 0.7f
#define KICKABLE_MARGIN_DELTA_MIN -0.1f
#define KICKABLE_MARGIN_DELTA_MAX 0.1f
#define KICK_RAND 0.1f
#define KICK_RAND_DELTA_FACTOR 1
#define KICK_POWER_RATE 0.027f
#define KICK_POWER_RATE_DELTA_MIN 0
#define KICK_POWER_RATE_DELTA_MAX 0
#define FOUL_DETECT_PROBABILITY 0.5f
#define FOUL_DETECT_PROBABILITY_DELTA_FACTOR 0
#define STAMINA_CAPACITY 130600
#define STAMINA_MAX 8000
#define STAMINA_INC_MAX 45
#define STAMINA_INC_MAX_DELTA_FACTOR 0
#define NEW_STAMINA_INC_MAX_DELTA_FACTOR -6000
#define EXTRA_STAMINA 50
#define EXTRA_STAMINA_DELTA_MIN 0
#define EXTRA_STAMINA_DELTA_MAX 50
#define RECOVER_INIT 1
#define RECOVER_MIN 0.5f
#define RECOVER_DEC 0.002f
#define RECOVER_DEC_THR 0.3f
#define EFFORT_INIT 1
#define EFFORT_MAX_DELTA_FACTOR -0.004f
#define EFFORT_MIN_DELTA_FACTOR -0.004f
#define EFFORT_MIN 0.6f
#define EFFORT_DEC 0.005f
#define EFFORT_INC 0.01f
#define EFFORT_DEC_THR 0.3f
#define EFFORT_INC_THR 0.6f
#define HEAR_INC 1
#define HEAR_MAX 1
// #define HALF_TIME 300
#define HALF_TIME -1
#define NR_NORMAL_HALFS 2
#define EXTRA_HALF_TIME 100
#define NR_EXTRA_HALFS 2
#define PENALTY_SHOOT_OUTS true
#define GOLDEN_GOAL false
#define KICK_OFF_OFFSIDE true
#define KICK_OFF_CLEAR_DISTANCE 9.15f
#define BALL_STUCK_AREA 3
// #define DROP_BALL_TIME 100
#define DROP_BALL_TIME -1
#define USE_OFFSIDE true
#define AFTER_OFFSIDE_WAIT 30
#define CLEAR_PLAYER_TIME 5
#define OFFSIDE_KICK_MARGIN 9.15f
#define OFFSIDE_ACTIVE_AREA_SIZE 2.5f
#define AFTER_FREE_KICK_FAULT_WAIT 30
#define FREE_KICK_FAULTS true
#define PROPER_GOAL_KICKS false
#define MAX_GOAL_KICKS 3
#define AFTER_GOAL_WAIT 50
#define AFTER_BACKPASS_WAIT 30
#define AFTER_CATCH_FAULT_WAIT 30
#define AFTER_FOUL_WAIT 30
#define PEN_COACH_MOVES_PLAYERS true
#define PEN_MAX_GOALIE_DIST_X 14
#define PEN_MAX_EXTRA_KICKS 5
#define PEN_NR_KICKS 5
#define PEN_RANDOM_WINNER false
#define PEN_BEFORE_SETUP_WAIT 10
#define PEN_SETUP_WAIT 70
#define PEN_READY_WAIT 10
#define PEN_TAKEN_WAIT 150
#define PEN_ALLOW_MULTI_KICKS true

#pragma endregion

typedef enum {
    PM_Null,
    PM_BeforeKickOff,
    PM_TimeOver,
    PM_PlayOn,
    PM_KickOff_Left,
    PM_KickOff_Right,
    PM_KickIn_Left,
    PM_KickIn_Right,
    PM_FreeKick_Left,
    PM_FreeKick_Right,
    PM_CornerKick_Left,
    PM_CornerKick_Right,
    PM_GoalKick_Left,
    PM_GoalKick_Right,
    PM_AfterGoal_Left,
    PM_AfterGoal_Right,
    PM_Drop_Ball,
    PM_OffSide_Left,
    PM_OffSide_Right,
    // [I.Noda:00/05/13] added for 3D viewer/commentator/small league
    PM_PK_Left,
    PM_PK_Right,
    PM_FirstHalfOver,
    PM_Pause,
    PM_Human,
    PM_Foul_Charge_Left,
    PM_Foul_Charge_Right,
    PM_Foul_Push_Left,
    PM_Foul_Push_Right,
    PM_Foul_MultipleAttacker_Left,
    PM_Foul_MultipleAttacker_Right,
    PM_Foul_BallOut_Left,
    PM_Foul_BallOut_Right,
    PM_Back_Pass_Left,
    PM_Back_Pass_Right,
    PM_Free_Kick_Fault_Left,
    PM_Free_Kick_Fault_Right,
    PM_CatchFault_Left,
    PM_CatchFault_Right,
    PM_IndFreeKick_Left,
    PM_IndFreeKick_Right,
    PM_PenaltySetup_Left,
    PM_PenaltySetup_Right,
    PM_PenaltyReady_Left,
    PM_PenaltyReady_Right,
    PM_PenaltyTaken_Left,
    PM_PenaltyTaken_Right,
    PM_PenaltyMiss_Left,
    PM_PenaltyMiss_Right,
    PM_PenaltyScore_Left,
    PM_PenaltyScore_Right,
    PM_Illegal_Defense_Left,
    PM_Illegal_Defense_Right,
    PM_MAX
} PlayMode;

typedef enum {
    STATE_DISABLE =         0x00000000,
    STATE_STAND =           0x00000001,
    STATE_KICK =            0x00000002,
    STATE_KICK_FAULT =      0x00000004,
    STATE_GOALIE =          0x00000008,
    STATE_CATCH =           0x00000010,
    STATE_CATCH_FAULT =     0x00000020,
    STATE_BALL_TO_PLAYER =  0x00000040,
    STATE_PLAYER_TO_BALL =  0x00000080,
    STATE_DISCARD =         0x00000100,
    STATE_LOST =            0x00000200, // [I.Noda:00/05/13] added for 3D viewer/commentator/small league
    STATE_BALL_COLLIDE =    0x00000400, // player collided with the ball
    STATE_PLAYER_COLLIDE =  0x00000800, // player collided with another player
    STATE_TACKLE =          0x00001000,
    STATE_TACKLE_FAULT =    0x00002000,
    STATE_BACK_PASS =       0x00004000,
    STATE_FREE_KICK_FAULT = 0x00008000,
    STATE_POST_COLLIDE =    0x00010000, // player collided with goal posts
    STATE_FOUL_CHARGED =    0x00020000, // player is frozen by intentional tackle foul
    STATE_YELLOW_CARD =     0x00040000,
    STATE_RED_CARD =        0x00080000,
    STATE_ILLEGAL_DEFENSE = 0x00100000,
} PlayerState;

typedef enum {
    LEFT = 1,
    NEUTRAL = 0,
    RIGHT = -1
} Side;

typedef enum {
    MOVE,
    DASH,
    TURN,
    KICK,
    TACKLE,
    NONE
} CommandType;

typedef struct {
    float x;
    float y;
} MyPVector;

typedef struct {
    float extra_stamina;
    float dash_power_rate;
    float player_size;
    float effort_max;
    float effort_min;
    float stamina_inc_max;
    float player_speed_max;
    float player_decay;
    float kick_rand;
    float inertia_moment;
    float kickable_margin;
    float kick_power_rate;
    float foul_detect_probability;
} MyHeteroPlayer;

typedef struct {
    Side side;
    bool goalie;
    int unum;
    float size;
    MyPVector pos;
    MyPVector vel;
    MyPVector accel;
    float max_speed;
    float max_accel;
    float decay;
    float randp;
    MyPVector post_collision_pos;
    int collision_count;
    bool collided;
    float kick_rand;

    float stamina;
    float effort;
    float recovery;
    float stamina_capacity;
    float consumed_stamina;

    float angle_body;
    float angle_body_committed;
    float angle_neck;
    float angle_neck_committed;
    
    bool enable;
    int32_t state;
    bool ball_collide;
    bool player_collide;
    bool post_collide;

    bool command_done;
    bool turn_neck_done;
    bool done_received;

    CommandType left_leg_command_type;
    CommandType right_leg_command_type;
    float left_leg_dash_power;
    float right_leg_dash_power;
    float left_leg_dash_dir;
    float right_leg_dash_dir;

    int kick_cycles;
    int dash_cycles;
    int tackle_cycles;
    int foul_cycles;

    int kick_count;
    int dash_count;
    int tackle_count;
    int foul_count;
    int turn_count;
    // int catch_count;
    // int move_count;
    int turn_neck_count;
    // int change_focus_count;
    // int change_view_count;
    // int say_count;
    int card_count;

    MyHeteroPlayer player_type;

    int hear_capacity_from_teammate;
    int hear_capacity_from_opponent;
    int goalie_catch_ban;
} MyPlayerV2;

typedef struct {
    float size;
    MyPVector pos;
    MyPVector vel;
    MyPVector accel;
    float max_speed;
    float max_accel;
    float decay;
    float randp;
    MyPVector post_collision_pos;
    int collision_count;
    bool collided;
} MyBall;

typedef struct {
    int player_index;
    MyPVector pos;
} Candidate;

typedef struct {
    unsigned int *seed;
    int time;
    int stoppage_time;
    int last_playon_start;
    PlayMode playmode;
    Side kick_off_side;
    MyHeteroPlayer player_types[PLAYER_TYPES];
    MyPlayerV2 players[NUM_PLAYERS];
    int8_t ball_catcher_index;
    MyBall ball;

    bool team_left_enabled;
    bool team_right_enabled;
    int team_left_points;
    int team_right_points;
    int team_left_pen_taken;
    int team_right_pen_taken;
    int team_left_pen_point;
    int team_right_pen_point;
    bool team_left_pen_won;
    bool team_right_pen_won;

#if REFEREES_ENABLED
    int time_ref_s_half_time_count;
    MyPVector ball_stuck_ref_last_ball_pos;
    int ball_stuck_ref_counter;
    int offside_ref_last_kick_time;
    int offside_ref_last_kick_stoppage_time;
    float offside_ref_last_kick_accel_r;
    Side offside_ref_last_kicker_side;
    int offside_ref_after_offside_time;
    MyPVector offside_ref_offside_pos;
    Candidate offside_ref_offside_candidates[NUM_PLAYERS];
    uint8_t offside_ref_offside_candidates_size;
    int free_kick_ref_after_free_kick_fault_time;
    bool free_kick_ref_kick_taken;
    int free_kick_ref_kick_taker_dashes;
    int free_kick_ref_kick_taker_index;
    int free_kick_ref_timer;
    int free_kick_ref_goal_kick_count;
    int touch_ref_after_goal_time;
    bool touch_ref_indirect_mode;
    int touch_ref_last_touched_time;
    float touch_ref_last_touched_accel_r;
    int touch_ref_last_indirect_kicker_index;
    MyPVector touch_ref_prev_ball_pos;
    int touch_ref_last_touched_index;
    bool catch_ref_team_l_touched;
    bool catch_ref_team_r_touched;
    int catch_ref_after_back_pass_time;
    int catch_ref_after_catch_fault_time;
    int catch_ref_before_last_back_passer_index;
    int catch_ref_last_back_passer_index;
    int catch_ref_last_back_passer_time;
    int foul_ref_after_foul_time;
    MyPVector penalty_ref_prev_ball_pos;
    bool penalty_ref_first_time;
    Side penalty_ref_pen_side;
    Side penalty_ref_cur_pen_taker;
    bool penalty_ref_timeover;
    int penalty_ref_timer;
    int penalty_ref_last_taker_index;
    int penalty_ref_pen_nr_taken;
    int penalty_ref_sLeftPenTaken[NUM_PLAYERS];
    int penalty_ref_sRightPenTaken[NUM_PLAYERS];
    int penalty_ref_sLeftPenTaken_size;
    int penalty_ref_sRightPenTaken_size;
#endif

} MyStadiumV2;

#endif // STADIUM_H