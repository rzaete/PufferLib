/* invictus_trainer_protocol.h
 *
 * Soccer protocol types and parsing for Invictus trainer / offline coach.
 *
 * This provides:
 *   - Trainer (offline coach) sensor structures (global Cartesian view)
 *   - Trainer effector/command structures
 *   - Message parsers for trainer responses:
 *       (see_global ...), (ok look ...), (hear ...), (think),
 *       (init ok), server/player params, (ok ...), (error ...)
 *   - Command serialization (move, change_mode, look, eye, recover, say, done, ...)
 *     via invictus_serialize_trainer_commands
 *
 * Shared items (Side, PlayMode, basic walkers, parse_server_param) are
 * pulled from invictus_common_protocol.h.
 *
 * The raw UDP transport lives in invictus_transport.h (InvictusTransport).
 * Connect trainers to the coach_port port (default 6001).
 * Init message format: (init (version 18.0))
 *
 * Include via a future invictus_trainer.h or use directly with the client.
 * Direct field access on public structs (no accessors).
 */

#ifndef INVICTUS_TRAINER_PROTOCOL_H
#define INVICTUS_TRAINER_PROTOCOL_H

#include "invictus_transport.h"
#include "invictus_common_protocol.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Trainer protocol types                                             */
/* ------------------------------------------------------------------ */

#define INVICTUS_TRAINER_MAX_PLAYERS 22

/* Global (Cartesian) view for trainers/coaches */
typedef struct {
    double x;
    double y;
    double vx;
    double vy;
} InvictusTrainerBall;

typedef struct {
    char   team[INVICTUS_TEAMNAME_MAX];
    int    unum;
    bool   goalie;
    double x;
    double y;
    double vx;
    double vy;
    double body;
    double neck;
    double arm;             /* point dir if present */
    bool   kicking;
    bool   tackling;
    bool   charged;         /* foul charged */
    int    card;            /* 0=none, 1=yellow, 2=red */
} InvictusTrainerPlayer;

typedef struct {
    long                time;
    InvictusTrainerBall ball;
    InvictusTrainerPlayer players[INVICTUS_TRAINER_MAX_PLAYERS];
    int                 num_players;
} InvictusTrainerVisual;

/* ------------------------------------------------------------------ */
/* Trainer effectors / commands                                       */
/* Populated by caller; commit_trainer_commands turns them into       */
/* wire protocol and clears. Supports (done) in synch mode.           */
/* ------------------------------------------------------------------ */

typedef struct {
    bool   active;
    double x;
    double y;
    double vx;
    double vy;
    bool   has_vel;
} InvictusTrainerBallMove;

typedef struct {
    bool   active;
    char   team[INVICTUS_TEAMNAME_MAX];
    int    unum;
    double x;
    double y;
    double angle;
    double vx;
    double vy;
    bool   has_angle;
    bool   has_vel;
} InvictusTrainerPlayerMove;

typedef struct {
    /* moves (at most one of each expected per cycle) */
    InvictusTrainerBallMove   ball_move;
    InvictusTrainerPlayerMove player_move;

    /* mode change */
    bool             change_mode;
    InvictusPlayMode new_playmode;

    /* one-shot actions */
    bool look;
    bool recover;
    bool check_ball;
    bool kickoff_start;   /* (start) */

    /* toggles */
    bool eye;
    bool eye_on;
    bool ear;
    bool ear_on;

    /* say */
    bool say;
    char say_msg[128];

    /* hetero */
    bool change_player_type;
    char cpt_team[INVICTUS_TEAMNAME_MAX];
    int  cpt_unum;
    int  cpt_ptype;
} InvictusTrainerEffectors;

/* ------------------------------------------------------------------ */
/* Public parser API                                                  */
/* ------------------------------------------------------------------ */

void invictus_parse_see_global(InvictusTrainerVisual *v, long *current_cycle, const char *msg);
void invictus_parse_ok_look(InvictusTrainerVisual *v, long *current_cycle, const char *msg);

/* High level dispatch for a trainer message.
   Common flags (think_received, synch_mode, slow_down_factor) are populated
   by calling invictus_parse_common (common protocol) separately. */
