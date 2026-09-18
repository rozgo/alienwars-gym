# AlienWars navigation MVP

## Goal and delivery plan

Implement one sensor-guided scout reaching a nearby beacon in the existing
AlienWars world. The native PufferLib task, native policy viewer and WASM viewer
must run the same action-driven simulation. Map Lab remains the world inspector.

1. Define and test headless motion, observations, rewards and episode boundaries.
2. Connect the native `alienwars` environment and bounded training configuration.
3. Show terrain, goals, sensors, action attempts, trails and reward terms in the
   policy viewer; expose live PufferLib optimization and outcome statistics.
4. Benchmark, run a bounded single-GPU pilot, evaluate an explicit checkpoint
   against random and greedy controllers on separate seeds, then inspect playback.
5. Record exact commands, configuration, timings, checkpoint hashes and failures.

Measured MVP result: [September 15 run report](runs/2026-09-15-alienwars-navigation.md).

## Task contract v1

- One small ground scout per independent episode. No inter-unit dynamics.
- Two categorical action heads: throttle reverse/stop/forward (0/1/2), and yaw
  left/straight/right (0/1/2). All nine combinations are legal; malformed values
  are handled as stop/straight and counted separately.
- Fixed 10 Hz decisions with three 30 Hz motion/sensing substeps. Motion uses
  bounded acceleration and yaw rate, sampled body clearance and support from the
  shared tetrahedral solid. A blocked move stops translation and produces contact.
  Entering a submerged surface bed also produces contact; bridges and underground
  floors remain distinct from water beds. This is not a swimming/fluid simulator.
  Loss of floor support is a failure; no teleportation between stacked floors.
- Policy input is the existing 613-float sensor contract plus eight task floats:
  relative goal right/forward/height, goal distance, forward speed, yaw rate,
  previous contact, and remaining episode time. The goal vector uses disclosed
  ideal simulator localization. Exact pose, graph distances and reference paths
  are available for diagnostics, not appended to actor input.
- Ground LiDAR and forward/downward radial depth are enabled. RF/sonar are off.
  Sensor cadence and validity/age fields retain their contract. Cosmetic props
  do not participate in collision or sensing.
- Success requires arrival within 0.9 world units on the goal floor. Success +1,
  support/domain failure -1, timeout at 600 decisions. Time cost -0.001 per step,
  contact cost -0.01. Progress uses discounted potential shaping with gamma .99
  and a bounded negative graph-distance potential; terminal potential is zero.
- Generate an immutable map/task bank at initialization. Episode reset chooses a
  map/task and randomized heading, clears sensors/motion/history and writes the
  initial observations. No generation, file I/O or allocation in step/reset.
- Training and evaluation banks use separate explicit map-seed ranges and task
  RNG seeds. Feature cohorts include surface slopes, bridges and cave passages;
  optional features are sampled only where generation actually provides them.
- Start with local surface tasks, then bridge/tunnel tasks, then a mixed task
  distribution. Curriculum changes are explicit run settings, not evaluation
  adaptation. Reset signals must reset policy recurrent state exactly once.

## Evidence required

Contract and failure tests with ASan/UBSan; deterministic repeated native traces
and matching native/WASM task outcomes (small libm steering drift is allowed);
headless timing excluding bank generation; finite rollout/training metrics;
held-out success, contacts, support failures, timeout and episode duration by
task category; random and goal-seeking baselines; checkpoint/config hashes;
native and browser playback. A 90% per-category held-out success rate is a
future release target, not a claim made by completing a training run.

Training visualization must show real samples and identify the run, backend,
checkpoint and evaluated seed set. Untrained/manual/baseline modes must be named.
Additional training seeds are required before claims of robustness.

## Build and inspect

Run from the repository root. The default Map Lab builds remain available.

```sh
uv run scripts/check_navigation.py
./build.sh alienwars build/nav-viewer --cpu --rl
./build/nav-viewer --env.controller=4 --env.maps=1 --env.map_seed=72
# Explicit diagnostic controller, using the same collision/motion:
./build/nav-viewer --env.controller=3 --env.maps=1 --env.map_seed=72 --env.task_kind=1
```

WASD or arrows control a manual scout. Space pauses, N starts another episode,
H toggles reward distance, G walkable terrain, L sensor rays, R the privileged
reference route, C cutaway, and F camera following. Drag to orbit and scroll to
zoom. The browser also exposes these options as Map Lab-style controls.

World generation uses the existing defaults with `symmetry=0`; map seeds are
consecutive from `env.map_seed`. Each map contributes up to 48 cached tasks.
Mixed episodes sample uniformly from the available tasks, so the distribution
is **not balanced by terrain category**. A feature-specific bank fails at startup
if none of its maps supplies that feature; it does not alter the terrain.

