# Fleet navigation reliability — September 18, 2026

**The implementation and three-seed training are complete; the reliability gate failed.**
Do not replace the public checkpoints with this candidate. The 95% collision-free
arrival target is not met across all five families, and continuous patrols still
expose persistent blockage. The public Map Lab retains its September 16 policies.
[Recorded dashboard](https://rozgo.github.io/alienwars-gym/training/reliability.html) ·
[Machine-readable results](navigation-reliability-2026-09-18.json) · [Checkpoints and raw records](https://github.com/rozgo/alienwars-gym/releases/tag/navigation-reliability-2026-09-18).

## What changed

Project Python tools now use `uv run`, a locked Python 3.12.12 environment and
stdlib-only core dependencies. Simulation, Raylib rendering and native PufferLib 5
PPO remain C/CUDA; Python orchestrates builds, experiments and reports.

Contract 3 keeps A* global routes and trains gentler local steering biases.
Measured RF/range history estimates approaching traffic. Deterministic assistance
adds actual-actuator trajectory and braking checks, keep-right/yield conventions,
bounded local detours and viewer replanning without teleports. Boat clearance
uses its physical hull rather than a conservative route-grid wall. Confinement
detection catches tiny oscillations that previously concealed stuck vehicles.
These mechanisms remain separate from learned PPO; see the
[implementation contract](../NAVIGATION_RELIABILITY.md).

Twelve learner vehicles train together with up to four frozen historical actors
in fixed Flecs slots. Frozen actors do not enter learner PPO rows. Each vehicle
has independent recurrent state; each of five movement families has its own
optimizer. Competitive faction self-play was considered and deferred because
combat actions, rewards and faction outcomes are not yet implemented.

## Final unseen-world evaluation

Generator 13, maps 41001–41016, eight preparations per map, up to 240 simulated
seconds per mission. Final evaluation source: `a4fed85fc42374802beb3d396a861bf243f45e38`.
The selected five-family set is **seed 575**, frozen at
`2026-09-18T17:00:30.992615+00:00` using selection maps 32001–32008 only.
Selection maximizes the worst-family collision-free requested arrival rate,
then the macro average. No family mixing or changes using final results.

| Family | Selected PPO + recovery | Three-seed range | Tracker + recovery | Prior release |
|---|---|---|---|---|
| Ground | 239/412 (58.0%) | 49.6–58.0% | 292/420 (69.5%) | 190/415 (45.8%) |
| Surface boats | 342/418 (81.8%) | 81.8–84.7% | 378/427 (88.5%) | 341/429 (79.5%) |
| Quadcopters | 141/144 (97.9%) | 97.9–97.9% | 141/143 (98.6%) | 117/141 (83.0%) |
| Fixed wings | 280/287 (97.6%) | 97.6–97.9% | 281/287 (97.9%) | 243/281 (86.5%) |
| Submarines | 389/431 (90.3%) | 90.3–94.0% | 404/431 (93.7%) | 301/430 (70.0%) |

The selected combined system improves on the prior release in all five families,
but **the tracker with recovery outperforms the candidate in every family**.
These results do not establish an advantage from adding the learned PPO residuals
to the new deterministic controller.

A clean arrival means the requested mission completed without contact. Unavailable
routes stay in the denominator. The prior release uses its exact contract-2
weights and original local action interpretation in the same current physical
worlds. The reference uses the new deterministic tracker and recovery without
PPO residuals. This matched comparison differs from historical generator-10 scores.
Every controller receives the same map/scenario equipment sequence. Serial and
scenario-sharded evaluation records were checked for exact agreement.

| Family | Initial clean arrivals | Return clean arrivals | Unavailable requests | Contact episodes | Terrain / unit contact decisions | Timeouts / arrivals after confinement |
|---|---|---|---|---|---|---|
| Ground | 216/384 | 23/28 | 13 | 154 | 144 / 4431 | 130 / 22 |
| Surface boats | 308/384 | 34/34 | 6 | 35 | 2 / 45 | 40 / 9 |
| Quadcopters | 125/128 | 16/16 | 0 | 4 | 2 / 1835 | 1 / 1 |
| Fixed wings | 251/256 | 29/31 | 1 | 6 | 1 / 5 | 0 / 0 |
| Submarines | 355/384 | 34/47 | 0 | 25 | 15 / 11 | 14 / 14 |

Initial missions have a fixed requested denominator. Return missions are requested
only after a first arrival in the repeated-destination scenario, so their counts
are conditional on controller behavior. Contact decisions include repeated rejected
moves; contact episodes merge consecutive contact. Confinement means staying within
0.35 units for ten seconds, with five seconds extra grace while explicitly yielding.
It is not proof of permanent mutual deadlock. The JSON also records yielding,
interventions, local replans and elapsed decision counts; successful waiting must
not be misreported as a failed mission.

## Assistance ablation and unfamiliar traffic

| Family | Selected with assistance | Same weights, assistance off | Unfamiliar traffic + worlds |
|---|---|---|---|
| Ground | 239/412 (58.0%) | 187/407 (45.9%) | 48/101 (47.5%) |
| Surface boats | 342/418 (81.8%) | 311/418 (74.4%) | 84/104 (80.8%) |
| Quadcopters | 141/144 (97.9%) | 135/144 (93.8%) | 36/36 (100.0%) |
| Fixed wings | 280/287 (97.6%) | 273/287 (95.1%) | 69/71 (97.2%) |
| Submarines | 389/431 (90.3%) | 339/427 (79.4%) | 90/106 (84.9%) |

The ablation uses the same selected weights and final worlds with deterministic
local assistance disabled; this measures dependence, not a separately trained
unassisted policy. The last column uses separate maps 42001–42004 and historical
seed-575 traffic, absent from the training pool. Different worlds and traffic are
both changed in this stress test; it does not isolate a causal traffic effect.

## Training provenance

49,545,216 retained learner agent steps across seeds 373, 474 and 575, using
32 worlds (301–332). Per seed: 2,359,296 clear-route, 4,718,592 traffic and
9,437,184 full-mission steps. Eight preparations target corridor travel, opposing
traffic, overtaking, crossings, queues, tunnel approaches, long journeys and
return trips. They are intentions, not guarantees of an encounter on every map.

The RTX 4090 GPU host ran native five-learner PPO with CPU environments, CUDA
13.0.88, driver 580.173.02, Clang 18.1.3 and GCC 13.3.0. Learner precision is bf16;
exports are fp32. Each family has 645 observations, categorical heads `[4,3,3,3]`,
two width-128 recurrent layers and 182,656 parameters. Final evaluations and Chrome
ran on the Mac. Other GPU jobs were preserved.

Summed trainer time: 7153.697 seconds; summed stage-process
time: 10477.654 seconds. These sum concurrent runs and must
not be interpreted as elapsed wall time or a clean throughput benchmark.

Stages 0/1 used `67eeed8f799263e45e5becdfd076f1e03b2ca889`; stage 2 used
`5376f1cfce657026755c5ef821f12d023264f4e9`. The first stage-2 attempts were
intentionally stopped before useful rollout for measured sensor-fusion and braking
fixes. The restarted stages loaded exact stage-1 weights with fresh optimizers.
All 45 retained family-stage updates changed finite weights and preserved exact
warm-initialization hashes. Smoke runs and interrupted attempts are excluded from
the 49.5-million-step total and retained in the record archive.

Additional deterministic clearance/recovery refinements followed diagnostic and
endurance failures, before checkpoint selection and final-world testing. The
candidate was evaluated with that frozen runtime (`80ec5716` core; later commits
add telemetry/reporting). A two-epoch, five-family GPU smoke at `88cf1da9` confirmed
finite metrics and updates under the final runtime. Those 36,864 additional smoke
steps did not produce the candidate checkpoints. Improved assistance must not be
presented as extra learned behavior.

## Validation and endurance

Native ASan/UBSan and WebAssembly checks passed for measured-track expiry, no hidden
neighbor-pose control, physical boat draft, head-on passage, parked-obstacle detours,
range-only fallback, escape from conservative sensor margins, aircraft crossings,
independent terminals, repeated-goal observations and zero step/reset allocations.
The historical Flecs traces still match exactly. CUDA joint gather sentinels and
all three candidate sets' native/WASM recurrent inference parity passed. Runtime
checks establish implementation properties, not the 95% navigation objective.

Native endurance ran seed 73 for 6,000 decisions (600 simulated seconds), all twelve
units, no resets. Finite state and physical nonoverlap passed. Navigation failed:

| Unit slot / family | Arrivals | Contact decisions | Current confinement ticks | Global retries | Failed |
|---|---|---|---|---|---|
| 0 / Ground | 4 | 1 | 2 | 0 | 0 |
| 1 / Ground | 123 | 0 | 25 | 0 | 0 |
| 2 / Ground | 4 | 2 | 3 | 0 | 0 |
| 3 / Surface boats | 6 | 0 | 2 | 0 | 0 |
| 4 / Surface boats | 5 | 0 | 1 | 0 | 0 |
| 5 / Surface boats | 5 | 0 | 1 | 0 | 0 |
| 6 / Quadcopters | 3 | 1 | 3572 | 3 | 0 |
| 7 / Fixed wings | 4 | 1 | 0 | 0 | 1 |
| 8 / Fixed wings | 30 | 0 | 1 | 0 | 0 |
| 9 / Submarines | 2 | 550 | 2 | 0 | 0 |
| 10 / Submarines | 2 | 0 | 5 | 0 | 0 |
| 11 / Submarines | 0 | 549 | 19 | 3 | 0 |

The quadcopter was confined for 357.2 seconds at the final snapshot, the recon wing
had failed after contact, and the heavy submarine had not completed any mission.
The two submarine slots recorded 549 mutual-contact decisions each. High arrival
counts on the rover's short patrol are not comparable to long ocean missions.

**Real Chrome:** Chrome ran 611.5 simulated seconds in 797.2 wall seconds, without resets or controller/equipment changes. No console runtime/GL errors were observed. Navigation failed: one fixed wing stopped after contact, the quadcopter remained confined for 106.1 seconds at the final snapshot, and two submarines completed no missions and recorded 4,235 unit-contact decisions each.
The complete browser snapshots, timestamps and error records are in the JSON.
Browser and native results are separate trajectories; do not assume exact cross-
platform replay or infer fleet reliability from the absence of a runtime error.

## Deployment decision and next experiment

Keep the current public policies. Publish this implementation, evaluation and
candidate weights as an experimental prerelease for reproducibility, without
changing the Map Lab checkpoint manifest or its compiled runtime.

The next navigation experiment should target blocked-ground maneuvers, close
submarine encounters and wing/quad right-of-way. Use newly assigned diagnosis and
selection seeds, retain these final worlds only as a published regression set,
and train against the stabilized assistance before another untouched final test.
Measure successful recovery and mission throughput alongside contact reduction;
refusing to move must never count as success. More optimization steps alone have
not established reliable fleet navigation here.

## Reproduce

In a clean checkout, use the candidate release's five `mission-N.bin` files and
`contract.json`; the frozen actor directory must match
`config/alienwars_shared_frozen.json`.

```sh
uv sync --locked
gh release download shared-navigation-2026-09-16 --pattern 'mission-*.bin' --dir FROZEN_DIRECTORY
gh release download navigation-reliability-2026-09-18 --pattern 'mission-*.bin' --pattern 'contract.json' --dir CANDIDATE_DIRECTORY
AW_SHARED_FROZEN_DIR=FROZEN_DIRECTORY uv run scripts/eval_shared.py \
  --models candidate=CANDIDATE_DIRECTORY --contract 3 \
  --seed 41001 --maps 16 --jobs 4 --reference --out outputs/reliability/reproduced
AW_MISSION_MODELS=CANDIDATE_DIRECTORY ./build.sh alienwars --web
uv run python -m http.server 8790 --bind 127.0.0.1 --directory build/web/alienwars
```

The report JSON contains checkpoint hashes, all retained stage commands, source
commits, selection results, final summaries, ablation and traffic-pool hashes.
The release archive contains full records and checksums. Contract 2 and 3 have
identical tensor dimensions but different semantics; never select a contract by
file size alone. Open `/maplab.html?seed=73` on the local server for the candidate.
