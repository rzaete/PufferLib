/* rcss2d: RoboCup 2D Soccer Simulation environment for PufferLib.
 */
// #define _GNU_SOURCE
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
#include <pthread.h>

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
    float avg_duration_per_tick;

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

    int tick;
    int max_episode_tick;

    // RCSS infrastructure (launched fresh on each c_reset)
    pid_t server_pid;
    pid_t monitor_pid;
    // Per-match config (ports chosen to avoid collision across vectorized envs)
    unsigned int player_port;
    unsigned int coach_port;
    unsigned int offline_coach_port;
    const char* host;
    const char* rl_team_name;       // e.g. "t2" — scripted + RL
    int use_monitor;           // 0/1, controlled from kwargs (default off)

    float starting_distance;
    float target_distance;
    float previous_distance;
    float step_penalty;

    InvictusPlayer* rl_player;
    InvictusTrainer* rl_trainer;
    unsigned int reset_counter;
    unsigned int step_counter;
    char server_log_file_name[256];
    FILE* log_file;

    bool boostrap_done;

    double accumulated_step_duration;

    unsigned int rng;
} Rcss2dTest;

void c_close(Rcss2dTest* env);

void log_message(const FILE* log_file, const char *format, ...) {
    if (!log_file) return;

    // Print content
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
}

// Stop and reap any children from a previous episode
static void stop_children(Rcss2dTest* env) {
    if (env->server_pid > 0) {
        process_send_sigint(env->server_pid);
    }

    msleep(500); // give children a moment to handle SIGINT
    if (env->monitor_pid > 0) { int s; waitpid(env->monitor_pid, &s, WNOHANG) || waitpid(env->monitor_pid, &s, 0); }
    if (env->server_pid > 0)  { int s; waitpid(env->server_pid,  &s, WNOHANG) || waitpid(env->server_pid,  &s, 0); }
    env->server_pid = env->monitor_pid = -1;
}

void bootstrap(Rcss2dTest* env) {
    

    invictus_player_reset(env->rl_player, env->host, env->player_port, env->rl_team_name, 0);
    // msleep(100);

    invictus_trainer_reset(env->rl_trainer, env->host, env->offline_coach_port);
    // msleep(100);

    invictus_wait_for_decision(env->rl_player);
    invictus_trainer_wait_for_decision(env->rl_trainer);

    float dummy_action[] = {2, 0, 0};
    if (invictus_player_act(env->rl_player, dummy_action, 1 + NUM_ATNS) != 0) {
        printf("act failed\n");
    }

    invictus_trainer_do_eye(env->rl_trainer, true);

    invictus_trainer_do_change_mode(env->rl_trainer, INVICTUS_PM_PlayOn);

    if (invictus_trainer_send_action(env->rl_trainer, false) != 0) {
        printf("send_action eye on failed\n");
    }

    invictus_wait_for_decision(env->rl_player);
    invictus_trainer_wait_for_decision(env->rl_trainer);
}

void init(Rcss2dTest* env) {
    env->log_file = NULL;

    char start_timestamp[20];
    get_current_timestamp_str(start_timestamp, sizeof(start_timestamp));
    sprintf(env->server_log_file_name, "%s-%u-server-out.log", start_timestamp, env->rng);
    char server_text_log_fixed_name[128];
    sprintf(server_text_log_fixed_name, "%s-%u-server", start_timestamp, env->rng);
    char env_log_file_name[256];
    sprintf(env_log_file_name, "%s-%u-env.log", start_timestamp, env->rng);

    // env->log_file = fopen(env_log_file_name, "a");
    // if (!env->log_file) {
        // perror("Failed to open log file");
        // exit(EXIT_FAILURE);
    // }

    // pthread_t ptid = pthread_self();
    // pid_t tid = gettid();
    // log_message(env->log_file,"[init] PTID:  %lu  TID:  %d  seed:  %u\n", (unsigned long)ptid, 1, env->rng);

    env->reset_counter = 0;
    env->step_counter = 0;
    
    // The vectorizer (vecenv.h) assigns the four float* buffers and sets num_agents.
    // We just zero local accumulators here.
    env->server_pid = -1;
    env->monitor_pid = -1;
    env->rl_player = (InvictusPlayer*)malloc(sizeof(InvictusPlayer));
    env->rl_trainer = (InvictusTrainer*)malloc(sizeof(InvictusTrainer));
    env->boostrap_done = false;

    int err = 0;
    err = get_ports_for_rcss(&env->player_port, &env->coach_port, &env->offline_coach_port, &env->rng);
    if (err < 0) {
        printf("rcss2d: failed to get ports for rcss\n");
        exit(err);
    }

    env->host = "127.0.0.1";
    env->rl_team_name       = "t2";
    if (env->use_monitor < 0) env->use_monitor = 0;

    unsigned int seed = env->rng % 1000;
    
    env->server_pid = launch_server(env->player_port, env->coach_port, env->offline_coach_port, seed, "/dev/null", server_text_log_fixed_name);
    msleep(100);

    if (env->use_monitor) {
        env->monitor_pid = launch_monitor(env->player_port, "/dev/null");
        msleep(100);
    }

    if (env->target_distance < 0) env->target_distance = 1;
    if (env->max_episode_tick <= 0) env->max_episode_tick = 2000;
    if (env->step_penalty < 0) env->step_penalty = 0;
}

