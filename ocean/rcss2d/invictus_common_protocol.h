/* invictus_common_protocol.h
 *
 * Shared soccer protocol types and parser primitives. (included by player and trainer protocols)
 *
 * This header is included by both:
 *   - invictus_player_protocol.h (player sensors + effectors)
 *   - invictus_trainer_protocol.h (offline coach / trainer)
 *
 * It provides:
 *   - Common constants
 *   - InvictusSide and InvictusPlayMode enums + lookup
 *   - Low-level string parsing helpers (skip_ws, parse_*)
 *   - Common structs: InvictusServerParams (synch_mode, slow_down_factor) and
 *     InvictusCommonControl (think_received etc.) + their parsers
 *   - General server_param and ok/error helpers (used by both)
 *
 * The raw UDP transport is still in invictus_transport.h.
 * Player-specific and trainer-specific extensions live in their
 * respective *_protocol.h files.
 *
 * Include this (or the player/trainer protocol headers) as needed.
 * Direct field access on all public structs is the supported style.
 */

#ifndef INVICTUS_COMMON_PROTOCOL_H
#define INVICTUS_COMMON_PROTOCOL_H

#include "invictus_transport.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Shared constants                                                   */
/* ------------------------------------------------------------------ */

#define INVICTUS_DEFAULT_VERSION   18.0
#define INVICTUS_MAX_SEEN_OBJS     40
#define INVICTUS_MAX_PLAYERS_SEEN  22
#define INVICTUS_TEAMNAME_MAX      32

/* ------------------------------------------------------------------ */
/* Common enums                                                       */
/* ------------------------------------------------------------------ */

typedef enum {
    INVICTUS_SIDE_UNKNOWN = 0,
    INVICTUS_SIDE_LEFT,
    INVICTUS_SIDE_RIGHT
} InvictusSide;

typedef enum {
    INVICTUS_PM_NULL = 0,
    INVICTUS_PM_BeforeKickOff,
    INVICTUS_PM_TimeOver,
    INVICTUS_PM_PlayOn,
    INVICTUS_PM_KickOff_Left,
    INVICTUS_PM_KickOff_Right,
    INVICTUS_PM_KickIn_Left,
    INVICTUS_PM_KickIn_Right,
    INVICTUS_PM_FreeKick_Left,
    INVICTUS_PM_FreeKick_Right,
    INVICTUS_PM_CornerKick_Left,
    INVICTUS_PM_CornerKick_Right,
    INVICTUS_PM_GoalKick_Left,
    INVICTUS_PM_GoalKick_Right,
    INVICTUS_PM_AfterGoal_Left,
    INVICTUS_PM_AfterGoal_Right,
    INVICTUS_PM_Drop_Ball,
    INVICTUS_PM_OffSide_Left,
    INVICTUS_PM_OffSide_Right,
    INVICTUS_PM_PK_Left,
    INVICTUS_PM_PK_Right,
    INVICTUS_PM_FirstHalfOver,
    INVICTUS_PM_Pause,
    INVICTUS_PM_Human,
    INVICTUS_PM_FoulCharge_Left,
    INVICTUS_PM_FoulCharge_Right,
    INVICTUS_PM_FoulPush_Left,
    INVICTUS_PM_FoulPush_Right,
    INVICTUS_PM_FoulMultipleAttacker_Left,
    INVICTUS_PM_FoulMultipleAttacker_Right,
    INVICTUS_PM_FoulBallOut_Left,
    INVICTUS_PM_FoulBallOut_Right,
    INVICTUS_PM_BackPass_Left,
    INVICTUS_PM_BackPass_Right,
    INVICTUS_PM_FreeKickFault_Left,
    INVICTUS_PM_FreeKickFault_Right,
    INVICTUS_PM_CatchFault_Left,
    INVICTUS_PM_CatchFault_Right,
    INVICTUS_PM_IndFreeKick_Left,
    INVICTUS_PM_IndFreeKick_Right,
    INVICTUS_PM_PenaltySetup_Left,
    INVICTUS_PM_PenaltySetup_Right,
    INVICTUS_PM_PenaltyReady_Left,
    INVICTUS_PM_PenaltyReady_Right,
    INVICTUS_PM_PenaltyTaken_Left,
    INVICTUS_PM_PenaltyTaken_Right,
    INVICTUS_PM_PenaltyMiss_Left,
    INVICTUS_PM_PenaltyMiss_Right,
    INVICTUS_PM_PenaltyScore_Left,
    INVICTUS_PM_PenaltyScore_Right,
    INVICTUS_PM_IllegalDefense_Left,
    INVICTUS_PM_IllegalDefense_Right,
    INVICTUS_PM_PenaltyOnfield_Left,
    INVICTUS_PM_PenaltyOnfield_Right,
    INVICTUS_PM_PenaltyFoul_Left,
    INVICTUS_PM_PenaltyFoul_Right,
    INVICTUS_PM_GoalieCatch_Left,
    INVICTUS_PM_GoalieCatch_Right,
    INVICTUS_PM_ExtendHalf,
    INVICTUS_PM_MAX
} InvictusPlayMode;

