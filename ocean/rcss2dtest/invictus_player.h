/* invictus_player.h
 *
 * Soccer player + RL interface.
 *
 * This header pulls in invictus_player_protocol.h (protocol + types), which in turn
 * pulls in invictus_transport.h (raw UDP comms), and layers on top:
 *   - Protocol parsing (see, sense_body, hear, init, ...)
 *   - Effectors population (do_* set intent; transport serializes to commands)
 *   - High-level RL-friendly API
 *
 * === Recommended API (for RL / external callers) ===
 *   int invictus_player_reset(...)           // full setup + connect + first obs
 *   int invictus_player_compute_observation(...)
 *   int invictus_player_act(...)
 *
 *   int invictus_player_init(...)            // low-level setup (no network)
 *   int invictus_player_close(...)           // safe cleanup (sends bye if needed)
 *
 * All return int error code: 0 = success, <0 = error.
 * Caller owns the InvictusPlayer storage (no malloc/free inside).
 *
 * === Lower-level / compatibility API ===
 *   invictus_connect / disconnect / step
 *   invictus_do_* (populate effectors)
 *   invictus_flush_commands / send_init / send_bye
 *   invictus_send / recv via &p->comm (use parse_* taking &p->sensors.visual / &p->sensors.body etc.)
 *
 * These are provided for finer control or compatibility. Many are thin
 * shims over the embedded InvictusTransport (via transport) + soccer guards.
 * For direct comms access you can also use:  &player->comm
 *
 * Usage: just #include "invictus_player.h"
 * (complete single-header implementation; include once in your program).
 */

#ifndef INVICTUS_PLAYER_H
#define INVICTUS_PLAYER_H

#include "invictus_player_protocol.h"

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
/* Protocol data types (brought in via client for parsers) + player state */
/* The full InvictusPlayer lives here. Client parsers populate it.    */
/* ------------------------------------------------------------------ */

typedef struct {
    /* communication layer */
    InvictusTransport comm;

    /* identity (from init response) */
    char         team[INVICTUS_TEAMNAME_MAX];
    double       version;
    bool         goalie;
    int          unum;
    InvictusSide side;

    /* raw last-parsed sensors */
    InvictusSensors sensors;

    /* effectors: populated by invictus_do_*; serialized on flush */
    InvictusEffectors effectors;

    /* minimal game context */
    long              current_cycle;
    InvictusPlayMode  playmode;

    /* command / action state */
    bool   action_required;

    /* common protocol state received from server / common messages
       (populated via invictus_parse_common) */
    InvictusServerParams  server_params;
    InvictusCommonControl common_control;

    int    last_decision_cycle;

    /* aliveness + timing (moved from transport; used by wait logic) */
    bool server_alive;
    int  interval_msec;
    int  waited_msec;

    /* counts for body cmd guard */
    int    last_body_cycle;
    int    last_kick_count;
    int    last_dash_count;
    int    last_turn_count;
} InvictusPlayer;

#define INVICTUS_OBSERVATION_DIM 148

/* ------------------------------------------------------------------ */
/* High-level RL API                                                  */
/* All return 0 on success, <0 on error.                              */
/* invictus_player_reset is the main entry point.                     */
/* ------------------------------------------------------------------ */

int invictus_player_reset(InvictusPlayer* p,
                          const char* host, int port,
                          const char* team, bool goalie);

int invictus_player_compute_observation(InvictusPlayer* p, float* obs, int max_obs);

int invictus_player_act(InvictusPlayer* p, const float* action, int dim);

/* ------------------------------------------------------------------ */
/* Lower-level API (commands, I/O shims, step, etc.)                  */
/*                                                                    */
/* These provide direct access to commands and the comms layer.       */
/* Most operation functions return int (0 success, <0 error).         */
/* Data I/O and step keep their historical return-value conventions.  */
/*                                                                    */
/* High-level functions above are implemented in terms of these.      */
/* You can also reach the raw client directly via:  &p->comm          */
/* (Direct field access on the struct is the preferred style.)        */
/* ------------------------------------------------------------------ */

/* ---------------- Low-level player setup / cleanup ---------------- */
int invictus_player_init(InvictusPlayer* p,
                         const char* team,
                         bool goalie);

