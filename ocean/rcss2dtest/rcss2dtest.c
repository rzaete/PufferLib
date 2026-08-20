#define _GNU_SOURCE
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#include "rcss2dtest.h"
#include "puffernet.h"

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

    Weights* weights = load_weights("checkpoints/rcss2dtest/1782422465496/0000000101426688.bin");
    int logit_sizes[] = {21, 37};
    PufferNet* net = make_puffernet(weights, 1, OBS_SIZE, 128, 1, logit_sizes, NUM_ATNS);

    Rcss2dTest env = {
        .num_agents = num_agents,
        .observations = observations,
        .actions = actions,
        .rewards = rewards,
        .terminals = terminals,
        .rng = 1,
        .use_monitor = 1,
        .target_distance = 4,
        .max_episode_tick = 2000,
        .step_penalty = 0.001,
    };
    init(&env);

    printf("demo2: starting rcss2d match with %d RL agents (random actions)\n", num_agents);
    fflush(stdout);

    c_reset(&env);

    c_step(&env);

    printf("demo2: RL clients connected, starting step loop\n");
    fflush(stdout);

    // Drive a reasonable number of steps. In synch mode c_step blocks until
    // decision data arrives, so the loop pace is server-driven.
    int step = 0;
    while (env.rl_trainer->server_alive) {
        // memset(actions, 0, num_agents * NUM_ATNS * sizeof(float));
        // for (int i = 0; i < num_agents ; i++) {
        //     int idx = i * NUM_ATNS;
        //     actions[idx++] = rand() % 21;
        //     actions[idx++] = rand() % 37;
        // }

        float obs_f[OBS_SIZE];
        for(int i=0; i<OBS_SIZE; i++) obs_f[i] = (float)env.observations[i];
        forward_puffernet(net, obs_f, env.actions);

        c_step(&env);

        // int any_term = 0;
        // for (int a = 0; a < num_agents; a++) {
        //     if (terminals[a] > 0.5f) { any_term = 1; break; }
        // }
        // if (any_term) {
        //     printf(">>> terminal at step %d (goal or safety)\n", step);
        // }
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
