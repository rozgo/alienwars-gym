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

## Task contract v1

- One small ground scout per independent episode. No inter-unit dynamics.
- Two categorical action heads: throttle reverse/stop/forward (0/1/2), and yaw
  left/straight/right (0/1/2). All nine combinations are legal; malformed values
  are handled as stop/straight and counted separately.
- Fixed 10 Hz decisions with three 30 Hz motion/sensing substeps. Motion uses
  bounded acceleration and yaw rate, sampled body clearance and support from the
  shared tetrahedral solid. A blocked move stops translation and produces contact.
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

Contract and failure tests with ASan/UBSan; deterministic native/WASM traces;
headless timing excluding bank generation; finite rollout/training metrics;
held-out success, contacts, support failures, timeout and episode duration by
task category; random and goal-seeking baselines; checkpoint/config hashes;
native and browser playback. A 90% per-category held-out success rate is a
future release target, not a claim made by completing a training run.

Training visualization must show real samples and identify the run, backend,
checkpoint and evaluated seed set. Untrained/manual/baseline modes must be named.
Additional training seeds are required before claims of robustness.
