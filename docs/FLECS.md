# Flecs runtime

The current Map Lab fleet and `alienwars_shared` training task store live unit
state in **Flecs 4.1.6 through its C API**. Raylib and PufferLib use the same
renderer-independent simulation. The biological cultivation exploration is saved
in [BIOLOGICAL_WARFARE.md](BIOLOGICAL_WARFARE.md); this port changes storage and
lifecycle, not the navigation task or its trained policy architecture.

## Ownership and execution

Each `AwMissionWorld` owns an independent `ecs_world_t`. Sixteen stable slots are
created at initialization; twelve are used by the current fleet. Every slot has
four component columns:

| Component | Authoritative state |
| --- | --- |
| `AwMission` | Physical vehicle pose/velocity, route progress, mission outcomes and the 645-float policy observation |
| `AwPerception` | Equipment, sensor samples, physical hull, pose history and odometry |
| `AwActive` | Participation in the shared simulation |
| `AwPaused` | Viewer pause state used by synchronous movement |

`missions.h` binds these columns with a Flecs query. The existing numeric kernels
operate directly on the borrowed columns; no parallel copy of the entity state
is synchronized to an otherwise decorative ECS. All IDs are world-local. The
stable actor slot is explicitly mapped to its Flecs entity, so family assignment
and observation ordering do not depend on numeric ECS handles.

The component set and row order remain fixed until world destruction. Debug
assertions check slot mapping on every bind. Reset clears component values and
sensor caches while retaining storage, IDs and queries. Close releases the query
and world. These owning structs must not be shallow-copied or cleared with a raw
`memset`; use their init/reset/close functions. Adding dynamic component layouts
or births later requires an explicit revision of these pointer-lifetime rules.

Simulation phases retain the current order: infer commands, resolve three 30 Hz
movement substeps synchronously, update mission outcomes, then sample sensors and
pack observations. Decisions remain 10 Hz. Flecs does not start a worker pool or
run a separate wall-clock scheduler. PufferLib continues parallelizing independent
environments; the browser advances the same simulation from its frame loop.

Terrain, density fields, route banks, planners and spatial queries retain their
specialized C structures. Rendering and interpolation remain presentation work.
This is not a conversion of every terrain tile or visual prop into an ECS entity.

## Sensing and historical tasks

`AwSensors` now borrows a contiguous `AwSensorUnit` array from its owner. The live
fleet supplies the `AwPerception` column. Standalone sensor fixtures supply an
explicit array, and the historical single-scout task owns a one-unit array.
Copies of a standalone owner must rebind that pointer to their own copied array.
The sensing equations, cadence and fixed 613-float output are unchanged.

The old navigation/local-navigation experiments retain their historical control
models and task contracts. They are not relabeled as Flecs-trained experiments.

## PufferLib boundary

The public `shared_api.h` C ABI, agent ordering, rewards, terminals, observations
and actions remain unchanged: twelve agents, five family policies, 645 inputs,
four categorical heads of sizes 4/3/3/3, and independent recurrence per unit.
Existing checkpoint files keep their original hashes and architectures.

Native CUDA training links Flecs and the environment as C object files. PPO,
Muon, rollout gathering and GPU policy execution retain their existing kernels.
Flecs is a CPU environment dependency, not a CUDA environment or learning
algorithm. Future birth/death handling and competitive self-play still need the
[documented RL work](BIOLOGICAL_WARFARE.md#proposed-data-architecture).

## Dependency and memory policy

The unmodified upstream distribution and MIT license are in `vendor/flecs`.
`version.json` records its exact commit and file hashes. Compile through
`ocean/alienwars/flecs_runtime.c` with the shared configuration header.

The training build selects core ECS plus the platform OS API, with `FLECS_LOW_FOOTPRINT`
for many small environments. Scripting, REST services, reflection and rendering
addons are not part of the rollout. Queries and all component storage are created
before stepping. No ECS structural changes occur during current step/reset calls.

The lifecycle test overrides Flecs allocation callbacks to count allocations,
measure retained bytes and verify complete teardown across 24 simultaneous worlds.
The adapter test also checks both application allocations and Flecs's OS counters.
Keep memory and throughput measurements alongside behavioral checks when changing
the configuration, capacities or component organization.

## Live browser Explorer

Open **Flecs Explorer** in the Map Lab header, or use
[Map Lab with Explorer](https://rozgo.github.io/alienwars-gym/maplab/?seed=73&explorer=1).
The embedded official Flecs Explorer reads the same running WebAssembly world.
Expand `Fleet`, select a unit, and inspect `AwMission` and `AwPerception` for
pose, velocity, mission progress, PPO inputs, odometry and attached sensor modules.
The Queries view accepts expressions such as `AwMission, AwActive`.
There are sixteen fixed slots, twelve active units and four reserved slots.

This viewer defines `AW_FLECS_EXPLORER`, enabling reflection, documentation,
query parsing and the REST dispatcher. Requests run directly in process; there
is no listening socket or external server. Writes are rejected in C to preserve
fixed entity topology. Use Map Lab controls to command or reconfigure units.
Closing the webview stops its polling. Rebuilding a map reconnects it to the
new world. Pointer-owned routes and large sensor beam buffers are not reflected.

These inspection addons are **absent from training**. The approximately 107 KiB
per-world overhead measured below describes the minimal training runtime; the
viewer has additional inspection metadata and query overhead. ECS storage and
queries are real, while fixed-step numeric kernels still drive simulation rather
than a Flecs system scheduler. Explorer's system timing views are not exposed.

The official frontend is pinned in `vendor/flecs-explorer/version.json`, with
unmodified selected assets in `frontend.tar.gz` and its MIT license.
`scripts/build_explorer.py` applies the read-only UI adaptations and packages
`docs/explorer/`. `web/explorer/alienwars_live.js` bridges its existing WASM
transport to Map Lab. No separate demo ECS world is created.

## Validation

```sh
python3 scripts/check_explorer.py
python3 scripts/check_flecs.py
python3 scripts/check_shared.py
python3 scripts/check_sensors.py
python3 scripts/check_navigation.py
python3 scripts/check_vehicles.py
./build.sh alienwars build/maplab --cpu --debug
python3 scripts/build_fleet_site.py
```

`check_flecs.py` archives the pre-port source from commit `179cbf02` into ignored
build storage and compiles the same behavioral trace against both versions. It
compares 1,200 world steps, repeated resets, all observations, movement state,
rewards and terminal events. It also records three timing samples per version
and backend. Native and WASM each match their respective pre-port trace; their
math libraries already produce different low-order floating-point values, so this
does not claim bit-identical native/WASM floating-point execution.

Ownership, world isolation, stable slots, allocation-free step/reset and complete
teardown are checked under native sanitizers and WASM. Policy inference parity,
equipment controls, command rejection, pose preservation and renderer behavior
remain part of the existing contract checks and browser acceptance.

Build metadata includes the Flecs revision and hashes alongside Raylib, compiler
and source provenance. GPU validation must include a bounded five-family update,
finite metrics/weights, unchanged-checkpoint loading, and the flag-zero trainer
path before publishing a runtime release.

The [initial port report](runs/flecs-port-2026-09-17.md) records the completed
native/WASM comparisons, Linux/CUDA smoke runs, browser checks and memory cost.
