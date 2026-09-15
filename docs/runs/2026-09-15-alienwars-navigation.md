# AlienWars navigation MVP — September 15, 2026

The native PufferLib loop now trains a small scout in the existing AlienWars
world. One training seed produced **192/192 arrivals** in the final stratified
held-out evaluation: 64 surface, 64 bridge and 64 tunnel episodes. Random actions
achieved 24/192; the direct-goal baseline achieved 184/192. This is a short-range
navigation result, with ideal goal localization, not evidence of combat,
long-range planning or robustness across training seeds.

[Task contract and commands](../NAVIGATION_RL.md) ·
[Measurements and artifact hashes](2026-09-15-alienwars-navigation.json)

## Source and machine

Work happened on `codex/alienwars-navigation` in an isolated worktree.
Final environment, training and evaluated native/WASM source:
`7ad9735900d2ee711601b05848869e3ecaec2dad` (clean during build/evaluation).
The dashboard replay and this report were added afterward. Terrain generation
is unchanged; the task uses shared support, collision, sensors and motion.

Training: one RTX 4090, CUDA 13.0.88, driver 580.173.02, Clang 18.1.3 on Linux.
CPU environment, native CUDA PPO, bf16, 178,688 parameters, hidden size 128,
two MinGRU layers. Other GPU workloads were preserved; dashboard utilization
and VRAM are whole-device measurements. The CPU viewer uses float32 inference.

Common settings: 256 agents, 8 CPU threads, 2 buffers, horizon 64, minibatch
4096, gamma .99, GAE .95, synchronous training, recurrent resets at terminals.
Training maps 71–78 (pilot 71–74), episode and learner seed 173. Validation maps
1001–1004, episode seed 8001, sampling seed 8002. Final maps 10001–10008, episode
seed 9001, sampling seed 9002. Task sampling is not category balanced in mixed
training; final evaluation is balanced.

## Runs, including failures

| Run | Task | Steps | Trainer seconds | Completed-episode arrival |
| --- | --- | ---: | ---: | ---: |
| `nav-pilot-s173` | Surface, 4 maps | 1,048,576 | 44.47 | 17.76% |
| `nav-surface-s173-8m` | Surface, 8 maps | 8,388,608 | 414.69 | 99.57% |
| `nav-mixed-s173-4m` | Mixed, aborted | 802,816 | 52.08 | 30.27% |
| `nav-mixed-warm-s173-4m` | Mixed, verified initialization | 4,194,304 | 285.25 | 99.98% |
| `nav-final-s173-1m` | Mixed, dry-support guard | 1,048,576 | 69.51 | 100.00% |

Rates aggregate logged completed episodes, not held-out performance. All logged
numeric metrics were finite. The successful curriculum used 13,631,488 steps
and 769.45 trainer seconds. Startup/build time is separate: the final process
took 99.74 seconds, including about 30 seconds of map preparation. Final-stage
throughput was about 15,086 steps/second including PPO, excluding preparation.

Learning rates were .008, .015, .008, .008 and .002 respectively;
`min_lr_ratio=.2`. Entropy coefficient was .01 for the pilot and .001 afterward.
The surface stage began from random initialization. Each later successful stage
loaded its predecessor's weights with fresh optimizer, RNG and recurrent state.
The final stage followed a movement fix rejecting submerged surface beds while
preserving distinct bridge decks and underground floors.

The first mixed attempt exposed an upstream issue: training ignored
`load_model_path`. It was stopped and is labeled **ABORTED** in the dashboard.
The fix loads explicit weights before rollout, checks size/finiteness, and saves
`initial.bin`; hashes confirmed both subsequent initializations exactly matched
their preceding checkpoints. This is weight initialization, not optimizer resume.

Checkpoint SHA-256 values:

- Surface: `1bb007d7e93267e90b8ad65b2925bbe25ec35e9ef707c42357e86b4fd69b3508`
- Mixed: `0e5193c603078c02b9fc23bcb600a58f2b4e63ac42d5d3d64bac573de7c085c5`
- Final: `ec8c3c85f688e79704a585068434c7eb1b8e4d356585ed28ceb8fb43071fcc5a`