## Native GPU training and live statistics

```sh
CUDA_HOME=/usr/local/cuda ./build.sh alienwars build/puffer-alienwars
./build/puffer-alienwars train \
  --base.run_id=nav-surface-unique \
  --base.checkpoint_dir=outputs/navigation/checkpoints \
  --env.task_kind=0 --env.maps=8 \
  --train.total_timesteps=8388608 --train.learning_rate=0.015 \
  --train.min_lr_ratio=0.2 --train.ent_coef=0.001
uv run scripts/nav_dashboard.py --port=8767
```

Open `http://127.0.0.1:8767`. The dashboard reads actual streamed
`logs/alienwars/RUN.jsonl` records, refreshes every two seconds, and shows missing
metrics as missing. It plots arrival/outcome rates, contacts, PPO losses,
entropy, KL/clipping, SPS and whole-device GPU load. Early arrival rates only
include episodes that have already ended; wait for timeout episodes before
interpreting them. The existing final `.ini` logs still work with PufferLib's
Constellation dashboard. `.config.ini` records the resolved starting settings.
For a demo, `?run=RUN.jsonl&replay=55` replays actual saved metrics at 55×
with a visible RECORDED REPLAY label; the default dashboard remains live.

On a GPU machine, serve the dashboard there and forward its loopback port through
the user's SSH alias. Alternatively, copy the JSONL/config files to the local
checkout's `logs/alienwars/`. The browser does not connect directly to SSH.

The CPU world is shared across vector environments; map construction, ray bounds,
small-body graph validation and per-goal distance fields run once at startup.
CUDA handles the native PufferNet/PPO learner. Rollout throughput is not CUDA
environment throughput, and it excludes map-bank preparation. Training requires
`env.controller=0`; scripted baselines are restricted to the separate viewer.
Keep `train.gamma=0.99` to match the task's potential shaping contract.

For a curriculum stage, pass `--base.load_model_path=EXACT_CHECKPOINT.bin` and a
new run ID. Native training loads those weights before the first rollout and
saves `initial.bin` for hash verification. Optimizer state, recurrence, RNG and
step counters start fresh. This is weight initialization, not training resume.

## Evaluate and view a checkpoint

Supply the exact file and its matching architecture; no `latest` or untrained
fallback is used. Flat weights are not optimizer-resume snapshots.

```sh
./build/nav-viewer outputs/navigation/checkpoints/selected.bin \
  --env.maps=1 --env.map_seed=10001 --env.episode_seed=9001 --base.seed=9002
uv run scripts/eval_navigation.py \
  --model outputs/navigation/checkpoints/selected.bin \
  --output outputs/navigation/evaluations/unique-run \
  --map-seed=10001 --maps=8 --episode-seed=9001 --sampling-seed=9002 \
  --episodes=64 --reference
```

The evaluator runs policy, random and direct-goal controllers on identical
episode lists for each terrain category. Optional reference results use the
privileged graph and must not be mistaken for a learned controller. It records
per-episode world hashes, start/goal IDs, outcomes, contacts, steps and aggregate
results with checkpoint/artifact hashes. The dashboard includes completed
evaluations beneath `outputs/navigation/evaluations/`.

Policy actions are sampled from PufferNet's categorical heads by default.
`--argmax` explicitly selects deterministic maximum-logit actions. CPU `rand()`
is seeded by `base.seed`; its sequence may differ between C libraries. Episode
selection has a separate portable per-environment RNG. Evaluation map seeds,
episode seeds and sampling seeds must be disclosed, and validation/selection
maps must remain separate from final evaluation maps.

For browser policy playback:

```sh
source .local/emsdk/emsdk_env.sh
AW_NAV_MODEL=outputs/navigation/checkpoints/selected.bin \
  ./build.sh alienwars --web --rl
uv run scripts/nav_dashboard.py --port=8767
```

Open `/viewer/?seed=10001&kind=-1&episode_seed=9001&sampling_seed=9002` on that
server. `kind=-1/0/1/2` selects mixed/surface/bridge/tunnel tasks;
`controller=0/1/2/3/4` selects checkpoint/random/greedy/reference/manual;
`argmax=1` selects maximum-logit inference. Checkboxes preserve overlay settings
in the URL. Missing feature cohorts or invalid checkpoints fail visibly.
Without `AW_NAV_MODEL`, the web build identifies itself as manual; it does not
pretend to contain a policy. `AW_NAV_HIDDEN` and `AW_NAV_LAYERS` must match any
nondefault model architecture. `manifest.json` identifies source and model hashes.

The action loop and cached sensor readings are independent of overlay visibility.
The web sidebar uses Map Lab's font stack, palette, borders and controls. The
policy receives sensors and a localized goal; proving that it relies on a
particular sensor requires a separate ablation study.