void invictus_parse_trainer_message(InvictusTrainerVisual *visual,
                                    InvictusPlayMode *playmode,
                                    long *current_cycle,
                                    const char *msg);

/* Serialize trainer effectors to a buffer (concatenated protocol cmds).
   If synch_mode, appends (done). Clears effectors. */
int invictus_serialize_trainer_commands(InvictusTrainerEffectors* effectors, bool synch_mode, char* buf, size_t bufsz);

/* ------------------------------------------------------------------ */
/* Implementation                                                     */
/* ------------------------------------------------------------------ */

static void invictus_clear_trainer_visual(InvictusTrainerVisual* v)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
    v->time = -1;
    v->ball.x = v->ball.y = v->ball.vx = v->ball.vy = 0.0;
    for (int i = 0; i < INVICTUS_TRAINER_MAX_PLAYERS; ++i) {
        v->players[i].unum = -1;
        v->players[i].x = v->players[i].y = 1e6;
        v->players[i].vx = v->players[i].vy = 0;
        v->players[i].body = v->players[i].neck = -360;
        v->players[i].arm = -360;
    }
}

/* ------------------------------------------------------------------ */
/* Trainer visual parsers (v7+ see_global / ok look)                  */
/* Format (short names):
 * (see_global TIME ((g l) gx gy) ((g r) gx gy) ((b) bx by vx vy)
 *    ((p "TEAM" UNUM [g]) x y vx vy body neck [arm] [t|k|f] [y|r]) ...)
 *
 * (ok look TIME ...) is identical in content.
 * ------------------------------------------------------------------ */

