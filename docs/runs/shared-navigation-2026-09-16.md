# Shared-world navigation — 2026-09-16

Twelve physical vehicles now act in the same worlds while five independent
native PufferLib 5 PPO learners update their family policies. A* supplies global
routes; a low-level tracker and learned local overrides drive the actual vehicle
physics. The browser adds individual/group destinations and feeds actual
attachable sensor measurements to the policies. Raylib renders the shared C
simulation in WebAssembly. See [the complete contract](../SHARED_NAVIGATION.md).

The implementation and measurement pipeline are working. **The 95% full-mission
arrival target is not met.** The deployed set improves ground/boat arrivals over
the reference here, but underperforms it in the other families. These are
experimental navigation policies, not reliable indefinite fleet autonomy.

## Final held-out missions

Eight unseen maps (21001–21008), three ordered scenarios each, up to 240 simulated
seconds per mission. All three training seeds and both baselines receive matching
scenarios and equipment-dropout sequences. Every requested route in this test
was available. Rates include unavailable requests in their denominator.

| Family | Deployed PPO | Three-seed range | Route tracker | Random overrides | Contact decisions: PPO / tracker |
|---|---:|---:|---:|---:|---:|
| Ground | 56/72 (77.8%) | 77.8–83.3% | 49/72 | 41/72 | 20462 / 49452 |
| Surface boats | 61/72 (84.7%) | 84.7–90.3% | 57/72 | 29/72 | 16156 / 27981 |
| Quadcopters | 19/24 (79.2%) | 75.0–91.7% | 24/24 | 24/24 | 9208 / 0 |
| Fixed wings | 43/48 (89.6%) | 89.6–91.7% | 44/48 | 35/48 | 5 / 4 |
| Submarines | 56/72 (77.8%) | 77.8–86.1% | 60/72 | 6/72 | 9123 / 15050 |

Contact decisions count repeated blocked moves, not unique impacts. The JSON also
reports collision episodes and decisions with less than 0.01 potential progress;
that last measure includes deliberate waiting or reversing. Every controller,
including random overrides, has the same A* route and route tracker. This is a
residual-navigation comparison, not random raw motor control. Quadcopters'
strong reference/random results make their learned regression particularly clear.
A fixed-wing impact ends that mission; wing arrivals are fly-throughs.

## Training and selection

49,545,216 agent steps across seeds 373, 474 and 575:
2,359,296 clear-route, 4,718,592 opposing-traffic and 9,437,184 full-mission steps
per seed. Training maps are 301–308. All five policies learn during each rollout;
each has its own optimizer and each vehicle its own recurrent state. Networks
have 645 inputs, action heads 4/3/3/3, two recurrent layers of width 128, and
182,656 parameters per family (913,280 total). This is independent PPO, not a
centralized critic or a team coordination objective.

Trainer-reported time totals 1107.355 seconds
(18.46 minutes). Stage process time totals
1393.685 seconds, including preparation. CPU worlds
feed a single RTX 4090; Clang 18.1.3, CUDA 13.0.88, driver 580.173.02, bf16 learner
and fp32 exported weights. Other GPU work was preserved. Training source:
`4d1f2800063c92b943cdb38263e048f1ba538e4f`. Each stage's exact command, configuration and hashes
are retained; optimizer state resets when loading the preceding stage's weights.

Selection uses only maps 11001–11003. Within each training seed and family, choose
between traffic/full checkpoints by arrivals, then contacts, then steps. Evaluate
the assembled five-policy set together again; choose the best macro family
arrival rate. This selected seed **474** before final-test results were examined.
Its family stages are **1, 2, 2, 1, 2**. Seed 373 subsequently did better on several
final families; deployment was not switched using that test information.
Selected model hashes and per-family source runs are in
[policies.json](../../web/maplab/policies.json).

Preliminary runs are separate from the completed-curriculum totals: a vectorizer
policy-count assertion was fixed, a 36,864-step five-learner smoke and a clear-route
pilot validated training, and a v2 curriculum was stopped to fix overlapping
parking goals and endpoint stopping. The nine final v3 stages began again from
independent initializations. Raw preliminary logs are retained with the release.

## Validation and browser endurance

- Native ASan/UBSan and WASM: exact flight-primitive replay, tracker arrival,
  synchronized collision order, independent terminal/reset state, no step/reset
  allocation, valid hull spawns and separated parking goals.
- CUDA gather: every field and recurrent state checked against independently
  indexed sentinels across five families, unequal populations and two buffers.
- All 45 family-stage updates changed finite weights; all nine stages have finite
  metrics and exact five-family warm initialization. The ordinary single-policy
  path passed an 8,192-step native Minimal GPU regression.
- Native/WASM inference agrees across 500 recurrent decisions and 2,000 actions
  with the deployed checkpoint set. Sensor, vehicle and camera suites pass.
- 256 native/WASM terrain seeds and 48 diversity worlds pass, alongside mountain,
  tunnel, bridge, manifold/winding, occlusion and detail checks. Generator remains
  version 10 with unchanged terrain hashes.
- Real Chrome verifies group/individual destination commands, rejected goals,
  preserved pose/momentum, equipment versus overlays, outside-canvas release,
  follow/selection UI and all twelve loaded unit profiles.

Chrome endurance: 604.0 simulated seconds in 620.9 wall seconds; 0 runtime/GL errors, 2 stopped aircraft after impact, 6 other units moved less than one world unit during the last minute.

Browser endurance measures runtime stability separately from navigation quality.
The default seed 73, symmetric floors 6/6, mixed palette and tunnels are used.
No resets or safety-controller substitutions occur during the endurance run.
Lifetime contacts/arrivals and final per-unit states are in the accompanying JSON.

## Remaining limits and reproduction

Narrow passage right-of-way and persistent deadlocks remain unresolved. Some
routes are geometrically feasible but poorly tracked by learned local overrides.
The heading/rate-aware flight planner is bounded and not complete. Group commands
assign separate destinations without formation control or reservations. Sensor
localization is ideal; cosmetic props and RGB perception are not authoritative.
A mission-relative odometer reference keeps new browser goals consistent with
training resets while the visible lifetime odometer continues.

```sh
./build.sh alienwars_shared build/puffer-shared
python3 scripts/train_shared.py --prefix UNIQUE_RUN_NAME
python3 scripts/check_shared.py
python3 scripts/eval_shared.py --models policy=MODEL_DIRECTORY \
  --seed 21001 --maps 8 --baselines --out outputs/shared/reproduced-evaluation
python3 scripts/build_fleet_site.py
```

Train in a clean isolated GPU checkout. `MODEL_DIRECTORY` must contain all five
`mission-N.bin` files. Evaluation source is `6a9317e186072f09b3d4d2cff9176e4667424271`;
its mission-relative odometer field is zero in episodic evaluation and leaves
training observations unchanged. Final evaluated results were reproduced from
that committed source. The selected policies and raw records are release assets;
compiled browser artifacts are deliberately published under `main:/docs`.