// Required function
void c_reset(Rcss2dTest* env) {
    env->reset_counter++;
    // pthread_t ptid = pthread_self();
    // pid_t tid = gettid();
    // log_message(env->log_file,"[reset %d] PTID:  %lu  TID:  %d  seed:  %u\n", env->reset_counter, (unsigned long)ptid, 1, env->rng);

    if (!env->boostrap_done) {
        // log_message(env->log_file,"[reset %d] bootstrap is not done! skipping...\n", env->reset_counter);
        return;
    }

    if (!env->rl_trainer->server_alive) {
        printf("server is dead!\n");
        c_close(env);
        exit(1);
    }

    // printf("reset at tick %d\n", env->tick);
    // fflush(stdout);

    memset(env->observations, 0, OBS_SIZE*sizeof(float));
    env->tick = 0;
    env->accumulated_step_duration = 0;

    // player reached the ball. move the ball to new position
    float new_x = (rand_r(&env->rng) % PITCH_LENGTH) - (PITCH_LENGTH / 2.0);
    float new_y = (rand_r(&env->rng) % PITCH_WIDTH) - (PITCH_WIDTH / 2.0);

    invictus_trainer_do_ball_move(env->rl_trainer, new_x, new_y);

    float p_new_x = (rand_r(&env->rng) % PITCH_LENGTH) - (PITCH_LENGTH / 2.0);
    float p_new_y = (rand_r(&env->rng) % PITCH_WIDTH) - (PITCH_WIDTH / 2.0);
    invictus_trainer_do_player_move(env->rl_trainer, env->rl_player->team, env->rl_player->unum, p_new_x, p_new_y);

    if (invictus_trainer_send_action(env->rl_trainer, false) != 0) {
        printf("send_action eye on failed\n");
    }

    float dummy_action[] = {2, 0, 0};
    if (invictus_player_act(env->rl_player, dummy_action, 1 + NUM_ATNS) != 0) {
        printf("act failed\n");
    }

    invictus_wait_for_decision(env->rl_player);
    invictus_trainer_wait_for_decision(env->rl_trainer);

    int obs_idx = 0;
    InvictusTrainerBall* ball = &env->rl_trainer->visual.ball;
    InvictusTrainerPlayer* player = &env->rl_trainer->visual.players[0];
    env->observations[obs_idx++] = (ball->x - player->x) / (float)PITCH_LENGTH_TOTAL;
    env->observations[obs_idx++] = (ball->y - player->y) / (float)PITCH_WIDTH_TOTAL;

    float dx = ball->x - player->x;
    float dy = ball->y - player->y;
    env->starting_distance = sqrtf(dx*dx + dy*dy);
    env->previous_distance = env->starting_distance;
}