## Held-out evaluation

All controllers received identical map/task/heading sequences per category.
Policy actions were sampled from categorical heads. No checkpoint was selected
using these final evaluation maps. The earlier surface checkpoint validation
scored 32/32 surface, 32/32 bridge and 30/32 tunnel.

| Controller | Surface | Bridge | Tunnel | Support failures |
| --- | ---: | ---: | ---: | ---: |
| Final policy | 64/64 | 64/64 | 64/64 | 0 |
| Random | 8/64 | 0/64 | 16/64 | 0 |
| Direct goal | 62/64 | 64/64 | 58/64 | 0 |
| Privileged graph reference | 64/64 | 64/64 | 63/64 | 0 |

Policy mean decisions: 33.34 surface, 96.69 bridge, 22.56 tunnel (10 decisions/s).
Mean contacts: .438, .016, .422 respectively. All non-arrivals timed out. The
reference controller's tunnel timeout is retained; it is a diagnostic controller,
not an oracle guaranteed to succeed. The strong direct-goal baseline shows that
many tasks are simple. Sensor ablations and multiple training seeds remain future
work; the observed policy is not proven to require each supplied sensor.

A separate native CUDA inference smoke evaluation completed 253 episodes with
success/perf 1.0 in 32.63 process seconds. It requested 192 episodes but evaluated
parallel batches, so the terminal batch overshot. This is a distinct mixed
protocol, not the paired 192-episode CPU experiment above.

```sh
python3 scripts/eval_navigation.py \
  --model outputs/navigation/checkpoints/final-s173.bin \
  --output outputs/navigation/evaluations/new-final-check \
  --map-seed=10001 --maps=8 --episode-seed=9001 --sampling-seed=9002 \
  --episodes=64 --reference
```

## Validation and watching the result

Passed navigation ASan/UBSan contract/boundary checks, zero-allocation checks,
immutable-world checks, recurrent-reset adapter checks, sensor checks and the
native smoke suite. Native/WASM fixtures matched task outcomes: the reference
completed 48/48 map-72 tasks on both; libm steering differences caused 2051 vs
2055 decisions (under .2%), so trajectories are not claimed bit-identical.
The final native viewer ran bridge episodes successfully; actual Chrome WASM
playback and overlay controls were inspected. Native and browser checkpoint
loaders require explicit, valid weights; there is no untrained policy fallback.

```sh
./build.sh alienwars build/nav-viewer --cpu --rl
./build/nav-viewer outputs/navigation/checkpoints/final-s173.bin \
  --env.maps=1 --env.map_seed=10001 --env.episode_seed=9001 \
  --base.seed=9002 --env.task_kind=1

source .local/emsdk/emsdk_env.sh
AW_NAV_MODEL=outputs/navigation/checkpoints/final-s173.bin \
  ./build.sh alienwars --web --rl
python3 scripts/nav_dashboard.py --port=8767
```

Browser: `http://127.0.0.1:8767/viewer/?seed=10001&kind=1&episode_seed=9001&sampling_seed=9002`.
Dashboard: `http://127.0.0.1:8767/`. Both use Map Lab's monospace typography and
neutral palette. The viewer shows reward terms, goal, trajectory, action
probabilities, value, range/depth observations, and optional map diagnostics.

Two demo files are under ignored `outputs/navigation/demo/`, each **8 seconds**,
1920×1080, 30 fps H.264 MP4 without audio:

- `training-observatory.mp4`: actual surface-stage metrics, visibly labeled
  **RECORDED REPLAY · 55×**; it is not a live training capture.
- `nav-lab.mp4`: final checkpoint executing bridge navigation at normal speed,
  with sensor rays, trail, goal and a privileged reference overlay.

Actual Chrome frames were timestamp-resampled to 30 fps. Encoded frames, duration
and dimensions were checked. `manifest.json` holds clip hashes. Raw logs,
checkpoints, videos and generated browser builds stay ignored; no Pages release
was made from this experimental worktree.