/* ------------------------------------------------------------------ */
/* Playmode lookup (shared by player + trainer hear parsing)          */
/* ------------------------------------------------------------------ */

static const struct { const char* name; InvictusPlayMode pm; } invictus_playmode_table[] = {
    { "before_kick_off",           INVICTUS_PM_BeforeKickOff },
    { "time_over",                 INVICTUS_PM_TimeOver },
    { "play_on",                   INVICTUS_PM_PlayOn },
    { "kick_off_l",                INVICTUS_PM_KickOff_Left },
    { "kick_off_r",                INVICTUS_PM_KickOff_Right },
    { "kick_in_l",                 INVICTUS_PM_KickIn_Left },
    { "kick_in_r",                 INVICTUS_PM_KickIn_Right },
    { "free_kick_l",               INVICTUS_PM_FreeKick_Left },
    { "free_kick_r",               INVICTUS_PM_FreeKick_Right },
    { "corner_kick_l",             INVICTUS_PM_CornerKick_Left },
    { "corner_kick_r",             INVICTUS_PM_CornerKick_Right },
    { "goal_kick_l",               INVICTUS_PM_GoalKick_Left },
    { "goal_kick_r",               INVICTUS_PM_GoalKick_Right },
    { "goal_l",                    INVICTUS_PM_AfterGoal_Left },
    { "goal_r",                    INVICTUS_PM_AfterGoal_Right },
    { "drop_ball",                 INVICTUS_PM_Drop_Ball },
    { "offside_l",                 INVICTUS_PM_OffSide_Left },
    { "offside_r",                 INVICTUS_PM_OffSide_Right },
    { "penalty_kick_l",            INVICTUS_PM_PK_Left },
    { "penalty_kick_r",            INVICTUS_PM_PK_Right },
    { "first_half_over",           INVICTUS_PM_FirstHalfOver },
    { "pause",                     INVICTUS_PM_Pause },
    { "human",                     INVICTUS_PM_Human },
    { "foul_charge_l",             INVICTUS_PM_FoulCharge_Left },
    { "foul_charge_r",             INVICTUS_PM_FoulCharge_Right },
    { "foul_push_l",               INVICTUS_PM_FoulPush_Left },
    { "foul_push_r",               INVICTUS_PM_FoulPush_Right },
    { "foul_multiple_attacker_l",  INVICTUS_PM_FoulMultipleAttacker_Left },
    { "foul_multiple_attacker_r",  INVICTUS_PM_FoulMultipleAttacker_Right },
    { "foul_ball_out_l",           INVICTUS_PM_FoulBallOut_Left },
    { "foul_ball_out_r",           INVICTUS_PM_FoulBallOut_Right },
    { "back_pass_l",               INVICTUS_PM_BackPass_Left },
    { "back_pass_r",               INVICTUS_PM_BackPass_Right },
    { "free_kick_fault_l",         INVICTUS_PM_FreeKickFault_Left },
    { "free_kick_fault_r",         INVICTUS_PM_FreeKickFault_Right },
    { "catch_fault_l",             INVICTUS_PM_CatchFault_Left },
    { "catch_fault_r",             INVICTUS_PM_CatchFault_Right },
    { "indirect_free_kick_l",      INVICTUS_PM_IndFreeKick_Left },
    { "indirect_free_kick_r",      INVICTUS_PM_IndFreeKick_Right },
    { "penalty_setup_l",           INVICTUS_PM_PenaltySetup_Left },
    { "penalty_setup_r",           INVICTUS_PM_PenaltySetup_Right },
    { "penalty_ready_l",           INVICTUS_PM_PenaltyReady_Left },
    { "penalty_ready_r",           INVICTUS_PM_PenaltyReady_Right },
    { "penalty_taken_l",           INVICTUS_PM_PenaltyTaken_Left },
    { "penalty_taken_r",           INVICTUS_PM_PenaltyTaken_Right },
    { "penalty_miss_l",            INVICTUS_PM_PenaltyMiss_Left },
    { "penalty_miss_r",            INVICTUS_PM_PenaltyMiss_Right },
    { "penalty_score_l",           INVICTUS_PM_PenaltyScore_Left },
    { "penalty_score_r",           INVICTUS_PM_PenaltyScore_Right },
    { "illegal_defense_l",         INVICTUS_PM_IllegalDefense_Left },
    { "illegal_defense_r",         INVICTUS_PM_IllegalDefense_Right },
    { "penalty_onfield_l",         INVICTUS_PM_PenaltyOnfield_Left },
    { "penalty_onfield_r",         INVICTUS_PM_PenaltyOnfield_Right },
    { "penalty_foul_l",            INVICTUS_PM_PenaltyFoul_Left },
    { "penalty_foul_r",            INVICTUS_PM_PenaltyFoul_Right },
    { "goalie_catch_ball_l",       INVICTUS_PM_GoalieCatch_Left },
    { "goalie_catch_ball_r",       INVICTUS_PM_GoalieCatch_Right },
    { "extend_half",               INVICTUS_PM_ExtendHalf },
    { NULL,                        INVICTUS_PM_NULL }
};

