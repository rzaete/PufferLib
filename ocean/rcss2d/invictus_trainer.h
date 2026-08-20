/* invictus_trainer.h
 *
 * Soccer trainer (offline coach) interface.
 *
 * This header pulls in invictus_trainer_protocol.h (protocol + types), which in turn
 * pulls in invictus_transport.h and invictus_common_protocol.h, and layers on top:
 *   - High-level trainer lifecycle (reset, wait, send, close)
 *   - Effector population via invictus_trainer_do_* (populate; send_action serializes)
 *
 * === Recommended API ===
 *   int invictus_trainer_reset(InvictusTrainer* t, const char* host, int port);
 *   int invictus_trainer_wait_for_decision(InvictusTrainer* t);
 *   int invictus_trainer_send_action(InvictusTrainer* t);   // commits current effectors (+ (done) if synch); sends each cmd separately for server parser compatibility
 *   int invictus_trainer_close(InvictusTrainer* t);
 *
 *   // populate before send_action:
 *   invictus_trainer_do_ball_move(...)
 *   invictus_trainer_do_player_move(...)
 *   invictus_trainer_do_change_mode(...)
 *   invictus_trainer_do_look / recover / check_ball / start / eye / ear / say / change_player_type
 *
 * All high-level functions return int: 0 = success, <0 = error.
 * Caller owns the InvictusTrainer storage (no malloc/free inside).
 *
 * Direct field access is the supported style:
 *   t->visual, t->effectors, t->server_params.synch_mode, t->common_control.think_received,
 *   t->action_required, t->server_alive, &t->comm, etc.
 *
 * Connects to the trainer/coach_port port (commonly 6001). Init is (init (version 18.0)).
 *
 * Usage: just #include "invictus_trainer.h"
 * (complete single-header implementation; include once in your program).
 */

#ifndef INVICTUS_TRAINER_H
#define INVICTUS_TRAINER_H

#include "invictus_player_protocol.h"   /* pulls bodies for parse_hear / parse_init (declared for shared use by trainer) */
#include "invictus_trainer_protocol.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Trainer client state                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    /* communication layer */
    InvictusTransport comm;

    /* sensors (global Cartesian view) */
    InvictusTrainerVisual visual;

    /* effectors: populated by invictus_trainer_do_* ; sent by send_action */
    InvictusTrainerEffectors effectors;

    /* minimal game context */
    long             current_cycle;
    InvictusPlayMode playmode;

    /* command / action state */
    bool action_required;

    /* common protocol state */
    InvictusServerParams  server_params;
    InvictusCommonControl common_control;

    int last_decision_cycle;

    /* aliveness + timing */
    bool server_alive;
    int  interval_msec;
    int  waited_msec;

    /* version used for init */
    double version;
} InvictusTrainer;

/* ------------------------------------------------------------------ */
/* High-level trainer API                                             */
/* All return 0 on success, <0 on error.                              */
/* invictus_trainer_reset is the main entry point.                    */
/* ------------------------------------------------------------------ */

int invictus_trainer_reset(InvictusTrainer* t, const char* host, int port);

int invictus_trainer_wait_for_decision(InvictusTrainer* t);

/* Serialize current effectors (via protocol) and send over transport.
   If server_params.synch_mode, (done) is appended.
   Clears the decision flags and the effectors (via serialize).
   Call this after one or more invictus_trainer_do_* calls.
   Each top-level command (incl. (done)) is transmitted as a separate
   datagram because the server Coach parser only processes the first
   command in a received string. */
int invictus_trainer_send_action(InvictusTrainer* t);

int invictus_trainer_close(InvictusTrainer* t);

/* ------------------------------------------------------------------ */
/* do_* effector population (trainer)                                 */
/* These only populate t->effectors. Call send_action to transmit.    */
/* ------------------------------------------------------------------ */

int invictus_trainer_do_ball_move(InvictusTrainer* t, double x, double y);
int invictus_trainer_do_ball_move_vel(InvictusTrainer* t, double x, double y, double vx, double vy);

int invictus_trainer_do_player_move(InvictusTrainer* t, const char* team, int unum, double x, double y);
int invictus_trainer_do_player_move_angle(InvictusTrainer* t, const char* team, int unum, double x, double y, double angle);
int invictus_trainer_do_player_move_vel(InvictusTrainer* t, const char* team, int unum, double x, double y, double angle, double vx, double vy);

int invictus_trainer_do_change_mode(InvictusTrainer* t, InvictusPlayMode mode);

