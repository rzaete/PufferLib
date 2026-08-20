/* rcss2d: RoboCup 2D Soccer Simulation environment for PufferLib.
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "helpers.h"
#include "invictus_player.h"
#include "invictus_trainer.h"
#include "invictus_trainer_protocol.h"

// NUM_ATNS and OBS_SIZE are provided by binding.c when building the extension
// (defines appear before #include "rcss2d.h" in binding.c). Standalone builds
// (rcss2d.c) get the fallbacks below. Must match binding.c .
#ifndef OBS_SIZE
#define OBS_SIZE 2
#endif
#ifndef NUM_ATNS
#define NUM_ATNS 2
#endif

const int PITCH_LENGTH = 105;
const int PITCH_WIDTH = 68;
const int PITCH_MARGIN = 5;
const int PITCH_LENGTH_TOTAL = PITCH_LENGTH + (2 * PITCH_MARGIN);
const int PITCH_WIDTH_TOTAL = PITCH_WIDTH + (2 * PITCH_MARGIN);

// Required struct. Only use floats!
typedef struct {
    float perf; // Recommended 0-1 normalized single real number perf metric
    float score; // Recommended unnormalized single real number perf metric
    float episode_return; // Recommended metric: sum of agent rewards over episode
    float episode_length; // Recommended metric: number of steps of agent episode
    // Any extra fields you add here may be exported to Python in binding.c
    float n; // Required as the last field
} Log;

typedef struct {
    Log log; // Required field. Env binding code uses this to aggregate logs

    // Per-env (match) state
    float* observations; // Required
    float* actions;      // Required
    float* rewards;      // Required
    float* terminals;    // Required
    int num_agents;      // the RL players on the RL team

    unsigned int rng;

    // RCSS infrastructure (launched fresh on each c_reset)
    pid_t server_pid;
    pid_t monitor_pid;
    pid_t scripted_pids[32];   // enough for goalies + field players + coaches
    int num_scripted_pids;

    InvictusPlayer* rl_players;
    InvictusTrainer* rl_trainer;
    int *ticks_since_reward;

    // Per-match config (ports chosen to avoid collision across vectorized envs)
    unsigned int player_port;
    unsigned int coach_port;
    unsigned int offline_coach_port;
    const char* host;
    const char* scripted_team_name; // e.g. "t1" — full 11 scripted
    const char* rl_team_name;       // e.g. "t2" — scripted + RL
    int use_monitor;           // 0/1, controlled from kwargs (default off)

    // Previous state used by compute_rewards_and_terminals for:
    // - transition detection on goal playmodes (prev_playmode)
    // - potential future ball progress shaping (prev_ball_* are now populated each step)
    float prev_ball_x;
    float prev_ball_y;
    int prev_score_rl_team;
    int prev_score_scripted;
    int episode_steps;
    int prev_playmode;
    bool should_reset;
} Rcss2d;

static void add_log(Rcss2d* env, bool rl_goal) {
    env->log.n += 1.0f;
    if (rl_goal) {
        env->log.score += 1.0f;
        env->log.perf += 1.0f;
    }
    // perf averaged becomes win-rate-like over reported episodes
}

// Stop and reap any children from a previous episode
static void stop_children(Rcss2d* env) {
    if (env->server_pid > 0) {
        process_send_sigint(env->server_pid);
    }

    msleep(500); // give children a moment to handle SIGINT

    // Best-effort reaping: first try non-blocking
    for (int i = 0; i < env->num_scripted_pids; ++i) {
        if (env->scripted_pids[i] > 0) {
            int status;
            if (waitpid(env->scripted_pids[i], &status, WNOHANG) <= 0) {
                // still alive, do blocking
                waitpid(env->scripted_pids[i], &status, 0);
            }
        }
    }
    if (env->monitor_pid > 0) { int s; waitpid(env->monitor_pid, &s, WNOHANG) || waitpid(env->monitor_pid, &s, 0); }
    if (env->server_pid > 0)  { int s; waitpid(env->server_pid,  &s, WNOHANG) || waitpid(env->server_pid,  &s, 0); }

    env->num_scripted_pids = 0;
    env->server_pid = env->monitor_pid = -1;
}

void init(Rcss2d* env) {
    // The vectorizer (vecenv.h) assigns the four float* buffers and sets num_agents.
    // We just zero local accumulators here.
    env->server_pid = -1;
    env->monitor_pid = -1;
    env->num_scripted_pids = 0;
    env->rl_players = (InvictusPlayer*)calloc(env->num_agents, sizeof(InvictusPlayer));
    env->ticks_since_reward = (int *)calloc(env->num_agents, sizeof(int));
    env->rl_trainer = (InvictusTrainer*)malloc(sizeof(InvictusTrainer));
    env->prev_ball_x = env->prev_ball_y = 0;
    env->prev_score_rl_team = env->prev_score_scripted = 0;
    env->episode_steps = 0;
    env->prev_playmode = 0;  // INVICTUS_PM_NULL
    env->should_reset = false;

    int err = 0;
    err = get_ports_for_rcss(&env->player_port, &env->coach_port, &env->offline_coach_port, &env->rng);
    if (err < 0) {
        printf("rcss2d: failed to get ports for rcss\n");
        exit(err);
    }

    env->host = "127.0.0.1";
    env->scripted_team_name = "t1";
    env->rl_team_name       = "t2";
    if (env->use_monitor < 0) env->use_monitor = 0;

    char start_timestamp[20];
    get_current_timestamp_str(start_timestamp, sizeof(start_timestamp));
    char server_log_file_name[256];
    sprintf(server_log_file_name, "%s-%u.log", start_timestamp, env->rng);

    unsigned int seed = env->rng % 1000;
    
    env->server_pid = launch_server(env->player_port, env->coach_port, env->offline_coach_port, seed, server_log_file_name);
    msleep(500);

    if (env->use_monitor) {
        env->monitor_pid = launch_monitor(env->player_port, "/dev/null");
        msleep(100);
    }

    pid_t* pids = env->scripted_pids;
    int idx = 0;

    // launch_team(env->host, env->player_port, env->coach_port, env->scripted_team_name, pids, "/dev/null");
    // idx = 12;

    // pids[idx++] = launch_player(env->host, env->player_port, env->rl_team_name, true, "/dev/null");
    // msleep(100);

    for (int i = 0; i < env->num_agents; i++) {
        invictus_player_reset(&env->rl_players[i], env->host, env->player_port, env->rl_team_name, 0);
        msleep(100);
    }

    invictus_trainer_reset(env->rl_trainer, env->host, env->offline_coach_port);

    // for (int i = 0 ; i < 11 - (env->num_agents + 1) ; i++) {
    //     pids[idx++] = launch_player(env->host, env->player_port, env->rl_team_name, false, "/dev/null");
    //     msleep(100);
    // }

    // pids[idx++] = launch_coach(env->host, env->coach_port, env->rl_team_name, "/dev/null");
    // msleep(100);

    env->num_scripted_pids = idx;
}

void update_goals(Rcss2d* env) {
    for (int a = 0; a < env->num_agents; a++) {
        InvictusTrainerBall* ball = &env->rl_trainer->visual.ball;
        InvictusTrainerPlayer* player = &env->rl_trainer->visual.players[a];
        float dx = ball->x - player->x;
        float dy = ball->y - player->y;
        float dist = sqrtf(dx*dx + dy*dy);
        env->rewards[a] = (-(fabsf(dx) / PITCH_LENGTH_TOTAL)) + (-(fabsf(dy) / PITCH_WIDTH_TOTAL));
        env->log.episode_return += env->rewards[a];
        // printf("r  %.1f\n", env->rewards[0]);
        // printf("ball_x %.1f  ball_y %.1f  player_x %.1f  player_y %.1f  dx %.1f  dy %.1f  dist %.1f\n",
                // ball->x, ball->y, player->x, player->y, dx, dy, dist);
        // fflush(stdout);
        if (dist > 25) {
            continue;
        }
        // player reached the ball. move the ball to new position
        float new_x = (rand_r(&env->rng) % PITCH_LENGTH) - (PITCH_LENGTH / 2.0);
        float new_y = (rand_r(&env->rng) % PITCH_WIDTH) - (PITCH_WIDTH / 2.0);

        invictus_trainer_do_ball_move(env->rl_trainer, new_x, new_y);

        // printf("[move] ball_x %.1f  ball_y %.1f\n",
        //         new_x, new_y);
        // fflush(stdout);

        
        env->log.score += 1.0f;
        env->log.episode_length += env->ticks_since_reward[a];
        env->log.perf += fmaxf(0.0f, 1.0f - 0.01f * env->ticks_since_reward[a]);
        env->ticks_since_reward[a] = 0;
        env->log.n++;
    }
}

// Compute per-RL-agent observations (delegates to the C wrapper which pulls from librcsc state).
// Must write exactly num_agents * OBS_SIZE floats.
void compute_observations(Rcss2d* env) {
    const int OBS_PER_AGENT = OBS_SIZE;
    for (int i = 0 ; i < env->num_agents ; i++) {
        invictus_wait_for_decision(&env->rl_players[i]);
        // if (invictus_player_compute_observation(&env->rl_players[i], env->observations + i * OBS_PER_AGENT, OBS_PER_AGENT) < 0) {
        //     // Zero the slot if client not ready
        //     for (int k = 0; k < OBS_PER_AGENT; ++k) {
        //         env->observations[i * OBS_PER_AGENT + k] = 0.0f;
        //     }
        // }
    }

    invictus_trainer_wait_for_decision(env->rl_trainer);

    int obs_idx = 0;
    InvictusTrainerBall* ball = &env->rl_trainer->visual.ball;
    InvictusTrainerPlayer* player = &env->rl_trainer->visual.players[0];
    env->observations[obs_idx++] = (ball->x - player->x) / (float)PITCH_LENGTH_TOTAL;
    env->observations[obs_idx++] = (ball->y - player->y) / (float)PITCH_WIDTH_TOTAL;
    // env->observations[obs_idx++] = player->vx;
    // env->observations[obs_idx++] = player->vy;
    // env->observations[obs_idx++] = env->rewards[0];
    // env->observations[obs_idx++] = player->x / (PITCH_LENGTH_TOTAL / 2.0);
    // env->observations[obs_idx++] = player->y / (PITCH_WIDTH_TOTAL / 2.0);

    // printf("obs  ");
    // for (int i = 0 ; i < OBS_SIZE ; i++) {
    //     printf("%.1f  ", env->observations[i]);
    // }
    // printf("\n");
    // fflush(stdout);
}

// Required function
void c_reset(Rcss2d* env) {
    env->episode_steps = 0;
    env->prev_playmode = 0;
    env->should_reset = false;

    // Initial observations for the two RL agents
    compute_observations(env);

    invictus_trainer_do_eye(env->rl_trainer, true);
    if (invictus_trainer_send_action(env->rl_trainer) != 0) {
        printf("send_action eye on failed\n");
    }
}

// Helpers for goal detection using the real playmode values from the parser
// (see invictus_client.h invictus_playmode_table: "goal_l" / "goal_r").
static bool rcss2d_is_rl_goal(InvictusPlayMode pm, InvictusSide side) {
    if (side == INVICTUS_SIDE_LEFT  && pm == INVICTUS_PM_AfterGoal_Left)  return true;
    if (side == INVICTUS_SIDE_RIGHT && pm == INVICTUS_PM_AfterGoal_Right) return true;
    return false;
}

// Simple reward shaping + early goal stop (per user request).
// Called after compute_observations (which ensures fresh parse of playmode/cycle/sensors).
static void compute_rewards_and_terminals(Rcss2d* env) {
    for (int a = 0; a < env->num_agents; ++a) {
        env->rewards[a] = 0.0f;
        env->terminals[a] = 0.0f;
    }

    // Use the first RL client's state as a proxy for the match (both clients see the same world).
    // State is fresh because compute_observations just ran invictus_player_compute_observation.
    InvictusPlayer* c0 = &env->rl_players[0];
    if (!c0 || !c0->comm.connected) {
        // Still apply penalty so the agent sees consistent (negative) signal.
        const float step_penalty = -0.001f;
        for (int a = 0; a < env->num_agents; ++a) {
            env->rewards[a] += step_penalty;
        }
        env->prev_playmode = 0;
        return;
    }

    // Reward shaping is intentionally minimal (living penalty + sparse goal events).
    // Real progress/possession shaping can be added later when more absolute state is exposed.
    const float STEP_PENALTY = -0.001f;
    for (int a = 0; a < env->num_agents; ++a) {
        env->rewards[a] += STEP_PENALTY;
    }

    int pm = c0->playmode;
    int t = c0->current_cycle;

    // Snapshot ball state (polar relative to the viewing player) so prev_* fields are live
    // for future reward shaping or debugging. Absolute field x/y would require additional
    // marker reconstruction or fullstate parsing.
    if (c0->sensors.visual.num_balls > 0) {
        env->prev_ball_x = (float)c0->sensors.visual.balls[0].dist;
        env->prev_ball_y = (float)c0->sensors.visual.balls[0].dir;
    } else {
        env->prev_ball_x = 1e6f;
        env->prev_ball_y = -360.0f;
    }

    // Early stop + team reward on goal (detect transition so we don't re-apply while
    // playmode may briefly remain in AfterGoal_* before the server moves to kick-off).
    if ((pm == INVICTUS_PM_AfterGoal_Left || pm == INVICTUS_PM_AfterGoal_Right) &&
        pm != env->prev_playmode) {
        float r = rcss2d_is_rl_goal((InvictusPlayMode)pm, c0->side) ? +1.0f : -1.0f;
        for (int a = 0; a < env->num_agents; ++a) {
            env->rewards[a] += r;
            env->terminals[a] = 1.0f;
        }
        // score (rl goals) and episode_return / n are handled by caller (c_step)
        // so we do not mutate log here.
    }

    // // Safety time-based terminal (real halves ~3000 cycles). Using raw server cycle is sufficient
    // // for a hard stop that keeps vectorized rollouts from running forever.
    // const int MAX_CYCLES = 6000;
    // if (t > 0 && t >= MAX_CYCLES) {
    //     for (int a = 0; a < env->num_agents; ++a) {
    //         env->terminals[a] = 1.0f;
    //     }
    //     env->should_reset = true;
    // }

    env->prev_playmode = pm;
    env->episode_steps = t;
}

// Required function
void c_step(Rcss2d* env) {
    // Send the actions chosen by Puffer for the two RL players.
    for (int a = 0; a < env->num_agents; ++a) {
        env->rewards[a] = 0.0f;   // will be filled after receive
        env->terminals[a] = 0.0f;
        env->ticks_since_reward[a] += 1;
        InvictusPlayer *agent = &env->rl_players[a];
        float *requested_actions = &env->actions[a*NUM_ATNS];

        float new_actions[1 + NUM_ATNS];
        new_actions[0] = 1;

        // Second action → [0, 100]
        int bins1 = 21;                     // must match ACT_SIZES[1]
        int b1 = (int)requested_actions[0];
        if (b1 < 0) b1 = 0;
        if (b1 >= bins1) b1 = bins1 - 1;
        new_actions[1] = (b1 / (float)(bins1 - 1)) * 100.0f;

        // Third action → [-180, 180]
        int bins2 = 37;                     // must match ACT_SIZES[2]
        int b2 = (int)requested_actions[1];
        if (b2 < 0) b2 = 0;
        if (b2 >= bins2) b2 = bins2 - 1;
        new_actions[2] = -180.0f + (b2 / (float)(bins2 - 1)) * 360.0f;
        
        if (invictus_player_act(agent, new_actions, 1 + NUM_ATNS) != 0) {
            printf("act failed\n");
        }

        if (env->ticks_since_reward[a] % 2000 == 0) {
            float new_x = (rand_r(&env->rng) % PITCH_LENGTH) - (PITCH_LENGTH / 2.0);
            float new_y = (rand_r(&env->rng) % PITCH_WIDTH) - (PITCH_WIDTH / 2.0);
            invictus_trainer_do_player_move(env->rl_trainer, agent->team, agent->unum, new_x, new_y);
        }
    }

    long cycle = env->rl_trainer->current_cycle;
    if (cycle > 0 && (cycle % 200 == 0)) {
        // printf("cycle %ld: sending recover\n", cycle);
        // fflush(stdout);
        invictus_trainer_do_recover(env->rl_trainer);
    }

    update_goals(env);

    if (invictus_trainer_send_action(env->rl_trainer) != 0) {
        printf("send_action failed\n");
    }
    
    // Observations from the latest world state in the librcsc-backed clients
    compute_observations(env);

    // printf("p ball_dist %.1f  ball_dir %.1f\n",
    //         env->rl_players[0].sensors.visual.balls[0].dist, env->rl_players[0].sensors.visual.balls[0].dir);
    // fflush(stdout);
    
    // // Compute rewards (step penalty + goal events) + terminals (goal or safety horizon)
    // compute_rewards_and_terminals(env);

    // // Update log accumulators.
    // // episode_return accumulates *sum of agent rewards* (per-agent identical team reward).
    // // episode_length here follows the previous per-agent +=1 convention (== num_agents * world steps).
    // // n is incremented *once* per shared match termination (goal or safety time), not per agent.
    // InvictusPlayer* c0 = &env->rl_players[0];
    // int pm   = (c0 && c0->comm.connected) ? c0->playmode : 0;
    // InvictusSide side = (c0 && c0->comm.connected) ? c0->side : INVICTUS_SIDE_UNKNOWN;

    // bool any_terminal = false;
    // for (int a = 0; a < env->num_agents; ++a) {
    //     env->log.episode_return += env->rewards[a];
    //     env->log.episode_length += 1.0f;
    //     if (env->terminals[a] > 0.0f) {
    //         any_terminal = true;
    //     }
    // }
    // if (any_terminal) {
    //     bool rl_goal = rcss2d_is_rl_goal((InvictusPlayMode)pm, side);
    //     add_log(env, rl_goal);
    //     if (env->should_reset)
    //         c_reset(env);
    // }
}

// Optional render (mostly useful for --local standalone). The real viz is usually the rcssmonitor child.
// We keep a trivial window so the standard ESC-exit behavior of Puffer ocean envs works.
void c_render(Rcss2d* env) {
    return;
}

// Required function. Should clean up anything you allocated.
// Do not free the four float* buffers (observations/actions/rewards/terminals) — vecenv owns them.
void c_close(Rcss2d* env) {
    for (int i = 0; i < env->num_agents; i++) {
        invictus_player_close(&env->rl_players[i]);
    }

    invictus_trainer_close(env->rl_trainer);

    stop_children(env);

    free(env->rl_players);
    free(env->ticks_since_reward);
}
