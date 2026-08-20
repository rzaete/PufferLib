# RCSS2D in PufferLib (Ocean)

**First step for any agent/human**: Read the `/docs` folder (start with `docs/handbook/README.md` and the numbered chapters) to gain the necessary background knowledge on Ocean env contracts, `vecenv.h`, multi-agent patterns, build process, and coding conventions before touching this environment.

This document is the rcss2d-specific companion. It describes the *current* state of the implementation so you can work on it without stale assumptions.

---

## High-Level Design

- One `Rcss2d` env = one full RCSS match.
- Hybrid roster per match:
  - 1 `rcssserver` (synch_mode=on, auto_mode=on, logging off, short waits).
  - Team "t1": full 11 scripted (goalie + 10 field + coach) using helios-base `sample_player` / `sample_coach`.
  - Team "t2": 9 scripted + **exactly 2 RL players** (the RL players are direct UDP clients, *not* child processes).
- Only the 2 RL agents are exposed to the trainer (`num_agents=2`).
- Scripted players are forked children (via `process_helper.h`).
- RL players use the self-contained pure-C "Invictus" client (`invictus_client.h` + `invictus_player.h`) — no librcsc / C++ bridge for the RL side.
- Server steps only when all players send timely commands (synch mode). RL clients must keep up.
- Early terminal + reward on goal (plus living penalty + safety max-cycle terminal).

The RL client implements the RCSS UDP protocol directly (init, see, sense_body, think, playmode, effectors for dash/turn/kick/... + (done) in synch mode).

---

## Key External Components

All paths below are the ones currently hardcoded in the source.

### RCSS Simulator
- `/home/babaeti2/rcssserver/build/rcssserver`
- `/home/babaeti2/rcssmonitor/build/rcssmonitor` (optional, via `use_monitor`)

Server flags used: `synch_mode=on`, `auto_mode=on`, `game_logging=off`, `text_logging=off`, `kick_off_wait=10`, `connect_wait=30`, random seeds.

### Scripted Players (Helios)
- `/home/babaeti2/helios/helios-base/build/bin/sample_player`
- `/home/babaeti2/helios/helios-base/build/bin/sample_coach`
- Configs: `player.conf`, `formations-dt/`, `coach.conf`
- Children are launched with explicit `LD_LIBRARY_PATH=/home/babaeti2/helios/librcsc/out/lib`

**Note**: librcsc / helios are only required for the *scripted* players. The RL players do not link or use them.

### PufferLib rcss2d Files (current)
- `ocean/rcss2d/rcss2d.h` — Env struct (`Rcss2d`), launch helpers, `c_reset`, `c_step`, `compute_observations`, `compute_rewards_and_terminals`, `c_close`, `c_render`, `stop_children`, team launchers.
- `ocean/rcss2d/rcss2d.c` — Standalone test `main()` (launches full matches for manual debugging).
- `ocean/rcss2d/binding.c` — `#define OBS_SIZE 64`, `NUM_ATNS 8`, `ACT_SIZES {1,1,1,1,1,1,1,1}`, `Env Rcss2d`, `my_init` (sets `num_agents=2`, `use_monitor=0`), `my_log`.
- `ocean/rcss2d/invictus_client.h` — Pure-C UDP transport + full protocol parsers (see/sense_body/hear/init/server_param) + effector -> wire command serialization.
- `ocean/rcss2d/invictus_player.h` — High-level RL API on top (`invictus_player_reset`, `invictus_player_compute_observation`, `invictus_player_act`) + body-command guards + state.
- `ocean/rcss2d/process_helper.h` — `process_launch_async`, `process_send_sigint`, etc.
- `config/rcss2d.ini` — `total_agents=2`, `use_monitor=0.0` recommended to start.
- `src/vecenv.h` — Core contract (read the handbook first).

Other references:
- `ocean/target/` — best simple multi-agent template (independent agents sharing a world).
- `build.sh` — has rcss2d special cases (still carries some legacy librcsc logic).

---

## Current Struct & Data Flow (from code)

```c
typedef struct {
    Log log;
    Client* client;
    float* observations; float* actions; float* rewards; float* terminals;
    int num_agents;               // 2
    unsigned int rng;
    pid_t server_pid, monitor_pid;
    pid_t scripted_pids[16];
    int num_scripted_pids;
    InvictusPlayer* rl_players;   // calloc(num_agents)
    unsigned int player_port, coach_port;
    const char* host;
    const char* scripted_team;    // "t1"
    const char* rl_team;          // "t2"
    int use_monitor;
    // prev_* for reward + playmode transition detection
    ...
} Rcss2d;
```

**c_reset**:
1. `stop_children` (SIGINT + waitpid on previous).
2. Pick ports from rng.
3. Launch server (and optional monitor).
4. Launch full t1 (goalie + 10 players + coach).
5. Launch partial t2 scripted (goalie + (10 - num_agents) players + coach).
6. For each RL player: `invictus_player_reset(...)` → client connect + `(init ...)` send.
7. `compute_observations()` (which internally waits for first decision data).

