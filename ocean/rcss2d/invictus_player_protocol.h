/* invictus_player_protocol.h
 *
 * Soccer protocol interface types and parsing for Invictus players.
 *
 * This pulls in shared items from invictus_common_protocol.h and adds:
 *   - Player sensor structures (Visual, Body, Sensors)
 *   - Player-specific message parsers (see, sense_body, hear, init, ...)
 *   - Player effector serialization (serialize_effectors)
 *
 * Shared: constants, Side/PlayMode + table, walker helpers, server_param.
 *
 * The raw UDP transport lives in invictus_transport.h (InvictusTransport).
 * The high-level RL player interface lives in invictus_player.h.
 *
 * Include via invictus_player.h for normal use.
 */

#ifndef INVICTUS_PLAYER_PROTOCOL_H
#define INVICTUS_PLAYER_PROTOCOL_H

#include "invictus_transport.h"
#include "invictus_common_protocol.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Player-specific protocol extensions                                */
/* Shared enums (Side/PlayMode), constants, table, and walker helpers */
/* are provided by invictus_common_protocol.h.                        */
/* ------------------------------------------------------------------ */

/* View (player-specific) */
typedef enum {
    INVICTUS_VIEW_QUALITY_HIGH = 0,
    INVICTUS_VIEW_QUALITY_LOW  = 1
} InvictusViewQuality;

typedef enum {
    INVICTUS_VIEW_WIDTH_NARROW = 0,
    INVICTUS_VIEW_WIDTH_NORMAL = 1,
    INVICTUS_VIEW_WIDTH_WIDE   = 2
} InvictusViewWidth;

/* Raw sensor data structures (exactly what the server sent) */
typedef struct {
    double dist;
    double dir;
    double dist_chng;
    double dir_chng;
} InvictusPolar;

typedef struct {
    char   obj[16];
    InvictusPolar polar;
    char   team[INVICTUS_TEAMNAME_MAX];
    int    unum;
    bool   goalie;
    double body;
    double face;
    double arm;
    bool   kicking;
} InvictusSeenObj;

typedef struct {
    long          time;
    int           num_objs;
    InvictusSeenObj objs[INVICTUS_MAX_SEEN_OBJS];
    int           num_balls;
    InvictusPolar balls[2];
    int           num_players;
    InvictusSeenObj players[INVICTUS_MAX_PLAYERS_SEEN];
    int           num_markers;
    InvictusSeenObj markers[30];
} InvictusVisual;

typedef struct {
    long   time;
    InvictusViewQuality view_quality;
    InvictusViewWidth   view_width;
    double stamina;
    double effort;
    double stamina_capacity;
    double speed_mag;
    double speed_dir;
    double head_angle;
    int    kick_count;
    int    dash_count;
    int    turn_count;
    int    say_count;
    int    turn_neck_count;
    int    catch_count;
    int    move_count;
    int    change_view_count;
    int    arm_movable;
    int    arm_expires;
    double arm_target_x;
    double arm_target_y;
    int    arm_count;
    char   focus_target_side[8];
    int    focus_target_unum;
    int    focus_count;
    int    tackle_expires;
    int    tackle_count;
    bool   collision_ball;
    bool   collision_player;
    bool   collision_post;
} InvictusBody;

/* Aggregated raw sensors (visual + body). Stored as a single member on InvictusPlayer. */
typedef struct {
    InvictusVisual visual;
    InvictusBody   body;
} InvictusSensors;

/* ------------------------------------------------------------------ */
/* Effectors (command intent). Player populates; commit_effectors     */
/* (in this layer) converts to wire protocol text for the transport.  */
/* ------------------------------------------------------------------ */

typedef enum {
    INVICTUS_BODY_CMD_NONE = 0,
    INVICTUS_BODY_CMD_DASH,
    INVICTUS_BODY_CMD_TURN,
    INVICTUS_BODY_CMD_KICK,
    INVICTUS_BODY_CMD_TACKLE,
    INVICTUS_BODY_CMD_CATCH,
    INVICTUS_BODY_CMD_MOVE
} InvictusBodyCmd;

typedef struct {
    InvictusBodyCmd type;
    double p1;
    double p2;
} InvictusBodyEffector;

typedef struct {
    bool   active;
    double moment;
} InvictusTurnNeckEffector;

typedef struct {
    bool                active;
    InvictusViewWidth   width;
} InvictusChangeViewEffector;

typedef struct {
    bool  active;
    char  message[256];
} InvictusSayEffector;