int invictus_trainer_do_look(InvictusTrainer* t);
int invictus_trainer_do_recover(InvictusTrainer* t);
int invictus_trainer_do_check_ball(InvictusTrainer* t);
int invictus_trainer_do_start(InvictusTrainer* t);   /* (start) / kickoff */

int invictus_trainer_do_eye(InvictusTrainer* t, bool on);
int invictus_trainer_do_ear(InvictusTrainer* t, bool on);

int invictus_trainer_do_say(InvictusTrainer* t, const char* msg);

int invictus_trainer_do_change_player_type(InvictusTrainer* t, const char* team, int unum, int ptype);

/* ------------------------------------------------------------------ */
/* Implementation                                                     */
/* ------------------------------------------------------------------ */

/* Internal state setup (not public; use reset) */
static int invictus_trainer_init(InvictusTrainer* t)
{
    if (!t) return -1;
    memset(t, 0, sizeof(*t));
    if (invictus_transport_init(&t->comm) != 0) {
        invictus_log("invictus: trainer_init: transport init failed");
        return -1;
    }

    t->version = INVICTUS_DEFAULT_VERSION;
    invictus_clear_trainer_visual(&t->visual);
    memset(&t->effectors, 0, sizeof(t->effectors));
    t->playmode = INVICTUS_PM_BeforeKickOff;
    t->current_cycle = 0;
    t->action_required = false;
    invictus_common_reset(&t->server_params, &t->common_control);
    t->last_decision_cycle = -1;
    t->server_alive = false;
    t->interval_msec = 10;
    t->waited_msec = 0;
    return 0;
}

static int invictus_trainer_send_init(InvictusTrainer* t)
{
    if (!t) return -1;
    char buf[256];
    snprintf(buf, sizeof(buf), "(init (version %.1f))", t->version);
    for (int k = 0; k < 6; k++) {
        int n = invictus_transport_send(&t->comm, buf);
        if (n > 0) return 0;
        struct timespec ts = {0, (30000 + k * 20000) * 1000L};
        nanosleep(&ts, NULL);
    }
    invictus_log("invictus: trainer send_init: failed to send init message");
    return -1;
}

static int invictus_trainer_receive_and_parse(InvictusTrainer* t)
{
    int processed = 0;
    while (invictus_transport_recv(&t->comm) > 0) {
        const char *msg = t->comm.last_msg_received;
        invictus_parse_common(&t->server_params, &t->common_control, msg);
        invictus_parse_trainer_message(&t->visual, &t->playmode, &t->current_cycle, msg);
        if (!t->server_params.synch_mode && t->server_params.slow_down_factor > 1) {
            t->interval_msec = 10 * t->server_params.slow_down_factor;
        }
        processed = 1;
    }
    return processed;
}

/* Wait for decision point (think in synch, or visual timing in non-synch).
   Does not send. Returns 1 when it is time to populate effectors and call send_action. */
int invictus_trainer_wait_for_decision(InvictusTrainer* t)
{
    if (!t || t->comm.sock < 0) {
        invictus_log("invictus: trainer_wait_for_decision: invalid state (null or no socket)");
        return -1;
    }

    /* If a decision signal is already present (from prior receive or previous wait),
       do not re-block. Caller is expected to act (or call send_action). */
    if (t->server_params.synch_mode ? t->common_control.think_received : t->action_required) {
        return 1;
    }

    for (;;) {
        int ret = invictus_transport_has_incoming_data(&t->comm, t->interval_msec);
        if (ret < 0) {
            if (errno == EINTR) continue;
            invictus_log("invictus: trainer_wait_for_decision: select error: %s", strerror(errno));
            return -1;
        }

        if (ret > 0) {
            invictus_trainer_receive_and_parse(t);
            t->waited_msec = 0;
        } else if (ret == 0) {
            t->waited_msec += t->interval_msec;
            if (t->waited_msec > 5000) {
                invictus_log("invictus: trainer_wait_for_decision: server timed out (no data >5s), marking dead");
                t->server_alive = false;
                return -1;
            }
            if (!t->server_params.synch_mode) {
                long last = t->visual.time;
                if (last > 0 && last != t->last_decision_cycle) {
                    if (t->waited_msec >= 75) {
                        t->action_required = true;
                    }
                }
            }
        }

        if (!t->server_alive) {
            invictus_log("invictus: trainer_wait_for_decision: server not alive");
            return -1;
        }

        bool time_to_act = t->server_params.synch_mode ? t->common_control.think_received : t->action_required;
        if (time_to_act) {
            if (!t->server_params.synch_mode) {
                t->last_decision_cycle = (int)t->visual.time;
            }
            return 1;
        }
    }
}