int invictus_player_close(InvictusPlayer* p);

/* ---------------- Message parsing & observation --------------------- */
int  invictus_get_raw_observation(InvictusPlayer* p, float* out, int max_floats);

/* Note: For other state, access fields directly on the struct (e.g. p->action_required,
   p->common_control.think_received, p->server_alive, p->server_params.synch_mode,
   p->comm.connected, etc.). This library prefers direct access over accessor functions
   for public struct members. */

/* ------------------------------------------------------------------ */
/* Implementation (player-specific)                                   */
/* ------------------------------------------------------------------ */

/* === Body command guard + do_* effectors population (player layer) == */

/* Body command guard (uses sense_body counts) */
static bool invictus_can_issue_body_cmd(InvictusPlayer* p)
{
    if (!p) return false;
    int cur = p->sensors.body.kick_count + p->sensors.body.dash_count + p->sensors.body.turn_count +
              p->sensors.body.catch_count + p->sensors.body.tackle_count;
    if (p->last_body_cycle != (int)p->sensors.body.time) {
        p->last_body_cycle = (int)p->sensors.body.time;
        return true;
    }
    if (cur > (p->last_kick_count + p->last_dash_count + p->last_turn_count)) {
        return true;
    }
    return false;
}

/* Receive pending datagrams and feed them to the client parse methods.
 * Calls common protocol parser first (for server_params + common_control),
 * then player-specific parser.
 * Returns non-zero if at least one was processed.
 */
static int invictus_receive_and_parse(InvictusPlayer* p)
{
    int processed = 0;
    while (invictus_transport_recv(&p->comm) > 0) {
        const char *msg = p->comm.last_msg_received;
        invictus_parse_common(&p->server_params, &p->common_control, msg);
        invictus_parse_message(&p->sensors.visual, &p->sensors.body, &p->playmode, &p->current_cycle,
                               &p->side, &p->unum, &p->version,
                               msg);
        if (!p->server_params.synch_mode && p->server_params.slow_down_factor > 1) {
            p->interval_msec = 10 * p->server_params.slow_down_factor;
        }
        processed = 1;
    }
    return processed;
}

/* Do_* command functions.
 * These now only populate the effectors struct. serialize_effectors
 * (in protocol layer) turns them into wire text for transport send.
 */
int invictus_do_dash(InvictusPlayer* p, double power, double rel_dir)
{
    if (!p || !invictus_can_issue_body_cmd(p)) return -1;
    p->effectors.body.type = INVICTUS_BODY_CMD_DASH;
    p->effectors.body.p1 = power;
    p->effectors.body.p2 = rel_dir;
    p->last_dash_count = p->sensors.body.dash_count;
    return 0;
}

int invictus_do_turn(InvictusPlayer* p, double moment)
{
    if (!p || !invictus_can_issue_body_cmd(p)) return -1;
    p->effectors.body.type = INVICTUS_BODY_CMD_TURN;
    p->effectors.body.p1 = moment;
    p->last_turn_count = p->sensors.body.turn_count;
    return 0;
}

int invictus_do_kick(InvictusPlayer* p, double power, double rel_dir)
{
    if (!p || !invictus_can_issue_body_cmd(p)) return -1;
    p->effectors.body.type = INVICTUS_BODY_CMD_KICK;
    p->effectors.body.p1 = power;
    p->effectors.body.p2 = rel_dir;
    p->last_kick_count = p->sensors.body.kick_count;
    return 0;
}

int invictus_do_tackle(InvictusPlayer* p, double power_or_dir, bool foul)
{
    if (!p || !invictus_can_issue_body_cmd(p)) return -1;
    p->effectors.body.type = INVICTUS_BODY_CMD_TACKLE;
    p->effectors.body.p1 = power_or_dir;
    p->effectors.body.p2 = foul ? 1.0 : 0.0;
    return 0;
}

int invictus_do_catch(InvictusPlayer* p)
{
    if (!p || !invictus_can_issue_body_cmd(p)) return -1;
    p->effectors.body.type = INVICTUS_BODY_CMD_CATCH;
    return 0;
}