typedef struct {
    InvictusBodyEffector       body;
    InvictusTurnNeckEffector   turn_neck;
    InvictusChangeViewEffector change_view;
    InvictusSayEffector        say;
} InvictusEffectors;

/* ------------------------------------------------------------------ */
/* Protocol (de)serialization API                                     */
/* These operate on client-defined types.                             */
/* parse_* are deserializers (populate from wire text).               */
/* serialize_* produce wire text from structs (no transport side      */
/* effects). Callers send via transport.                              */
/* ------------------------------------------------------------------ */

void invictus_parse_see(InvictusVisual *v, long *current_cycle, const char *msg);
void invictus_parse_sense_body(InvictusBody *b, long *current_cycle, const char *msg);
void invictus_parse_hear(InvictusPlayMode *playmode, long *current_cycle, const char *msg);
void invictus_parse_init(InvictusSide *side, int *unum, double *version, const char *msg);

/* High-level message parser taking the relevant sub-objects.
   Common params/control (synch, think, etc.) are handled via
   invictus_parse_common in the common protocol. */
void invictus_parse_message(InvictusVisual *see,
                            InvictusBody *body,
                            InvictusPlayMode *playmode,
                            long *current_cycle,
                            InvictusSide *side,
                            int *unum,
                            double *version,
                            const char *msg);

/* Serialize active effectors to a buffer (concatenated protocol cmds).
 * If synch_mode, appends (done). Clears the effectors after reading.
 * Returns 0 on success. Buffer must be large enough (use INVICTUS_MAX_MESG). */
int invictus_serialize_effectors(InvictusEffectors* effectors, bool synch_mode, char* buf, size_t bufsz);

/* ------------------------------------------------------------------ */
/* Implementation (protocol + bridge)                                 */
/* ------------------------------------------------------------------ */

/* === Internal utilities & sensor clearing (protocol support) ======== */

static void invictus_clear_visual(InvictusVisual* v)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
    v->time = -1;
    for (int i = 0; i < INVICTUS_MAX_SEEN_OBJS; ++i) {
        v->objs[i].unum = -1;
        v->objs[i].polar.dist = 1e6;
        v->objs[i].polar.dir  = -360.0;
    }
}

static void invictus_clear_body(InvictusBody* b)
{
    if (!b) return;
    memset(b, 0, sizeof(*b));
    b->time = -1;
    b->stamina = 4000.0;
    b->effort  = 1.0;
    b->speed_mag = 0.0;
    b->speed_dir = 0.0;
    b->head_angle = 0.0;
    b->focus_target_unum = -1;
    b->arm_movable = 0;
    b->tackle_expires = 0;
}

/* === Parse implementations === */