/* Internal: split a concatenated trainer command string into individual
   top-level (....) messages and send each via its own transport_send.
   Required because the server's Coach parser only processes the first
   top-level command per received buffer string. Quote-aware to handle
   (say "...") containing parentheses. */
static int invictus_send_trainer_commands_split(InvictusTransport *comm, const char *msg)
{
    if (!comm || !msg || !*msg) return 0;
    const char *p = msg;
    int count = 0;
    for (;;) {
        invictus_skip_ws(&p);
        if (*p != '(') break;
        const char *start = p;
        int depth = 0;
        bool inq = false;
        while (*p) {
            char c = *p;
            if (c == '"') { inq = !inq; ++p; continue; }
            if (!inq) {
                if (c == '(') { ++depth; }
                else if (c == ')') {
                    --depth;
                    ++p;
                    if (depth <= 0) break;
                    continue;
                }
            }
            ++p;
        }
        size_t len = (size_t)(p - start);
        if (len > 0) {
            char tmp[INVICTUS_MAX_MESG];
            if (len >= sizeof(tmp)) len = sizeof(tmp)-1;
            memcpy(tmp, start, len);
            tmp[len] = '\0';
            if (invictus_transport_send(comm, tmp) > 0) ++count;
        }
        if (!*p) break;
    }
    return count;
}

/* Commit and transmit whatever is currently in effectors.
   Honors synch_mode for (done) appending (via serialize).
   Clears decision flags and the effectors struct.
   Each top-level command is sent in its own datagram so the server
   Coach parser (which only looks at the first command per string) sees
   every command including a trailing (done). */
int invictus_trainer_send_action(InvictusTrainer* t)
{
    if (!t) {
        invictus_log("invictus: trainer_send_action: null trainer");
        return -1;
    }

    char buf[INVICTUS_MAX_MESG];
    buf[0] = '\0';
    int rc = invictus_serialize_trainer_commands(&t->effectors, t->server_params.synch_mode, buf, sizeof(buf));
    if (rc == 0 && buf[0]) {
        /* Split rather than one send(buf) to be compatible with server
           trainer command parsing. */
        (void)invictus_send_trainer_commands_split(&t->comm, buf);
    }

    t->action_required = false;
    t->common_control.think_received = false;
    if (!t->server_params.synch_mode) {
        t->last_decision_cycle = (int)t->current_cycle;
    }

    return 0;
}

int invictus_trainer_reset(InvictusTrainer* t, const char* host, int port)
{
    if (!t) return -1;

    if (invictus_trainer_close(t) != 0) {
        invictus_log("invictus: trainer_reset: close of prior session failed");
        return -1;
    }

    if (invictus_trainer_init(t) != 0) {
        invictus_log("invictus: trainer_reset: trainer init failed");
        return -1;
    }

    if (!host || invictus_transport_connect(&t->comm, host, port) != 0) {
        invictus_log("invictus: trainer_reset: connect to %s:%d failed", host ? host : "(null)", port);
        return -1;
    }

    t->server_alive = true;
    t->interval_msec = 10;
    t->waited_msec = 0;

    if (invictus_trainer_send_init(t) != 0) {
        invictus_log("invictus: trainer_reset: send_init failed");
        return -1;
    }
    return 0;
}

int invictus_trainer_close(InvictusTrainer* t)
{
    if (!t) return 0;

    int s = t->comm.sock;
    bool looks_live = t->comm.connected || t->server_alive || (s >= 3 && s < 4096);
    if (looks_live) {
        if (t->server_alive) {
            int n = invictus_transport_send(&t->comm, "(bye)");
            if (n <= 0) {
                invictus_log("invictus: trainer_close: failed to send bye (best effort)");
            }
        }
        invictus_transport_disconnect(&t->comm);
    }
    t->server_alive = false;

    return 0;
}

/* === do_* implementations (population only) === */

int invictus_trainer_do_ball_move(InvictusTrainer* t, double x, double y)
{
    if (!t) return -1;
    t->effectors.ball_move.active = true;
    t->effectors.ball_move.x = x;
    t->effectors.ball_move.y = y;
    t->effectors.ball_move.has_vel = false;
    return 0;
}

int invictus_trainer_do_ball_move_vel(InvictusTrainer* t, double x, double y, double vx, double vy)
{
    if (!t) return -1;
    t->effectors.ball_move.active = true;
    t->effectors.ball_move.x = x;
    t->effectors.ball_move.y = y;
    t->effectors.ball_move.vx = vx;
    t->effectors.ball_move.vy = vy;
    t->effectors.ball_move.has_vel = true;
    return 0;
}