static void invictus_parse_trainer_visual_common(InvictusTrainerVisual *v, long *current_cycle, const char *msg, bool is_look)
{
    (void)is_look; /* both formats use same body for now */
    if (!v || !msg) return;

    const char *s = msg;

    invictus_clear_trainer_visual(v);

    /* advance past prefix */
    if (strncmp(s, "(see_global ", 12) == 0) {
        s += 12;
    } else if (strncmp(s, "(ok look ", 9) == 0) {
        s += 9;
    } else {
        /* try to find first number anyway */
        while (*s && *s != ' ' && *s != '-' && (*s < '0' || *s > '9')) ++s;
    }

    /* time */
    invictus_skip_ws(&s);
    long t = (long)invictus_parse_double(&s, -1);
    if (t < 0) t = *current_cycle;
    v->time = t;

    /* skip goals: two ((g x) x y) objects */
    int depth = 0;
    int goal_count = 0;
    while (*s && goal_count < 2) {
        if (*s == '(') { ++depth; }
        else if (*s == ')') { --depth; if (depth == 0) ++goal_count; }
        ++s;
        if (goal_count >= 2) break;
    }
    /* now at after goals, expect ball */

    invictus_skip_ws(&s);

    /* parse ball ((b) x y vx vy) */
    if (*s == '(') {
        /* consume the whole ball sexp */
        /* expect "((b) " */
        while (*s && *s != '(') ++s; /* already at ( */
        /* skip to after name */
        while (*s && *s != ')') ++s;
        ++s; /* past ) of name */
        invictus_skip_ws(&s);
        v->ball.x  = invictus_parse_double(&s, 0.0);
        v->ball.y  = invictus_parse_double(&s, 0.0);
        v->ball.vx = invictus_parse_double(&s, 0.0);
        v->ball.vy = invictus_parse_double(&s, 0.0);
        /* consume to matching ) */
        depth = 1;
        while (*s && depth > 0) {
            if (*s == '(') ++depth;
            else if (*s == ')') --depth;
            ++s;
        }
    }

    /* players */
    int pidx = 0;
    while (*s && pidx < INVICTUS_TRAINER_MAX_PLAYERS) {
        invictus_skip_ws(&s);
        if (*s != '(') break;

        /* start of ((p "T" N ...) or ((p "T" N g) ...) */
        ++s; /* ( */
        invictus_skip_ws(&s);
        if (*s != '(') { /* malformed, skip this token group */
            depth = 1; while (*s && depth>0){ if(*s=='(')++depth; else if(*s==')')--depth; ++s; } continue;
        }
        ++s; /* inner ( */
        invictus_skip_ws(&s);

        char tag[4] = {0};
        invictus_parse_token(&s, tag, sizeof(tag));
        if (tag[0] != 'p' && tag[0] != 'P' && tag[0] != 'b' && tag[0] != 'B') {
            /* goal or other, skip whole object */
            depth = 1; while (*s && depth > 0){ if(*s=='(')++depth; else if(*s==')')--depth; ++s; }
            continue;
        }

        if (pidx >= INVICTUS_TRAINER_MAX_PLAYERS) break;

        InvictusTrainerPlayer *pl = &v->players[pidx];
        memset(pl, 0, sizeof(*pl));
        pl->unum = -1;
        pl->body = pl->neck = pl->arm = -360.0;

        /* team name (quoted) */
        invictus_skip_ws(&s);
        invictus_parse_token(&s, pl->team, sizeof(pl->team));

        /* unum */
        invictus_skip_ws(&s);
        pl->unum = invictus_parse_int(&s, -1);

        /* optional goalie flag 'g' or 'G' immediately after unum before ) */
        invictus_skip_ws(&s);
        if (*s == 'g' || *s == 'G') {
            pl->goalie = true;
            ++s;
        }

        /* close the name group */
        while (*s && *s != ')') ++s;
        if (*s == ')') ++s;

        /* now numbers: x y vx vy body neck ... */
        pl->x   = invictus_parse_double(&s, 1e6);
        pl->y   = invictus_parse_double(&s, 1e6);
        pl->vx  = invictus_parse_double(&s, 0.0);
        pl->vy  = invictus_parse_double(&s, 0.0);
        pl->body = invictus_parse_double(&s, -360);
        pl->neck = invictus_parse_double(&s, -360);

        /* optional extra fields until the closing ) of this player object */
        while (*s && *s != ')') {
            invictus_skip_ws(&s);
            if (*s == ')' || *s == '\0') break;

            if (*s == 't' || *s == 'T') { pl->tackling = true; ++s; continue; }
            if (*s == 'k' || *s == 'K') { pl->kicking = true; ++s; continue; }
            if (*s == 'f' || *s == 'F') { pl->charged = true; ++s; continue; }
            if (*s == 'y' || *s == 'Y') { pl->card = 1; ++s; continue; }
            if (*s == 'r' || *s == 'R') { pl->card = 2; ++s; continue; }

            /* arm / point dir is a number */
            char *endp = NULL;
            double maybe_arm = strtod(s, &endp);
            if (endp != s && maybe_arm > -360 && maybe_arm < 360) {
                pl->arm = maybe_arm;
                s = endp;
            } else {
                /* unknown token, advance */
                while (*s && *s != ' ' && *s != ')') ++s;
            }
        }

        /* consume final ) of player */
        while (*s && *s != ')') ++s;
        if (*s == ')') ++s;

        ++pidx;
        v->num_players = pidx;
    }

    if (t > *current_cycle) *current_cycle = t;
}

void invictus_parse_see_global(InvictusTrainerVisual *v, long *current_cycle, const char *msg)
{
    invictus_parse_trainer_visual_common(v, current_cycle, msg, false);
}

void invictus_parse_ok_look(InvictusTrainerVisual *v, long *current_cycle, const char *msg)
{
    invictus_parse_trainer_visual_common(v, current_cycle, msg, true);
}

/* ------------------------------------------------------------------ */
/* Trainer message dispatcher                                         */
/* ------------------------------------------------------------------ */

void invictus_parse_trainer_message(InvictusTrainerVisual *visual,
                                    InvictusPlayMode *playmode,
                                    long *current_cycle,
                                    const char *msg)
{
    if (!msg || !*msg) return;

    if (strncmp(msg, "(see_global ", 12) == 0) {
        invictus_parse_see_global(visual, current_cycle, msg);
    } else if (strncmp(msg, "(ok look ", 9) == 0) {
        invictus_parse_ok_look(visual, current_cycle, msg);
    } else if (strncmp(msg, "(hear ", 6) == 0) {
        /* reuse the player hear parser for referee playmode */
        invictus_parse_hear(playmode, current_cycle, msg);
    } else if (strncmp(msg, "(init ", 6) == 0) {
        /* (init ok) or errors - nothing to parse into identity for trainer */
    } else if (strncmp(msg, "(ok ", 4) == 0) {
        invictus_parse_ok_error(NULL, msg);
    } else if (strncmp(msg, "(error ", 7) == 0 || strncmp(msg, "(warning ", 9) == 0) {
        invictus_parse_ok_error(NULL, msg);
    } else if (strncmp(msg, "(change_player_type ", 20) == 0) {
        /* server echoes sometimes; ignore for now */
    } else {
        /* unknown or common params (synch/slow) handled via invictus_parse_common */
    }
}

