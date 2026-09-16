# Five-family local navigation — 2026-09-15

Five native PufferLib 5 PPO controllers were trained for the twelve-vehicle roster. A* supplies global route sections; vehicle physics enforces movement and medium constraints. Variants share family parameters and have separate recurrent state. Other traffic is kinematic: this is not joint multiagent learning.

Final evaluation used 128 scenarios per family on eight unseen maps (20001–20008), separate from training (201–208) and selection (10001–10004). All controllers received matching scenario sequences. Episodes last at most 50 simulated seconds.

| Family | PPO arrivals | Goal-seeking reference | Random | PPO / reference contact decisions |
|---|---:|---:|---:|---:|
| Ground | 95/128 (74.2%) | 91/128 | 0/128 | 441 / 17597 |
| Surface boats | 86/128 (67.2%) | 75/128 | 0/128 | 778 / 21518 |
| Quadcopters | 128/128 (100.0%) | 80/128 | 0/128 | 6 / 23674 |
| Fixed wings | 105/128 (82.0%) | 21/128 | 5/128 | 23 / 104 |
| Submarines | 96/128 (75.0%) | 65/128 | 0/128 | 738 / 24421 |

Contact decisions count repeated blocked simulation decisions, not unique collisions. Fixed-wing contact ends an episode. The reference turns toward the next waypoint without obstacle avoidance; it is a disclosed baseline, not an optimized local planner.

The runs used 59,768,832 agent steps, including the failed 1,048,576-step pilot. Trainer-reported time totals 20.1 minutes; map preparation, builds, transfers and evaluation are excluded. CPU worlds feed a single RTX 4090 native CUDA learner. Clang 18.1.3, CUDA 13.0.88, bf16 learner and fp32 checkpoint exports.

The initial ground pilot achieved 0/32 validation arrivals. Goal-bearing normalization and the revised PPO schedule produced learning; subsequent traffic and profile stages initialized from exact earlier weights with fresh optimizer/recurrent state. No run is claimed as an optimizer resume. See the JSON record and release archive for exact source snapshots, resolved configurations, metrics and hashes.

A 120-second complete-fleet probe on seed 73 moved all twelve units, but the hauler stalled and one transport collided. These controllers are experimental, not reliable indefinite autonomy. The browser exposes collision status and an explicit **Restart vehicles** control. It does not silently replace PPO with scripted steering.

The flight planner searches position/altitude, not a full heading lattice. Single-seed training, ideal localization/neighbor tracks, fixed controller sensor profiles, and missing cosmetic-prop collision remain limitations. The old Navigation Lab checkpoint and Observatory are separate historical experiments.

[Machine-readable results](local-navigation-2026-09-15.json) · [Local task contract](../LOCAL_NAVIGATION.md) · [Checkpoints and raw run records](https://github.com/rozgo/alienwars-gym/releases/tag/vehicles-2026-09-15)