// Required function
void c_step(Rcss2dTest* env) {
    if (!env->boostrap_done) {
        bootstrap(env);
        env->boostrap_done = true;
        c_reset(env);
    }

    if (!env->rl_trainer->server_alive) {
        printf("server is dead!\n");
        env->server_pid = -1;
        c_close(env);
        exit(1);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    env->tick += 1;

    env->rewards[0] = 0.0f;   // will be filled after receive
    env->terminals[0] = 0.0f;

    float new_actions[1 + NUM_ATNS];
    new_actions[0] = 1;

    // Second action → [0, 100]
    int bins1 = 21;                     // must match ACT_SIZES[1]
    int b1 = (int)env->actions[0];
    if (b1 < 0) b1 = 0;
    if (b1 >= bins1) b1 = bins1 - 1;
    new_actions[1] = (b1 / (float)(bins1 - 1)) * 100.0f;

    // Third action → [-180, 180]
    int bins2 = 37;                     // must match ACT_SIZES[2]
    int b2 = (int)env->actions[1];
    if (b2 < 0) b2 = 0;
    if (b2 >= bins2) b2 = bins2 - 1;
    new_actions[2] = -180.0f + (b2 / (float)(bins2 - 1)) * 360.0f;
    
    if (invictus_player_act(env->rl_player, new_actions, 1 + NUM_ATNS) != 0) {
        printf("act failed\n");
    }

    long cycle = env->rl_trainer->current_cycle;
    if (cycle > 0 && (cycle % 100 == 0)) {
        // printf("cycle %ld: sending recover\n", cycle);
        // fflush(stdout);
        invictus_trainer_do_recover(env->rl_trainer);
    }

    // update_goals(env);

    if (invictus_trainer_send_action(env->rl_trainer, false) != 0) {
        printf("send_action failed\n");
    }
    
    invictus_wait_for_decision(env->rl_player);\
    invictus_trainer_wait_for_decision(env->rl_trainer);

    int obs_idx = 0;
    InvictusTrainerBall* ball = &env->rl_trainer->visual.ball;
    InvictusTrainerPlayer* player = &env->rl_trainer->visual.players[0];
    env->observations[obs_idx++] = (ball->x - player->x) / (float)PITCH_LENGTH_TOTAL;
    env->observations[obs_idx++] = (ball->y - player->y) / (float)PITCH_WIDTH_TOTAL;

    // printf("p ball_dist %.1f  ball_dir %.1f\n",
    //         env->rl_players[0].sensors.visual.balls[0].dist, env->rl_players[0].sensors.visual.balls[0].dir);
    // fflush(stdout);

    float dx = ball->x - player->x;
    float dy = ball->y - player->y;
    float dist = sqrtf(dx*dx + dy*dy);
    float dist_change = env->previous_distance - dist;

    env->rewards[0] = (dist_change / env->starting_distance) - env->step_penalty;
    env->previous_distance = dist;

    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t start_ns = (uint64_t)start.tv_sec * 1000000000ULL + start.tv_nsec;
    uint64_t end_ns = (uint64_t)end.tv_sec * 1000000000ULL + end.tv_nsec;
    uint64_t elapsed_ns = end_ns - start_ns;
    float elapsed_us = (float)elapsed_ns / 1000;
    env->accumulated_step_duration += elapsed_us;
    // log_message(env->log_file, "step elapsed:  %f\n", elapsed_us);

    if (env->tick > env->max_episode_tick
            /* || player->x < -(PITCH_LENGTH / 2.0) - (PITCH_MARGIN / 2.0)
            || player->y < -(PITCH_WIDTH / 2.0) - (PITCH_MARGIN / 2.0)
            || player->x > (PITCH_LENGTH / 2.0) + (PITCH_MARGIN / 2.0)
            || player->y > (PITCH_WIDTH / 2.0) + (PITCH_MARGIN / 2.0) */) {
        env->terminals[0] = 1;
        env->rewards[0] -= 1;
    }

    if (dist < env->target_distance) {
        env->terminals[0] = 1;
        env->rewards[0] += 1;
    }

    env->log.episode_length += 1;
    env->log.episode_return += env->rewards[0];

    if (env->terminals[0] == 1) {
        env->log.avg_duration_per_tick = env->accumulated_step_duration / env->log.episode_length;
        env->log.score = (env->starting_distance - dist) / env->log.episode_length;
        env->log.perf = (dist < env->target_distance) ? 1 : 0;
        env->log.n++;
        c_reset(env);
    }
}

// Optional render (mostly useful for --local standalone). The real viz is usually the rcssmonitor child.
// We keep a trivial window so the standard ESC-exit behavior of Puffer ocean envs works.
void c_render(Rcss2dTest* env) {
    return;
}

// Required function. Should clean up anything you allocated.
// Do not free the four float* buffers (observations/actions/rewards/terminals) — vecenv owns them.
void c_close(Rcss2dTest* env) {
    printf("closing env with seed %u\n", env->rng);
    fflush(stdout);

    invictus_player_close(env->rl_player);
    invictus_trainer_close(env->rl_trainer);

    if (env->server_pid > 0)
        stop_children(env);

    free(env->rl_player);

    if (env->log_file) {
        fclose(env->log_file);
    }

    printf("done closing env with seed %u\n", env->rng);
    fflush(stdout);
}