/* ------------------------------------------------------------------ */
/* Trainer command commit (effectors -> wire)                         */
/* ------------------------------------------------------------------ */

int invictus_serialize_trainer_commands(InvictusTrainerEffectors* e, bool synch_mode, char* buf, size_t bufsz)
{
    if (!e || !buf || bufsz == 0) {
        invictus_log("invictus: serialize_trainer_commands: invalid args");
        return -1;
    }

    int pos = 0;
    #define APPEND(fmt, ...) do { \
        if (pos < (int)bufsz - 1) { \
            int n = snprintf(buf + pos, bufsz - pos, fmt, ##__VA_ARGS__); \
            if (n > 0) pos += n; \
        } \
    } while (0)

    /* ball move */
    if (e->ball_move.active) {
        if (e->ball_move.has_vel) {
            APPEND("(move (ball) %.2f %.2f 0 %.2f %.2f)",
                   e->ball_move.x, e->ball_move.y,
                   e->ball_move.vx, e->ball_move.vy);
        } else {
            APPEND("(move (ball) %.2f %.2f)",
                   e->ball_move.x, e->ball_move.y);
        }
    }

    /* player move */
    if (e->player_move.active && e->player_move.team[0] && e->player_move.unum > 0) {
        if (e->player_move.has_vel && e->player_move.has_angle) {
            APPEND("(move (player %s %d) %.2f %.2f %.2f %.2f %.2f)",
                   e->player_move.team, e->player_move.unum,
                   e->player_move.x, e->player_move.y,
                   e->player_move.angle,
                   e->player_move.vx, e->player_move.vy);
        } else if (e->player_move.has_angle) {
            APPEND("(move (player %s %d) %.2f %.2f %.2f)",
                   e->player_move.team, e->player_move.unum,
                   e->player_move.x, e->player_move.y,
                   e->player_move.angle);
        } else {
            APPEND("(move (player %s %d) %.2f %.2f)",
                   e->player_move.team, e->player_move.unum,
                   e->player_move.x, e->player_move.y);
        }
    }

    /* change mode */
    if (e->change_mode) {
        /* use the name from table (they match wire format) */
        const char *mname = "play_on";
        for (int i = 0; invictus_playmode_table[i].name; ++i) {
            if (invictus_playmode_table[i].pm == e->new_playmode) {
                mname = invictus_playmode_table[i].name;
                break;
            }
        }
        APPEND("(change_mode %s)", mname);
    }

    if (e->look) {
        APPEND("(look)");
    }
    if (e->recover) {
        APPEND("(recover)");
    }
    if (e->check_ball) {
        APPEND("(check_ball)");
    }
    if (e->kickoff_start) {
        APPEND("(start)");
    }

    if (e->eye) {
        APPEND("(eye %s)", e->eye_on ? "on" : "off");
    }
    if (e->ear) {
        APPEND("(ear %s)", e->ear_on ? "on" : "off");
    }

    if (e->say && e->say_msg[0]) {
        APPEND("(say \"%s\")", e->say_msg);
    }

    if (e->change_player_type && e->cpt_team[0] && e->cpt_unum > 0) {
        APPEND("(change_player_type %s %d %d)",
               e->cpt_team, e->cpt_unum, e->cpt_ptype);
    }

    if (synch_mode) {
        APPEND("(done)");
    }

    buf[pos] = '\0';

    /* clear for next cycle */
    memset(e, 0, sizeof(*e));
    return 0;

    #undef APPEND
}

#ifdef __cplusplus
}
#endif

#endif /* INVICTUS_TRAINER_PROTOCOL_H */