void invictus_parse_see(InvictusVisual *v, long *current_cycle, const char *msg)
{
    if (!v || !msg) return;
    const char* s = msg;

    while (*s && *s != ' ') ++s;
    invictus_skip_ws(&s);

    long t = (long)invictus_parse_double(&s, -1);
    if (t < 0) t = *current_cycle;

    invictus_clear_visual(v);
    v->time = t;

    int obj_idx = 0;
    while (*s && *s != ')' && obj_idx < INVICTUS_MAX_SEEN_OBJS) {
        invictus_skip_ws(&s);
        if (*s != '(') break;

        InvictusSeenObj* o = &v->objs[obj_idx];
        memset(o, 0, sizeof(*o));
        o->polar.dist = 1e6;
        o->polar.dir  = -360.0;
        o->unum = -1;

        ++s;
        invictus_skip_ws(&s);
        if (*s == '(') ++s; /* consume inner name-group ( */
        invictus_skip_ws(&s);

        char first[16] = {0};
        invictus_parse_token(&s, first, sizeof(first));
        strncpy(o->obj, first, sizeof(o->obj)-1);

        if (first[0] == 'b' || first[0] == 'B') {
            /* consume remaining name tokens to the name's ) */
            while (*s && *s != ')') {
                invictus_skip_ws(&s);
                if (*s == ')') break;
                char tmp[16]; invictus_parse_token(&s, tmp, sizeof(tmp));
            }
            if (*s == ')') ++s;

            o->polar.dist = invictus_parse_double(&s, 1e6);
            o->polar.dir  = invictus_parse_double(&s, -360);
            o->polar.dist_chng = invictus_parse_double(&s, 0);
            o->polar.dir_chng  = invictus_parse_double(&s, 0);
            if (v->num_balls < 2)
                v->balls[v->num_balls++] = o->polar;
        } else if (first[0] == 'p' || first[0] == 'P') {
            /* identity (team/unum/goalie) is still inside the name group */
            invictus_skip_ws(&s);
            if (*s == '"') {
                invictus_parse_token(&s, o->team, sizeof(o->team));
            }
            int maybe_unum = invictus_parse_int(&s, -1);
            if (maybe_unum >= 0 && maybe_unum <= 11) {
                o->unum = maybe_unum;
            }
            /* optional goalie indicator inside name group */
            invictus_skip_ws(&s);
            if (*s == 'g' || *s == 'G') {
                o->goalie = true;
                char gtmp[16]; invictus_parse_token(&s, gtmp, sizeof(gtmp));
            }
            /* consume any leftover tokens in name group until ) */
            while (*s && *s != ')') {
                invictus_skip_ws(&s);
                if (*s == ')') break;
                char tmp[16]; invictus_parse_token(&s, tmp, sizeof(tmp));
            }
            if (*s == ')') ++s;

            /* now the numeric values after name group */
            o->polar.dist = invictus_parse_double(&s, 1e6);
            o->polar.dir  = invictus_parse_double(&s, -360);
            o->polar.dist_chng = invictus_parse_double(&s, 0);
            o->polar.dir_chng  = invictus_parse_double(&s, 0);
            o->body = invictus_parse_double(&s, -360);
            o->face = invictus_parse_double(&s, -360);
            o->arm = invictus_parse_double(&s, -360);
            invictus_skip_ws(&s);
            if (*s == '0' || *s == '1') {
                o->kicking = (*s == '1');
                ++s;
            }
            if (v->num_players < INVICTUS_MAX_PLAYERS_SEEN)
                v->players[v->num_players++] = *o;
        } else {
            /* marker/flag/goal/line: consume remaining name tokens (e.g. r, b, 30) to ) */
            while (*s && *s != ')') {
                invictus_skip_ws(&s);
                if (*s == ')') break;
                char tmp[16]; invictus_parse_token(&s, tmp, sizeof(tmp));
            }
            if (*s == ')') ++s;

            o->polar.dist = invictus_parse_double(&s, 1e6);
            o->polar.dir  = invictus_parse_double(&s, -360);
            /* some markers report velocity */
            o->polar.dist_chng = invictus_parse_double(&s, 0);
            o->polar.dir_chng  = invictus_parse_double(&s, 0);
            if (v->num_markers < 30)
                v->markers[v->num_markers++] = *o;
        }

        int depth = 1;
        while (*s && depth > 0) {
            if (*s == '(') ++depth;
            else if (*s == ')') --depth;
            ++s;
        }
        ++obj_idx;
        v->num_objs = obj_idx;
    }

    if (t > *current_cycle) *current_cycle = t;
}