int invictus_do_turn_neck(InvictusPlayer* p, double moment)
{
    if (!p) return -1;
    p->effectors.turn_neck.active = true;
    p->effectors.turn_neck.moment = moment;
    return 0;
}

int invictus_do_change_view(InvictusPlayer* p, InvictusViewWidth width)
{
    if (!p) return -1;
    p->effectors.change_view.active = true;
    p->effectors.change_view.width = width;
    return 0;
}

int invictus_do_move(InvictusPlayer* p, double x, double y)
{
    if (!p) return -1;
    p->effectors.body.type = INVICTUS_BODY_CMD_MOVE;
    p->effectors.body.p1 = x;
    p->effectors.body.p2 = y;
    return 0;
}

int invictus_do_say(InvictusPlayer* p, const char* msg)
{
    if (!p || !msg) return -1;
    p->effectors.say.active = true;
    strncpy(p->effectors.say.message, msg, sizeof(p->effectors.say.message) - 1);
    p->effectors.say.message[sizeof(p->effectors.say.message) - 1] = '\0';
    return 0;
}

int invictus_send_init(InvictusPlayer* p)
{
    if (!p) return -1;
    char buf[256];
    if (p->goalie)
        snprintf(buf, sizeof(buf), "(init %s (version %.1f) (goalie))", p->team, p->version);
    else
        snprintf(buf, sizeof(buf), "(init %s (version %.1f))", p->team, p->version);
    for (int k = 0; k < 6; k++) {
        int n = invictus_transport_send(&p->comm, buf, false);
        if (n > 0) return 0;
        // transient on nonblock UDP or server not quite ready; back off a bit
        struct timespec ts = {0, (30000 + k * 20000) * 1000L};
        nanosleep(&ts, NULL);
    }
    invictus_log("invictus: send_init: failed to send init message");
    return -1;
}

/* Low-level init: sets up player identity and internal state. */
/* Does not connect to server. For high-level use, call via reset. */
int invictus_player_init(InvictusPlayer* p,
                         const char* team,
                         bool goalie)
{
    if (!p) return -1;
    memset(p, 0, sizeof(*p));
    if (invictus_transport_init(&p->comm) != 0){
        invictus_log("invictus: player_init: client init failed");
        return -1;
    }

    p->version = INVICTUS_DEFAULT_VERSION;
    if (team && *team) {
        strncpy(p->team, team, sizeof(p->team)-1);
    } else {
        strncpy(p->team, "Invictus", sizeof(p->team)-1);
    }
    p->goalie = goalie;
    p->unum = -1;
    p->side = INVICTUS_SIDE_UNKNOWN;

    invictus_clear_visual(&p->sensors.visual);
    invictus_clear_body(&p->sensors.body);
    memset(&p->effectors, 0, sizeof(p->effectors));
    p->playmode = INVICTUS_PM_BeforeKickOff;

    p->current_cycle = 0;
    p->action_required = false;
    invictus_common_reset(&p->server_params, &p->common_control);
    p->last_decision_cycle = -1;
    p->server_alive = false;
    p->interval_msec = 10;
    p->waited_msec = 0;
    p->last_body_cycle = 0;
    p->last_kick_count = 0;
    p->last_dash_count = 0;
    p->last_turn_count = 0;
    return 0;
}

