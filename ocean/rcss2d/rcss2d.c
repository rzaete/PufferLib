#define _GNU_SOURCE
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#include "rcss2d.h"
#include "puffernet.h"

void demo1() {
    unsigned int base_player_port = 6000, base_coach_port = 6002;

    for (int t = 0 ; t < 1 ; t++) {
        unsigned int player_port = base_player_port + 10 * t, coach_port = base_coach_port + 10 * t;

        const char *host = "localhost", 
            *teamname1 = "t1", 
            *teamname2 = "t2";
        pid_t team1_pid_list[12], team2_pid_list[12];

        launch_server(player_port, coach_port, coach_port-1, 2, NULL);
        msleep(500);
        launch_monitor(player_port, NULL);
        msleep(100);

        launch_team(host, player_port, coach_port, teamname1, team1_pid_list, NULL);
        launch_team(host, player_port, coach_port, teamname2, team2_pid_list, NULL);


        // for (int i = 0 ; i < 12 ; i++) {
        //     process_send_sigint(team1_pid_list[i]);
        //     process_send_sigint(team2_pid_list[i]);
        // }
        // stop_server(server_pid);
        sleep(3);
    }

    int status;
    pid_t terminated_pid;
    
    // waitpid(-1, ...) matches ANY child process
    while ((terminated_pid = waitpid(-1, &status, 0)) > 0) {
        printf("Parent reaped Child PID %d\n", terminated_pid);
    }
}

void demo2()
{
    srand(1);

    const int num_agents = 1;
    float* observations = (float*)calloc(num_agents * OBS_SIZE, sizeof(float));
    float* actions = (float*)calloc(num_agents * NUM_ATNS, sizeof(float));
    float* rewards = (float*)calloc(num_agents, sizeof(float));
    float* terminals = (float*)calloc(num_agents, sizeof(float));

    if (!observations || !actions || !rewards || !terminals) {
        printf("demo2: alloc failed\n");
        free(observations); free(actions); free(rewards); free(terminals);
        return;
    }

    Rcss2d env = {
        .num_agents = num_agents,
        .observations = observations,
        .actions = actions,
        .rewards = rewards,
        .terminals = terminals,
        .rng = 1,
        .use_monitor = 1,
    };
    init(&env);

    printf("demo2: starting rcss2d match with %d RL agents (random actions)\n", num_agents);
    fflush(stdout);

    c_reset(&env);

    // Basic health check: if the first RL client didn't connect during c_reset,
    // the handshake or timing failed (c_reset already stopped children).
    if (env.num_agents <= 0 || !env.rl_players || !env.rl_players[0].comm.connected) {
        printf("demo2: c_reset failed to connect RL clients; aborting demo\n");
        c_close(&env);
        free(observations);
        free(actions);
        free(rewards);
        free(terminals);
        return;
    }

    printf("demo2: RL clients connected, starting step loop\n");
    fflush(stdout);

    // Drive a reasonable number of steps. In synch mode c_step blocks until
    // decision data arrives, so the loop pace is server-driven.
    int step = 0;
    while (env.rl_players[0].server_alive) {
        memset(actions, 0, num_agents * NUM_ATNS * sizeof(float));
        for (int i = 0; i < num_agents ; i++) {
            int idx = i * NUM_ATNS;
            int r = rand() % 10;
            int body_cmd = 1;  /* default dash */
            if (r < 6) body_cmd = 1;      /* dash */
            else if (r < 10) body_cmd = 2; /* turn */
            // else body_cmd = 3;            /* kick */
            actions[idx] = (float)body_cmd;
            if (body_cmd == 1) {            /* dash power, rel_dir */
                actions[idx + 1] = 20.0f + (float)(rand() % 60);
                actions[idx + 2] = 0.0f;
            } else if (body_cmd == 2) {     /* turn moment */
                actions[idx + 1] = (float)(rand() % 41 - 20);
            } else if (body_cmd == 3) {     /* kick power, rel_dir */
                actions[idx + 1] = 10.0f + (float)(rand() % 70);
                actions[idx + 2] = (float)(rand() % 31 - 15);
            }

            /* occasional neck turn */
            if ((rand() % 4) == 0) {
                actions[idx + 3] = 1.0f;
                actions[idx + 4] = (float)(rand() % 30 - 15);
            }
        }

        c_step(&env);
        // c_render(&env);

        // if ((step % 100) == 0 || step < 3) {
        //     printf("step %4d  return=%.4f  len=%.0f  n=%.0f  score=%.0f\n",
        //            step, env.log.episode_return, env.log.episode_length,
        //            env.log.n, env.log.score);
        //     fflush(stdout);
        // }

        // Optional: stop after first terminal event for quicker demo runs.
        // (internal episodes continue the match via server auto-kickoff)
        int any_term = 0;
        for (int a = 0; a < num_agents; a++) {
            if (terminals[a] > 0.5f) { any_term = 1; break; }
        }
        if (any_term) {
            printf(">>> terminal at step %d (goal or safety)\n", step);
        }
        ++step;
    }

    printf("demo2: finished after %d steps. final log: return=%.3f len=%.0f n=%.0f score=%.0f\n",
           step, env.log.episode_return, env.log.episode_length, env.log.n, env.log.score);

    c_close(&env);

    free(observations);
    free(actions);
    free(rewards);
    free(terminals);
}

int main() {
    // demo();
    demo2();
}