int invictus_trainer_do_player_move(InvictusTrainer* t, const char* team, int unum, double x, double y)
{
    if (!t) return -1;
    t->effectors.player_move.active = true;
    if (team) {
        strncpy(t->effectors.player_move.team, team, sizeof(t->effectors.player_move.team)-1);
        t->effectors.player_move.team[sizeof(t->effectors.player_move.team)-1] = '\0';
    } else {
        t->effectors.player_move.team[0] = '\0';
    }
    t->effectors.player_move.unum = unum;
    t->effectors.player_move.x = x;
    t->effectors.player_move.y = y;
    t->effectors.player_move.has_angle = false;
    t->effectors.player_move.has_vel = false;
    return 0;
}

int invictus_trainer_do_player_move_angle(InvictusTrainer* t, const char* team, int unum, double x, double y, double angle)
{
    if (!t) return -1;
    t->effectors.player_move.active = true;
    if (team) {
        strncpy(t->effectors.player_move.team, team, sizeof(t->effectors.player_move.team)-1);
        t->effectors.player_move.team[sizeof(t->effectors.player_move.team)-1] = '\0';
    } else {
        t->effectors.player_move.team[0] = '\0';
    }
    t->effectors.player_move.unum = unum;
    t->effectors.player_move.x = x;
    t->effectors.player_move.y = y;
    t->effectors.player_move.angle = angle;
    t->effectors.player_move.has_angle = true;
    t->effectors.player_move.has_vel = false;
    return 0;
}

int invictus_trainer_do_player_move_vel(InvictusTrainer* t, const char* team, int unum, double x, double y, double angle, double vx, double vy)
{
    if (!t) return -1;
    t->effectors.player_move.active = true;
    if (team) {
        strncpy(t->effectors.player_move.team, team, sizeof(t->effectors.player_move.team)-1);
        t->effectors.player_move.team[sizeof(t->effectors.player_move.team)-1] = '\0';
    } else {
        t->effectors.player_move.team[0] = '\0';
    }
    t->effectors.player_move.unum = unum;
    t->effectors.player_move.x = x;
    t->effectors.player_move.y = y;
    t->effectors.player_move.angle = angle;
    t->effectors.player_move.vx = vx;
    t->effectors.player_move.vy = vy;
    t->effectors.player_move.has_angle = true;
    t->effectors.player_move.has_vel = true;
    return 0;
}

int invictus_trainer_do_change_mode(InvictusTrainer* t, InvictusPlayMode mode)
{
    if (!t) return -1;
    t->effectors.change_mode = true;
    t->effectors.new_playmode = mode;
    return 0;
}

int invictus_trainer_do_look(InvictusTrainer* t)
{
    if (!t) return -1;
    t->effectors.look = true;
    return 0;
}

int invictus_trainer_do_recover(InvictusTrainer* t)
{
    if (!t) return -1;
    t->effectors.recover = true;
    return 0;
}

int invictus_trainer_do_check_ball(InvictusTrainer* t)
{
    if (!t) return -1;
    t->effectors.check_ball = true;
    return 0;
}

int invictus_trainer_do_start(InvictusTrainer* t)
{
    if (!t) return -1;
    t->effectors.kickoff_start = true;
    return 0;
}

int invictus_trainer_do_eye(InvictusTrainer* t, bool on)
{
    if (!t) return -1;
    t->effectors.eye = true;
    t->effectors.eye_on = on;
    return 0;
}

int invictus_trainer_do_ear(InvictusTrainer* t, bool on)
{
    if (!t) return -1;
    t->effectors.ear = true;
    t->effectors.ear_on = on;
    return 0;
}

int invictus_trainer_do_say(InvictusTrainer* t, const char* msg)
{
    if (!t || !msg) return -1;
    t->effectors.say = true;
    strncpy(t->effectors.say_msg, msg, sizeof(t->effectors.say_msg) - 1);
    t->effectors.say_msg[sizeof(t->effectors.say_msg) - 1] = '\0';
    return 0;
}

int invictus_trainer_do_change_player_type(InvictusTrainer* t, const char* team, int unum, int ptype)
{
    if (!t || !team) return -1;
    t->effectors.change_player_type = true;
    strncpy(t->effectors.cpt_team, team, sizeof(t->effectors.cpt_team)-1);
    t->effectors.cpt_team[sizeof(t->effectors.cpt_team)-1] = '\0';
    t->effectors.cpt_unum = unum;
    t->effectors.cpt_ptype = ptype;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* INVICTUS_TRAINER_H */