/* Raw observation packer */
int invictus_get_raw_observation(InvictusPlayer* p, float* out, int max_floats)
{
    if (!p || !out || max_floats < INVICTUS_OBSERVATION_DIM) return 0;
    int n = 0;

    out[n++] = (float)p->sensors.body.time;
    out[n++] = (float)p->sensors.body.stamina;
    out[n++] = (float)p->sensors.body.effort;
    out[n++] = (float)p->sensors.body.speed_mag;
    out[n++] = (float)p->sensors.body.speed_dir;
    out[n++] = (float)p->sensors.body.head_angle;
    out[n++] = (float)p->sensors.body.kick_count;
    out[n++] = (float)p->sensors.body.dash_count;
    out[n++] = (float)p->sensors.body.turn_count;
    out[n++] = (float)p->sensors.body.turn_neck_count;
    out[n++] = (float)p->sensors.body.tackle_count;
    out[n++] = (float)p->sensors.body.catch_count;
    out[n++] = (float)(p->sensors.body.view_width);
    out[n++] = (float)(p->sensors.body.view_quality);

    if (p->sensors.visual.num_balls > 0) {
        out[n++] = (float)p->sensors.visual.balls[0].dist;
        out[n++] = (float)p->sensors.visual.balls[0].dir;
        out[n++] = (float)p->sensors.visual.balls[0].dist_chng;
        out[n++] = (float)p->sensors.visual.balls[0].dir_chng;
    } else {
        out[n++] = 1e6f;
        out[n++] = -360.f;
        out[n++] = 0.f;
        out[n++] = 0.f;
    }

    int take = (p->sensors.visual.num_players < 21) ? p->sensors.visual.num_players : 21;
    for (int i = 0; i < take; ++i) {
        const InvictusSeenObj* pl = &p->sensors.visual.players[i];
        out[n++] = (float)pl->polar.dist;
        out[n++] = (float)pl->polar.dir;
        out[n++] = (float)pl->body;
        out[n++] = (float)pl->face;
        out[n++] = (float)(pl->unum >= 0 ? pl->unum : 0);
        out[n++] = (pl->team[0] == p->team[0] ? 1.f : -1.f);
    }
    for (int i = take; i < 21; ++i) {
        out[n++] = 1e6f;
        out[n++] = -360.f;
        out[n++] = 0.f;
        out[n++] = 0.f;
        out[n++] = 0.f;
        out[n++] = 0.f;
    }

    out[n++] = (float)p->current_cycle;
    out[n++] = (float)p->playmode;
    out[n++] = (float)p->unum;
    out[n++] = (float)p->side;

    return INVICTUS_OBSERVATION_DIM;
}

/* Wait loop used by high-level interface */
static int invictus_wait_for_decision(InvictusPlayer* p)
{
    if (!p || p->comm.sock < 0) {
        invictus_log("invictus: wait_for_decision: invalid state (null or no socket)");
        return -1;
    }

    for (;;) {
        int ret = invictus_transport_has_incoming_data(&p->comm, p->interval_msec);
        if (ret < 0) {
            if (errno == EINTR) continue;
            invictus_log("invictus: wait_for_decision: select error: %s", strerror(errno));
            return -1;
        }

        if (ret > 0) {
            invictus_receive_and_parse(p);
            p->waited_msec = 0;
        } else if (ret == 0) {
            /* timeout/backoff path (modeled on librcsc handleTimeout + isDecisionTiming) */
            p->waited_msec += p->interval_msec;
            if (p->waited_msec > 5000) { /* server_wait_seconds * 1000 (default 5s) */
                invictus_log("invictus: wait_for_decision: server timed out (no data >5s), marking dead");
                p->server_alive = false;
                return -1;
            }
            if (!p->server_params.synch_mode) {
                long last = p->sensors.body.time;
                if (last > 0 && last != p->last_decision_cycle) {
                    if (p->waited_msec >= 75) { /* conservative non-synch wait thr */
                        p->action_required = true;
                    }
                }
            }
        }

        if (!p->server_alive) {
            invictus_log("invictus: wait_for_decision: server not alive");
            return -1;
        }

        bool time_to_act = p->server_params.synch_mode ? p->common_control.think_received : p->action_required;
        if (time_to_act) {
            if (!p->server_params.synch_mode) {
                p->last_decision_cycle = (int)p->sensors.body.time;
            }
            return 1;
        }
    }
}

/* === High-level RL API implementations ============================ */

int invictus_player_reset(InvictusPlayer* p,
                          const char* host, int port,
                          const char* team, bool goalie)
{
    if (!p) return -1;

    /* Low-level close (it contains its own safety guards internally).
     * This cleanly shuts down any previous session (sends bye if needed)
     * before we re-init and connect.
     */
    if (invictus_player_close(p) != 0) {
        invictus_log("invictus: reset: close of prior session failed");
        return -1;
    }

    /* Low-level init: set up team/version/goalie and clear state */
    if (invictus_player_init(p, team, goalie) != 0) {
        invictus_log("invictus: reset: player init failed");
        return -1;
    }

    /* connect to the requested server (required) */
    if (!host || invictus_transport_connect(&p->comm, host, port) != 0) {
        invictus_log("invictus: reset: connect to %s:%d failed", host ? host : "(null)", port);
        return -1;
    }

    p->server_alive = true;
    p->interval_msec = 10;
    p->waited_msec = 0;

    if (invictus_send_init(p) != 0) {
        invictus_log("invictus: reset: send_init failed");
        return -1;
    }
    return 0;
}