void invictus_parse_sense_body(InvictusBody *b, long *current_cycle, const char *msg)
{
    if (!b || !msg) return;
    const char* s = msg;

    invictus_clear_body(b);

    while (*s && *s != ' ') ++s;
    invictus_skip_ws(&s);

    b->time = (long)invictus_parse_double(&s, *current_cycle);

    while (*s && *s != ')') {
        invictus_skip_ws(&s);
        if (*s != '(') { ++s; continue; }

        ++s;
        invictus_skip_ws(&s);

        char key[32] = {0};
        invictus_parse_token(&s, key, sizeof(key));

        if (strcmp(key, "view_mode") == 0) {
            char q[8], w[8];
            invictus_parse_token(&s, q, sizeof(q));
            invictus_parse_token(&s, w, sizeof(w));
            b->view_quality = (strstr(q,"low") || strstr(q,"LOW")) ? INVICTUS_VIEW_QUALITY_LOW : INVICTUS_VIEW_QUALITY_HIGH;
            if (strstr(w,"narrow") || strstr(w,"NARROW")) b->view_width = INVICTUS_VIEW_WIDTH_NARROW;
            else if (strstr(w,"wide") || strstr(w,"WIDE")) b->view_width = INVICTUS_VIEW_WIDTH_WIDE;
            else b->view_width = INVICTUS_VIEW_WIDTH_NORMAL;
        } else if (strcmp(key, "stamina") == 0) {
            b->stamina = invictus_parse_double(&s, 4000.0);
            b->effort  = invictus_parse_double(&s, 1.0);
            b->stamina_capacity = invictus_parse_double(&s, 0.0);
        } else if (strcmp(key, "speed") == 0) {
            b->speed_mag = invictus_parse_double(&s, 0.0);
            b->speed_dir = invictus_parse_double(&s, 0.0);
        } else if (strcmp(key, "head_angle") == 0) {
            b->head_angle = invictus_parse_double(&s, 0.0);
        } else if (strcmp(key, "kick") == 0) {
            b->kick_count = invictus_parse_int(&s, 0);
        } else if (strcmp(key, "dash") == 0) {
            b->dash_count = invictus_parse_int(&s, 0);
        } else if (strcmp(key, "turn") == 0) {
            b->turn_count = invictus_parse_int(&s, 0);
        } else if (strcmp(key, "say") == 0) {
            b->say_count = invictus_parse_int(&s, 0);
        } else if (strcmp(key, "turn_neck") == 0) {
            b->turn_neck_count = invictus_parse_int(&s, 0);
        } else if (strcmp(key, "catch") == 0) {
            b->catch_count = invictus_parse_int(&s, 0);
        } else if (strcmp(key, "move") == 0) {
            b->move_count = invictus_parse_int(&s, 0);
        } else if (strcmp(key, "change_view") == 0) {
            b->change_view_count = invictus_parse_int(&s, 0);
        } else if (strcmp(key, "arm") == 0) {
            while (*s && *s != ')') {
                invictus_skip_ws(&s);
                if (*s != '(') { ++s; continue; }
                ++s;
                char sub[16]; invictus_parse_token(&s, sub, sizeof(sub));
                if (strcmp(sub,"movable")==0) b->arm_movable = invictus_parse_int(&s, 0);
                else if (strcmp(sub,"expires")==0) b->arm_expires = invictus_parse_int(&s, 0);
                else if (strcmp(sub,"target")==0) {
                    b->arm_target_x = invictus_parse_double(&s, 0);
                    b->arm_target_y = invictus_parse_double(&s, 0);
                } else if (strcmp(sub,"count")==0) b->arm_count = invictus_parse_int(&s, 0);
                int d=1; while(*s && d>0){ if(*s=='(')d++; else if(*s==')')d--; ++s; }
            }
        } else if (strcmp(key, "focus") == 0) {
            while (*s && *s != ')') {
                invictus_skip_ws(&s);
                if (*s != '('){ ++s; continue; }
                ++s;
                char sub[16]; invictus_parse_token(&s, sub, sizeof(sub));
                if (strcmp(sub,"target")==0) {
                    invictus_parse_token(&s, b->focus_target_side, sizeof(b->focus_target_side));
                    b->focus_target_unum = invictus_parse_int(&s, -1);
                } else if (strcmp(sub,"count")==0) {
                    b->focus_count = invictus_parse_int(&s, 0);
                }
                int d=1; while(*s && d>0){ if(*s=='(')d++; else if(*s==')')d--; ++s; }
            }
        } else if (strcmp(key, "tackle") == 0) {
            while (*s && *s != ')') {
                invictus_skip_ws(&s);
                if (*s != '('){ ++s; continue; }
                ++s;
                char sub[16]; invictus_parse_token(&s, sub, sizeof(sub));
                if (strcmp(sub,"expires")==0) b->tackle_expires = invictus_parse_int(&s, 0);
                else if (strcmp(sub,"count")==0) b->tackle_count = invictus_parse_int(&s, 0);
                int d=1; while(*s && d>0){ if(*s=='(')d++; else if(*s==')')d--; ++s; }
            }
        } else if (strcmp(key, "collision") == 0) {
            invictus_skip_ws(&s);
            if (strstr(s, "ball"))   b->collision_ball = true;
            if (strstr(s, "player")) b->collision_player = true;
            if (strstr(s, "post"))   b->collision_post = true;
        }
        int depth = 1;
        while (*s && depth > 0) {
            if (*s == '(') ++depth;
            else if (*s == ')') --depth;
            ++s;
        }
    }

    if (b->time > *current_cycle) *current_cycle = b->time;
}

void invictus_parse_hear(InvictusPlayMode *playmode, long *current_cycle, const char *msg)
{
    if (!playmode || !msg) return;
    const char* s = msg;
    while (*s && *s != ' ') ++s;
    invictus_skip_ws(&s);
    long t = (long)invictus_parse_double(&s, -1);
    if (t > *current_cycle) *current_cycle = t;

    invictus_skip_ws(&s);
    char sender[32];
    invictus_parse_token(&s, sender, sizeof(sender));

    if (strcmp(sender, "referee") == 0) {
        char mode[64];
        invictus_parse_token(&s, mode, sizeof(mode));
        *playmode = invictus_playmode_from_str(mode);
    }
}

