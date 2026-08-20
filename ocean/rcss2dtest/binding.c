#include "rcss2dtest.h"

// Per-RL-agent observation size. Must match what compute_observations + the wrapper write.
// v1 layout in rcss_client.cpp writes/pads to 16 values.
#define OBS_SIZE 2
#define NUM_ATNS 2
// 8 action dims per agent. First value selects body cmd (0=none,1=dash,2=turn,3=kick,4=tackle,5=catch,6=move).
// Subsequent provide binned params (power/dir/moments mapped from 0..N-1 by the act decoder).
// This allows the multi-discrete policy heads to produce varied commands + param levels.
#define ACT_SIZES {21, 37}
#define OBS_TENSOR_T FloatTensor

#define Env Rcss2dTest
#include "vecenv.h"

void my_init(Env* env, Dict* kwargs) {
    env->num_agents = 1;

    env->use_monitor = 0;
    DictItem* mon = dict_get_unsafe(kwargs, "use_monitor");
    if (mon) env->use_monitor = (int)mon->value;

    env->target_distance = 0;
    DictItem* distance = dict_get_unsafe(kwargs, "target_distance");
    if (distance) env->target_distance = (int)distance->value;

    env->max_episode_tick = 0;
    DictItem* max_episode_tick = dict_get_unsafe(kwargs, "max_episode_tick");
    if (max_episode_tick) env->max_episode_tick = (int)max_episode_tick->value;

    env->step_penalty = 0;
    DictItem* step_penalty = dict_get_unsafe(kwargs, "step_penalty");
    if (step_penalty) env->step_penalty = (int)step_penalty->value;

    // Ports / teams etc. can be extended later.
    init(env);
}

void my_log(Log* log, Dict* out) {
    dict_set(out, "perf", log->perf);
    dict_set(out, "score", log->score);
    dict_set(out, "episode_return", log->episode_return);
    dict_set(out, "episode_length", log->episode_length);
    dict_set(out, "duration_per_tick", log->avg_duration_per_tick);
}
