# rcssemptynet (Ocean)

**Stage-1 empty-net scoring curriculum.** Train a policy that can finish
against an empty net (no opponents, no selfplay pool). The checkpoint is
the warm-start prior for competitive `rcsslocal` 2v2 selfplay.

## Roster

One-sided empty net: all RL agents play on a single side.

| Config | Meaning |
|--------|---------|
| `num_players=2`, `side=0` | Default: two LEFT agents (recommended for 2v2 transfer) |
| `num_players=1`, `side=0` | Single LEFT attacker |
| `num_players=N`, `side=1` | RIGHT-only (LEFT filler seats the physical side) |

- Competitive 2v2 is `rcsslocal` (both sides non-zero).
- `side=0` → LEFT team `t1` (attack +x / right goal)
- `side=1` → RIGHT team `t2` (attack −x / left goal)
- Agents `[0, num_players)` all on `side`; opposing side is empty.
- `total_agents` must be divisible by `num_players`.

Config (`[env]`): `num_players`, `side` (`0`=LEFT, `1`=RIGHT).

## Task

Two terminal modes (`early_terminal` in config):

| Mode | `early_terminal` | Episode ends on | Max length |
|------|------------------|-----------------|------------|
| Fixed horizon | `0` | tick only | lerps with curriculum |
| Early terminal | `1` | goal / fault / timeout | constant `starting_horizon_ticks` |

- **Fixed**: goals / faults → event rewards, then soft-respawn ball+players in `PLAY_ON`.
- **Early**: first goal / fault ends the episode (plus tick timeout).
- Shared world → **shared terminal** for all agents; full env reset.

## Reward (per agent)

```
r = + goal_reward     # own-team goal (may fire multiple times)
  - goal_reward     # opponent goal (n/a with empty net)
  + fault_reward    # only the side at fault (typically negative)
```

## Difficulty (adaptive curriculum)

`difficulty` ∈ [0, 1] drives both spawn region and (fixed-horizon) episode length.

Ball and each player sampled independently inside an expanding AABB, facing the ball.

- **difficulty = 0**: small box near the **attack goal** (`|y|≤7`, depth ~12 m)
- **difficulty = 1**: whole pitch with a 1–2 m line margin
- Anchored to the RL side’s target goal (RIGHT goal for `side=0`, LEFT for `side=1`)

**Fixed horizon only** — episode length also grows with difficulty:

```
fixed_horizon_ticks = starting_horizon_ticks + difficulty · (HALF_TICKS − starting_horizon_ticks)
```

`HALF_TICKS` is one RCSS half (3000). **Early terminal** — timeout is always
`starting_horizon_ticks` (spawn box still expands with difficulty).

Per-env adaptive update after every episode:

```
success ← (goal_diff > 0)   # goals_for − goals_against from RL side
success_rate ← (1 − success_update_rate)·success_rate + success_update_rate·success
if success_rate > target_success_rate:
    difficulty ← min(1, difficulty + difficulty_rate·(success_rate − target_success_rate))
```

Difficulty only increases (never decreases). `target_success_rate` is the
threshold that must be beaten before difficulty ramps. Empty-net default is
`0.70` (higher than competitive 2v2).

Config: `starting_horizon_ticks`, `difficulty`, `success_update_rate`,
`target_success_rate`, `difficulty_rate`.

### Metrics

| Metric | Meaning |
|--------|---------|
| `difficulty` | Mean task difficulty (0→1; spawn + fixed horizon) |
| `success_rate` | Smoothed rate of episodes with positive goal diff |
| `perf` | Early: `1 − tick/starting_horizon_ticks` on success, else 0. Fixed: goals_for · starting_horizon_ticks / fixed_horizon_ticks |
| `score` | Early: +1 success / 0 timeout / −1 fault. Fixed: goal_diff |
| `left_goal` / `right_goal` / `fault_left` / `fault_right` | Event counts per finished episode (may sum >1; flushed only at episode end) |

No `hist_*` / pure-selfplay split — this env has no selfplay pool.

### Demo

`./build.sh rcssemptynet --cpu` produces `./rcssemptynet` from `src/puffercpu.c`.
It loads `config/default.ini` + `config/rcssemptynet.ini`. Override `[env]` /
`[policy]` keys with `section.key=value`. Training keeps `use_monitor=0`; pass
`env.use_monitor=1` to open soccerwindow in the eval binary.

```bash
./build.sh rcssemptynet --cpu
./rcssemptynet latest env.use_monitor=1
./rcssemptynet latest env.use_monitor=1 env.difficulty=1
./rcssemptynet path/to/run.bin env.use_monitor=1
./rcssemptynet --headless --base.eval_episodes=1
```

## Observations (`OBS_SIZE` in `rcssemptynet.h`, **per agent**)

Same layout as `rcsslocal` so weight blobs transfer:

| Block | Size |
|-------|------|
| Ball xy, vel | 4 |
| Me xy, vel, body | 5 |
| Me stamina, effort, stamina_capacity | 3 |
| Teammate slots (max 1 × xy/vel/body) | 5 |
| Opponent slots (max 2 × xy/vel/body) | 10 |

Self stamina only (teammates/opponents do not expose stamina). Normalized by
rcssserver defaults (`stamina/8000`, effort raw ~`[0.6,1]`, `capacity/130600`).

With `num_players=2`, teammate is live and opponents are zero-padded. Ego-attack
frame (RIGHT mirrored) matches competitive transfer.

Stamina recovery runs once per episode reset (`stadium_recoveryPlayers`), not
mid-episode.

## No selfplay pool

`[selfplay] enabled = 0`. All agents use `policy = 0`. `tag` /
`boundary_reached` exist on `Env` because 5.0 always has them; this env
does not drive a hist-policy pool.

## Build / train / transfer

```bash
./build.sh rcssemptynet                 # native trainer -> ./puffer
./puffer train
# When difficulty ~ 1 and goal rate is solid:
./build.sh rcsslocal && ./puffer train --load-model-path checkpoints/rcssemptynet/<run_id>/<step>.bin
./build.sh rcssemptynet --cpu
./rcssemptynet latest env.use_monitor=1
```

`[policy] hidden_size` / `num_layers` must match `rcsslocal` for warm-start
(exact weight file size check). `puf_init` / `puf_log` live in
`ocean/rcssemptynet/rcssemptynet.h` (no `binding.c`).