static InvictusPlayMode invictus_playmode_from_str(const char* s)
{
    if (!s) return INVICTUS_PM_NULL;
    for (int i = 0; invictus_playmode_table[i].name; ++i) {
        if (strcmp(s, invictus_playmode_table[i].name) == 0)
            return invictus_playmode_table[i].pm;
    }
    return INVICTUS_PM_NULL;
}

/* ------------------------------------------------------------------ */
/* Shared low-level parser helpers (char* walkers)                    */
/* These are intentionally static (header-only style).                */
/* ------------------------------------------------------------------ */

static void invictus_skip_ws(const char** s)
{
    while (**s == ' ' || **s == '\t' || **s == '\n' || **s == '\r') ++(*s);
}

static double invictus_parse_double(const char** s, double errval)
{
    invictus_skip_ws(s);
    char* end = NULL;
    double v = strtod(*s, &end);
    if (end == *s) return errval;
    *s = end;
    return v;
}

static int invictus_parse_int(const char** s, int errval)
{
    invictus_skip_ws(s);
    char* end = NULL;
    long v = strtol(*s, &end, 10);
    if (end == *s) return errval;
    *s = end;
    return (int)v;
}

static void invictus_parse_token(const char** s, char* out, int outsz)
{
    invictus_skip_ws(s);
    int i = 0;
    bool quoted = (**s == '"');
    if (quoted) ++(*s);
    while (**s && i < outsz-1) {
        char c = **s;
        if (quoted && c == '"') { ++(*s); break; }
        if (!quoted && (c == ' ' || c == ')' || c == '(')) break;
        out[i++] = c;
        ++(*s);
    }
    out[i] = '\0';
}