int invictus_player_compute_observation(InvictusPlayer* p, float* obs, int max_obs)
{
    if (!p || !p->comm.connected) {
        invictus_log("invictus: compute_observation: not connected");
        return -1;
    }

    int r = invictus_wait_for_decision(p);
    if (r != 1) return r;

    int n = invictus_get_raw_observation(p, obs, max_obs);
    return n;
}

int invictus_player_act(InvictusPlayer* p, const float* action, int dim)
{
    if (!p || !action || dim <= 0) {
        invictus_log("invictus: player_act: invalid args");
        return -1;
    }

    int body_cmd = (dim > 0) ? (int)action[0] : 0;
    float p1     = (dim > 1) ? action[1] : 0.f;
    float p2     = (dim > 2) ? action[2] : 0.f;

    int neck_cmd = (dim > 3) ? (int)action[3] : 0;
    float neck_p = (dim > 4) ? action[4] : 0.f;

    int misc_cmd = (dim > 5) ? (int)action[5] : 0;
    float misc_p = (dim > 6) ? action[6] : 0.f;

    switch (body_cmd) {
    case 1: invictus_do_dash(p, p1, p2); break;
    case 2: invictus_do_turn(p, p2); break;
    case 3: invictus_do_kick(p, p1, p2); break;
    case 4: invictus_do_tackle(p, p1, (p2 > 0.5f)); break;
    case 5: invictus_do_catch(p); break;
    case 6: invictus_do_move(p, p1, p2); break;
    default: break;
    }

    if (neck_cmd == 1) {
        invictus_do_turn_neck(p, neck_p);
    }

    if (misc_cmd == 1) {
        int w = (int)(misc_p + 0.5f);
        InvictusViewWidth vw = INVICTUS_VIEW_WIDTH_NORMAL;
        if (w == 0) vw = INVICTUS_VIEW_WIDTH_NARROW;
        else if (w == 2) vw = INVICTUS_VIEW_WIDTH_WIDE;
        invictus_do_change_view(p, vw);
    }

    char cmdbuf[INVICTUS_MAX_MESG];
    cmdbuf[0] = '\0';
    int rc = invictus_serialize_effectors(&p->effectors, p->server_params.synch_mode, cmdbuf, sizeof(cmdbuf));
    if (rc == 0 && cmdbuf[0]) {
        // printf("[send] %s\n", cmdbuf);
        // fflush(stdout);
        int sent = invictus_transport_send(&p->comm, cmdbuf, false);
        if (sent <= 0 && (p->comm.connected || p->comm.sock >= 0)) {
            invictus_log("invictus: failed to send action");
        }
    }

    p->action_required = false;
    p->common_control.think_received = false;
    if (!p->server_params.synch_mode) {
        p->last_decision_cycle = (int)p->sensors.body.time;
    }

    return 0;
}

int invictus_player_close(InvictusPlayer* p)
{
    if (!p) return 0;

    /* Guard against uninitialized stack storage on first reset (or reuse).
     * Only consider live if sock is a plausible fd (>=3) or explicit connected flag.
     * Prevents spurious (bye) or close(garbage_fd) which would cause disconnects.
     */
    int s = p->comm.sock;
    bool looks_live = p->comm.connected || p->server_alive || (s >= 3 && s < 4096);
    if (looks_live) {
        if (p->server_alive) {
            int n = invictus_transport_send(&p->comm, "(bye)", false);
            if (n <= 0) {
                invictus_log("invictus: send_bye: failed to send bye (best effort)");
            }
        }
        invictus_transport_disconnect(&p->comm);
    }
    p->server_alive = false;

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* INVICTUS_PLAYER_H */