**c_step**:
1. For each RL agent: `invictus_player_act(...)` (maps 8-float action vector into body/neck/misc effectors, flushes with `(done)` in synch mode).
2. `compute_observations()` → each calls `invictus_player_compute_observation` → `invictus_wait_for_decision` (select + parse until think/see + action_required) + pack raw obs.
3. `compute_rewards_and_terminals()` (step penalty + goal +/-1 + terminal on goal playmode transition or MAX_CYCLES).
4. Accumulate log (episode_return, length, n, score on RL goal).

Observation size is 64 floats (body state + ball polar + up to 5 other players + cycle/playmode/unum/side + padding). See `invictus_get_raw_observation`.

Action interpretation (see `invictus_player_act`): first value selects body command type (1=dash,2=turn,...), subsequent values are params; heads 4-7 for neck/misc.

---

## Build & Runtime Notes (Current)

- rcss2d builds with the standard clang path (no special C++ or librcsc flags at build time). Only the forked scripted players require `LD_LIBRARY_PATH` at runtime pointing at librcsc.
- Runtime LD_LIBRARY_PATH is **still required** because the forked scripted players need `librcsc.so`.
- `_GNU_SOURCE` for `execvpe`.
- Port allocation is simple rng-based (per-env).
- Start with `total_agents=2`, `num_buffers=1` — this env is expensive (forking ~18-20 processes per reset).
- Always `pkill -x rcssserver sample_player rcssmonitor` between runs.

---

## Current Status & Limitations

Working:
- Server + optional monitor + scripted children + 2 direct RL clients.
- Full init handshake, unum/side, sensory parsing, action mapping for many effectors.
- Rewards, early goal terminals, safety horizon.
- Clean reaping.
- Smoke via standalone and Python vec.

Performance:
- Reset is slow (process spawn heavy).
- Steps are functional but not fast.

Known limitations / accepted:
- Hardcoded absolute paths for server/helios (and msleep launch timing, blocking UDP in step, reset cost) are accepted for this hybrid design.
- Obs vector (64) is relative/polar only (first 5 players); richer state needs coach/fullstate.
- Action: 8-dims with first selecting body cmd (binned 0-6), later providing binned params (scaled in decoder). See binding.c + invictus_player_act.
- Reward minimal (step + goal). No progress/possession yet.
- Fresh full roster on c_reset (expensive); episodes can span kickoffs inside one match via internal reset.
- Determinism limited by external processes + UDP + scripted helios.
- (build.sh no longer carries rcss special cases; pure clang path works).

Debug: server logs (when not disabled), the printf in rcss2d.c / launchers, and stderr logs inside invictus_*.

---

## Important Code Locations

- `ocean/rcss2d/rcss2d.h` — main lifecycle, launches, reward/obs logic.
- `ocean/rcss2d/invictus_player.h` + `invictus_client.h` — the RL client implementation. Edit here for protocol, new actions, richer obs packing.
- `ocean/rcss2d/binding.c` — sizes and init (OBS_SIZE / NUM_ATNS / ACT_SIZES live here).
- `ocean/rcss2d/process_helper.h` — process control.
- `build.sh` — search "rcss2d".
- `config/rcss2d.ini`
- `src/vecenv.h` + handbook chapters 01-06, 08.

External (for the scripted side only):
- helios sample_player sources and librcsc headers (only if you need to understand the 9 scripted teammates).

---

## Quick Start

```bash
# Build
./build.sh rcss2d --local     # standalone binary
./build.sh rcss2d --cpu       # for Python (activate venv first)

# Smoke (standalone)
LD_LIBRARY_PATH=/home/babaeti2/helios/librcsc/out/lib ./rcss2d

# Python smoke
. venv/bin/activate
LD_LIBRARY_PATH=/home/babaeti2/helios/librcsc/out/lib:$LD_LIBRARY_PATH python -c '
from pufferlib import _C
import ctypes, numpy as np
args = {"vec": {"total_agents": 2, "num_buffers": 1}, "env": {"use_monitor": 0.0}}
ve = _C.create_vec(args)
ve.reset()
na = ve.total_agents * ve.num_atns
actions = np.zeros(na, dtype=np.float32)
ve.cpu_step(actions.ctypes.data)
print("ok")
ve.close()
'
```

Clean up children between experiments.

---

## Gotchas

- Every client (including RL) must send actions promptly or the match dies.
- In synch mode a `(done)` is appended automatically by the client when flushing.
- Body commands are guarded by sense_body counters (see `invictus_can_issue_body_cmd`).
- `compute_observations` blocks (via select) until the client sees a think/see that requires action.
- Ports must not collide across vectorized envs.
- Spawning the helios roster dominates reset time.

This file (plus the `/docs` handbook) is what you read first when asked to work on `ocean/rcss2d`.