/* ------------------------------------------------------------------ */
/* Common protocol structs for messages/params shared by player+trainer */
/* ------------------------------------------------------------------ */

/* Stable server configuration received via (server_param), (player_param), etc.
   These values generally do not change during a game. */
typedef struct {
    bool synch_mode;
    int  slow_down_factor;   /* default 1 */
} InvictusServerParams;

/* Transient control flags driven by common messages such as (think).
   Both players and trainers receive these. */
typedef struct {
    bool think_received;
} InvictusCommonControl;

/* Reset functions for the common protocol structs (recommended
   over manual field assignment for defaults). */

static void invictus_server_params_reset(InvictusServerParams *p)
{
    if (!p) return;
    p->synch_mode = false;
    p->slow_down_factor = 1;
}

static void invictus_common_control_reset(InvictusCommonControl *c)
{
    if (!c) return;
    c->think_received = false;
}

/* Convenience: reset both common structs at once. */
static void invictus_common_reset(InvictusServerParams *params, InvictusCommonControl *control)
{
    invictus_server_params_reset(params);
    invictus_common_control_reset(control);
}

/* ------------------------------------------------------------------ */
/* Shared message helpers (ok/error logging, server_param)            */
/* ------------------------------------------------------------------ */

static void invictus_parse_ok_error(void *unused, const char *msg)
{
    (void)unused;
    if (strstr(msg, "(error ") || strstr(msg, "(warning ")) {
        invictus_log("invictus: server said: %s", msg);
    }
}

void invictus_parse_server_param(InvictusServerParams *p, const char *msg)
{
    if (!p || !msg) return;
    /* look for synch_mode */
    const char *s = strstr(msg, "synch_mode");
    if (s) {
        s += 10;
        while (*s && (*s==' ' || *s=='(' || *s==')')) ++s;
        if (*s == '1') p->synch_mode = true;
        else if (*s == '0') p->synch_mode = false;
    }
    /* look for slow_down_factor (default 1) */
    s = strstr(msg, "slow_down_factor");
    if (s) {
        s += 16;
        while (*s && (*s==' ' || *s=='(' || *s==')')) ++s;
        int v = 0;
        while (*s >= '0' && *s <= '9') { v = v*10 + (*s - '0'); ++s; }
        if (v > 0) p->slow_down_factor = v;
    }
}

/* Parser for transient common control messages (e.g. (think)).
   Callers (player and trainer) embed InvictusCommonControl and invoke
   this via invictus_parse_common or directly. */
void invictus_parse_common_control(InvictusCommonControl *c, const char *msg)
{
    if (!c || !msg) return;
    if (strncmp(msg, "(think)", 7) == 0) {
        c->think_received = true;
    }
}

/* Main common entry point. Inspects msg and delegates to the appropriate
   struct-specific parser(s). Used by player receive path and trainer code. */
void invictus_parse_common(InvictusServerParams *params, InvictusCommonControl *control, const char *msg)
{
    if (!msg || !*msg) return;

    if (strncmp(msg, "(think)", 7) == 0) {
        invictus_parse_common_control(control, msg);
    } else if (strncmp(msg, "(server_param ", 14) == 0 ||
               strncmp(msg, "(player_param ", 14) == 0 ||
               strncmp(msg, "(player_type ", 13) == 0) {
        invictus_parse_server_param(params, msg);
    }
}

/* Declarations for shared parsers implemented in player_protocol.h
   (they depend only on shared types and are useful for trainers). */
void invictus_parse_hear(InvictusPlayMode *playmode, long *current_cycle, const char *msg);
void invictus_parse_init(InvictusSide *side, int *unum, double *version, const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* INVICTUS_COMMON_PROTOCOL_H */