void invictus_parse_init(InvictusSide *side, int *unum, double *version, const char *msg)
{
    if (!side || !msg) return;
    const char* s = msg;
    while (*s && *s != ' ') ++s;
    invictus_skip_ws(&s);

    char sidec[4] = {0};
    invictus_parse_token(&s, sidec, sizeof(sidec));
    *side = (sidec[0] == 'l' || sidec[0] == 'L') ? INVICTUS_SIDE_LEFT :
            (sidec[0] == 'r' || sidec[0] == 'R') ? INVICTUS_SIDE_RIGHT : INVICTUS_SIDE_UNKNOWN;

    *unum = invictus_parse_int(&s, -1);

    double ver = invictus_parse_double(&s, 0.0);
    if (ver > 0.0) *version = ver;
}

/* The main entry point for parsing a message into the provided objects.
   Player-specific messages only. Common (think / server params) are
   handled by calling invictus_parse_common from the common protocol. */
void invictus_parse_message(InvictusVisual *see,
                            InvictusBody *body,
                            InvictusPlayMode *playmode,
                            long *current_cycle,
                            InvictusSide *side,
                            int *unum,
                            double *version,
                            const char *msg)
{
    if (!msg || !*msg) return;

    if (strncmp(msg, "(see ", 5) == 0) {
        invictus_parse_see(see, current_cycle, msg);
    } else if (strncmp(msg, "(sense_body ", 12) == 0) {
        invictus_parse_sense_body(body, current_cycle, msg);
    } else if (strncmp(msg, "(hear ", 6) == 0) {
        invictus_parse_hear(playmode, current_cycle, msg);
    } else if (strncmp(msg, "(init ", 6) == 0 || strncmp(msg, "(reconnect ", 11) == 0) {
        invictus_parse_init(side, unum, version, msg);
    } else if (strncmp(msg, "(ok ", 4) == 0) {
        invictus_parse_ok_error(NULL, msg);
    } else if (strncmp(msg, "(error ", 7) == 0 || strncmp(msg, "(warning ", 9) == 0) {
        invictus_parse_ok_error(NULL, msg);
    } else if (strncmp(msg, "(fullstate ", 11) == 0) {
        /* fullstate handling (if needed) can be added here */
    } else {
        /* unknown or handled at common layer (think, server_param, ...) */
    }
}

int invictus_serialize_effectors(InvictusEffectors* e, bool synch_mode, char* buf, size_t bufsz)
{
    if (!e || !buf || bufsz == 0) {
        invictus_log("invictus: serialize_effectors: invalid args");
        return -1;
    }

    int pos = 0;
    #define APPEND(fmt, ...) do { \
        if (pos < (int)bufsz - 1) { \
            int n = snprintf(buf + pos, bufsz - pos, fmt, ##__VA_ARGS__); \
            if (n > 0) pos += n; \
        } \
    } while (0)

    if (e->body.type != INVICTUS_BODY_CMD_NONE) {
        switch (e->body.type) {
        case INVICTUS_BODY_CMD_DASH:
            APPEND("(dash %.2f %.2f)", e->body.p1, e->body.p2);
            break;
        case INVICTUS_BODY_CMD_TURN:
            APPEND("(turn %.2f)", e->body.p1);
            break;
        case INVICTUS_BODY_CMD_KICK:
            APPEND("(kick %.2f %.2f)", e->body.p1, e->body.p2);
            break;
        case INVICTUS_BODY_CMD_TACKLE:
            if (e->body.p2 > 0.5) {
                APPEND("(tackle %.2f on)", e->body.p1);
            } else {
                APPEND("(tackle %.2f)", e->body.p1);
            }
            break;
        case INVICTUS_BODY_CMD_CATCH:
            APPEND("(catch 0.0)");
            break;
        case INVICTUS_BODY_CMD_MOVE:
            APPEND("(move %.2f %.2f)", e->body.p1, e->body.p2);
            break;
        default:
            break;
        }
    }

    if (e->turn_neck.active) {
        APPEND("(turn_neck %.2f)", e->turn_neck.moment);
    }

    if (e->change_view.active) {
        const char* w = (e->change_view.width == INVICTUS_VIEW_WIDTH_NARROW) ? "narrow" :
                        (e->change_view.width == INVICTUS_VIEW_WIDTH_WIDE)   ? "wide" : "normal";
        APPEND("(change_view %s high)", w);
    }

    if (e->say.active && e->say.message[0]) {
        APPEND("(say \"%s\")", e->say.message);
    }

    if (synch_mode) {
        APPEND("(done)");
    }

    buf[pos] = '\0';

    /* one-shot: clear so they are not re-committed */
    memset(e, 0, sizeof(*e));
    return 0;

    #undef APPEND
}

#ifdef __cplusplus
}
#endif

#endif /* INVICTUS_PLAYER_PROTOCOL_H */
