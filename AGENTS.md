# Working on AlienWars Gym

## Scope and source of truth

This is a source-based PufferLib 5.0 project. Start with `README.md`,
`docs/DEVELOPMENT.md`, and `docs/UPSTREAM.md`. Check `git status` before edits and
preserve unrelated work. Follow explicit user instructions over this guide.
AlienWars has a procedural 3D Map Lab with twelve physical ground, naval, air
and submarine units. `alienwars_shared` trains five family policies simultaneously in shared worlds,
with A* global routes and actual attachable-sensor inputs; see
`docs/SHARED_NAVIGATION.md`. `alienwars_local` is the historical v1 experiment. A separate historical
navigation MVP trains a small scout through the native PufferLib interface. Combat is not implemented. See `docs/NAVIGATION_RL.md`.
Do not describe
scripted patrols as trained policies. The Breakout bootstrap experiment was
retired on September 15, 2026; do not restore its demo or project tooling.

User direction, September 17, 2026: humans do not exist in AlienWars. All new
military craft, equipment and infrastructure are grown biological organisms or
tissues. Each unit is itself the biological entity: no pilots, crew, riders,
occupants or separate creature/vehicle pairing. Use unit/entity, body and organ
language for new content. Economy follows habitat preparation, seeding, tending, maturation,
collection, sustainment and bounded recycling. Preserve meaningful mass, recoil,
metabolism and ammunition tradeoffs. See [the biological warfare direction](docs/BIOLOGICAL_WARFARE.md)
and [the proposed next slice](docs/CULTIVATION_SLICE.md). Current Map Lab unit models are biological; historical viewers may retain development stand-ins. Species names and detailed traits remain open.
These are design documents, not evidence of implemented combat or cultivation.

The current fleet stores mission, perception, active and pause state in Flecs
through the C API; see [FLECS.md](docs/FLECS.md). Entity columns have fixed topology
for the current policy contract. Use `aw_mission_world_init/reset/close`; never
shallow-copy or memset a live owning world. `AwSensors` borrows its unit buffer.
Compile the shared `flecs_runtime.c` in every fleet consumer, including CUDA
training, standalone checks and capture/browser builds. Retain allocation guards,
the pre-port traces and exact checkpoint compatibility during changes.
`AW_FLECS_EXPLORER` enables viewer-only reflection and in-process inspection; do
not enable it in training. Keep Explorer read-only and retain fixed slot order.
`docs/explorer/` is a deliberate Pages artifact built by `scripts/build_explorer.py`.

The public Navigation Lab packages the evaluated checkpoint selected by
`web/navigation/release.json`. Build with `scripts/build_navigation_site.py`.
`docs/navigation/` (including the small policy data bundle) and `docs/training/`
(recorded metrics and evaluation summaries) are deliberate Pages artifacts.
Keep raw checkpoints, logs and video masters in ignored directories or Release
assets. The static Observatory must identify recorded data and preserve numeric
metrics; local live training still uses `scripts/nav_dashboard.py`.

User direction, September 14, 2026: the repository is public and the first
browser release uses the existing **Raylib web path**, published through
GitHub Pages from **`main:/docs`**. See `docs/WEB.md`. Map Lab is the site entry point;
`docs/index.html` redirects to `maplab/` while retaining query and fragment.
Three.js is not part of this release. Browser simulation uses WASM; CUDA
training stays on the GPU machine.

Use the native C/CUDA API in this checkout. Do not add a legacy PufferLib 2/3
Python package, Gymnasium wrapper, PyTorch trainer, or global Python environment
as the default training path. Python here is optional standard-library tooling.
The website occasionally trails the source: there is no `binding.c` in the
current minimal template. Inspect headers before following an older tutorial.

## Repository map

- `ocean/<env>/<env>.h`: CPU environment and observation/action metadata.
- `ocean/<env>/<env>.cu`: optional CUDA environment or custom encoder; read it
  before assuming which it is. `--cu` selects a CUDA environment backend.
- `src/pufferenv.h`: `Env`, `Agent`, logging, and `puf_*` interface.
- `ocean/minimal/`: commented multiagent example with a custom encoder.
- `config/default.ini` then `config/<env>.ini`: merged runtime configuration;
  command-line `--section.key=value` overrides both.
- `src/pufferl.cu`, `src/algo.cu`: native trainer and learning kernels.
- `src/puffercpu.c`: standalone CPU inference and viewer.
- `scripts/`: Map Lab builds, native smoke checks and native/WASM validation.
- `outputs/`, `build/`, `checkpoints/`, `logs/`: ignored generated artifacts.
- `docs/runs/`: concise checked-in measurements and artifact hashes.
- `ocean/alienwars/map.h`, `layout.h`: deterministic global planning, WFC and
  navigation; independent of rendering.
- `ocean/alienwars/mountain_layout.h`, `mountain.h`, `natural_routes.h`, `traversal.h`: regional
  route WFC, graded excavation sockets and sampled walkable floor spans.
- `ocean/alienwars/render.h`, `props.h`, `shaders.h`, `alienwars.c`: Raylib
  Map Lab rendering, cosmetic meshes, materials and viewer.
- `web/maplab/shell.html`, `docs/maplab/`: Map Lab source and compiled Pages output.
- `docs/index.html`: authored site-entry redirect to Map Lab.
- `docs/maplab/index.html`, `maplab.js`, `maplab.wasm`, `maplab.data`, `build.json`: deliberate
  compiled Pages artifacts; regenerate with `scripts/build_maplab.sh`, never hand-edit.

Authored Map Lab meshes/materials live in `resources/alienwars/art/`; see
`docs/ART_PIPELINE.md` for sources, licenses and regeneration. These compact
assets are deliberate Git/Pages artifacts. Keep raw source downloads and editable
Blender outputs ignored. Preserve +Z-forward mesh orientation and unit envelopes;
asset articulation must follow actual body motion and remain viewer-only.

## Environment implementation

1. Specify observations, legal actions, reward terms, terminal conditions,
   reset behavior, simulation timestep and measurable success before tuning.
2. Start with a CPU environment using the existing header interface. Match
   `obs_t`, `OBS_SIZE`, `NUM_ATNS`, `ACT_SIZES`, agent count and buffer strides.
   Actions/rewards/terminals use float pointers in `Agent`.
3. Implement `puf_init`, `puf_reset`, `puf_step`, `puf_render`, `puf_close`, and
   `puf_log`. `Log` contains float fields with `n` last. Keep required `Env`
   fields consistent with `src/pufferenv.h` and the vectorizer.
4. Write all observations each step or clear sparse buffers first. Clear rewards
   and terminals every step. Reset internally and emit a terminal exactly when
   intended; test episode boundaries and recurrent-state resets explicitly.
5. Keep observation/reward scales roughly order one. Inspect real data for
   nonfinite values, invalid actions, stale flags and out-of-bounds access.
   Diagnose memory corruption before tuning away early NaNs.
6. Allocate stable contiguous buffers at initialization. Avoid allocations,
   file/network I/O and rendering in the step loop. Use per-environment RNG;
   seed it explicitly. Stagger fixed-length environments where appropriate.
7. Establish a correct CPU baseline before optimizing or implementing CUDA.
   CPU and GPU sources are separate: verify behavior with matched configuration
   and seeds; matching weights alone does not prove backend equivalence.

## Build and validation

Run from the repository root (configuration/resources use relative paths).

```sh
./scripts/check.sh
./build.sh alienwars build/maplab --cpu --debug # terrain viewer, not training
./scripts/build_maplab.sh
python3 scripts/check_maplab.py                # native/WASM parity + navigation
```

Use `--cpu --debug` with ASan/UBSan for environment development on Linux and
macOS. Run focused contract/boundary tests when environment logic changes, then
a bounded headless rollout, short GPU training with finite losses, explicit
checkpoint evaluation and the real viewer. Documentation-only edits need diff
checks, not expensive training. Existing `tests/` includes historical Python
tests; do not assume blanket `pytest` validates this native branch.

Check actions, reset/terminal behavior and rewards against the declared task,
including poor-but-valid and failure trajectories. A finished training process
is not proof of learning. Compare a disclosed baseline with held-out evaluation
and keep training, selection and evaluation seeds distinct. Use several training
seeds before claiming robustness. Report failed runs and limits honestly.

## Training and reproducibility

- User direction, September 17, 2026: the long-term training objective is actual
  warfare between alien factions, with self-play as an intended capability.
  At the next RL iteration, explicitly assess whether to implement self-play
  in that pass; record the decision, rationale and proposed evaluation in the
  iteration plan. Consider historical traffic policies for navigation and
  opponent pools for faction warfare. See [the self-play direction](docs/SHARED_NAVIGATION.md#self-play-and-faction-warfare).
  Current joint navigation training is not competitive faction self-play.
- Training requires an NVIDIA GPU and development toolkit (NVCC, cuBLAS, NCCL,
  OpenMP, ccache and graphics link libraries). macOS supports CPU inference and
  rendering, not native training. Do not infer CUDA availability from Python.
- Inspect GPU utilization/processes before launching. Use an isolated checkout
  for this project; preserve other jobs. Start with one GPU and a bounded run;
  use Protein sweeps only after environment correctness and baseline learning.
- Record source commit/dirty diff, native compiler versions, GPU, backend,
  precision, seeds, resolved config, commands, actual steps and timings.
  Separate build/process time from trainer-reported time.
- Use unique output directories, explicit model paths and checkpoint SHA-256.
  Avoid `latest` for reproducible reports. Flat `.bin` policies need their
  matching configuration/architecture and are not optimizer-resume snapshots.
- Keep long-running source checkouts fixed. Move source by normal Git commits
  and fast-forward pulls; do not force-push or reset another machine's work.

## Credentials, artifacts and handoff

Keep SSH keys, tokens, host addresses and machine-specific paths outside Git.
Use an SSH alias in the user's SSH config or ignored `.local/` notes. Publish
only within the user's authorization; preserve the upstream MIT license and
asset attribution. Keep upstream as a separate remote and update it deliberately.

Keep raw logs, native binaries, downloaded libraries and checkpoints ignored.
Commit concise reports with measurements, settings and hashes. Selected durable
binary artifacts should use release assets or an explicit LFS policy, not an
accidental bulk `git add .`. Review `git diff --check` and staged changes.
Handoff should include what ran, what was measured, remaining limitations and
an exact command to watch the trained policy.

For Pages releases, commit the authored source first, rebuild, run the WASM
check and inspect the real browser demo before committing the generated files.
Verify public deployment status and artifact hashes after pushing. The small
compiled Pages artifacts are an explicit exception to the general rule keeping
generated binaries out of Git; credentials, SDKs, raw logs and other build
products remain ignored. Do not publish an untrained fallback as a trained demo.

For Map Lab changes, follow `docs/MAPLAB.md`. Keep the strategic route plan,
road/tunnel/material WFC constraints, height surfaces, navigation and renderer coherent. Record a
generator-version change when seed outputs change. Validate rotational symmetry, cardinal edge
connections, all road lanes through floor 10, tunnel clearance and portals, resources and paths; test contradictions and
disconnected maps. Keep cosmetic props separate from authoritative collision.
The assembly view replays recorded cell resolution; do not call it a live solver
or claim it visualizes retries. New global-layout, combat or RL behavior needs
an explicit environment contract and appropriate validation.

Natural terrain must retain connected 3D tile geometry: shared corner/edge
profiles, shaped cliff transitions, slopes and continuous shorelines. Do not
replace it with independent flat-topped columns or use material-only WFC as a
substitute for geometric tile constraints. Compare real browser renders with the
accepted Map Lab v1 contour quality (source/artifacts at `03b52577`) when changing
terrain topology. Test complete boundary profiles and terrain/road/tunnel joins;
passing pathfinding tests alone is not sufficient visual acceptance. Use the
Tile boundaries inspection mode to check actual mesh continuation.

Caves use the shared implicit solid and fixed tetrahedral lattice in `caves.h`
and `volume.h`. Do not reintroduce a roof cap, a fixed tunnel floor, or exceptions
to shared surface-edge matching. A* owns global passage connectivity; passage
WFC owns compatible arch sockets. Collision and support queries must sample the
same tetrahedra as rendering. Preserve ramp entrances and allow intentional
surface breaches. Validate stacked passages, body clearance, support, portal
references and internal mesh-edge pairing in both native and WASM builds.

Global world variety is part of correctness. Preserve seeded landform count and
shape, variable base sites, generated road walks, enclosed lake basins and
procedural chamber routes. Keep the same-settings diversity regression in the
native/WASM release checks; unique hashes or prop/material changes alone do not
prove world variety. Respect the two diagonals and rotational symmetry.

The ocean grid in `ocean.h` spans 96 × 96 cells around the 64 × 64 land region.
Preserve the submerged outer land sockets, continuous shelf, draft-aware naval
routing and separation from enclosed lakes. Naval IDs are not surface/cave IDs.

Version 8 fits optional mountain passages to the completed, validated world.
Never add peaks, raise a route foundation or reroll the landscape to force a
mountain/tunnel composition. Regional WFC domains must reflect existing dry
support, rock and reachable approaches. Failed candidates restore the world;
no suitable crossing is a valid result. Keep the larger-body bypass near the
existing surface, preserve covered travel, seeded topology and rotational
symmetry, and test that all original terrain/road/lake metadata stays unchanged.
The deep cave A* and its four profiles remain separate. Walkable spans come from
the same meshed density as support/collision; never link stacked floors by x/z
alone. Keep small/large-body and mesh tests in native/WASM checks. This remains
a regional grammar, not the complete road-search/hydrology research roadmap.

The user permits doubling map size when features need more room. Treat a larger
land grid as an architectural change: scale ocean bounds, navigation/storage
capacities and the browser mesh budget together; do not compress features merely
to preserve the current dimensions. Version 9 still uses the 64 × 64 land grid.

Version 9 combines pinned rolling lowland sockets with the existing cliff WFC.
Keep bank and road shoulders continuous, and retain relief/bridge coverage tests.
Bridges fit the completed terrain through `bridges.h`; deck geometry in
`bridge_field.h` is part of the shared solid, with separate walkable spans above
unchanged water beds. Validate both bank joins, both body sizes, headroom below
low decks, meaningful water gaps and rotational pairs. Never fill the channel
or reposition a lake to force a bridge. Decorative trusses are not collision.

`patrols.h` prepares route banks for twelve physical vehicles without changing
world generation or its hash. `vehicle_profiles.h` and `vehicles.h` own physical profile tradeoffs.
`mission_routes.h`, `missions.h` and `command_fleet.h` own current navigation,
policy inputs and viewer control; `local_navigation.h`/`fleet.h` retain the
historical v1 task. Preserve fixed-wing forward airspeed and slow turns,
quad hover/strafe, submarine submersion, boat draft/mast, and body clearance.
Use +Z forward in every model. Ground traction must affect both physical speed
and A* route cost. Keep rendered heading/sensor mounts tied to physical pose;
interpolate display poses without altering the fixed simulation timestep.
Each vehicle has independent recurrence; parameter sharing is within a family.
Disclose development reference control, trained checkpoints, and evaluation
limits. Do not call kinematic curriculum traffic joint multiagent training.
Unit-only see-through rendering must restore GL depth state and never change
terrain shadows, sensors, collision or map generation. Run vehicle, local adapter
and sensor checks in native/WASM, then inspect the actual browser roster.
Surface-triangle winding must use a reliable signed-field direction; retain the
sloped-mesh regression so near-zero samples cannot invert visible triangles.

Version 11 removes mixed biomes. Every world uses one palette: Temperate (1),
Desert (2), or Frozen (3), with Temperate as the default. Preserve these IDs;
retired Mixed (0), volcanic (4), and invalid palette IDs fall back to Temperate
in native generation and browser URLs. Keep fallback and single-climate material
checks in native/WASM validation. Version 10 already removed lava. Historical
training reports and recordings retain their original generator provenance.

Sensor contract v1 is in `sensors.h` / `sensor_rays.h`; see `docs/SENSORS.md`.
Keep exact pose separate from policy observations and ideal local odometry.
Any module can attach to any unit; the sonar mount must actually be underwater.
Use the shared tetrahedral solid, continuous ocean shelf and explicit unit body
proxies for range measurements. Keep fixed contiguous buffers, cached scheduled
samples and zero step allocations. Overlay visibility must not affect sensing,
RNG, navigation, camera framing or terrain shadows. The first camera is 8 × 6
radial depth, not RGB. Cosmetic props do not occlude sensors yet; do not imply
otherwise. Run `scripts/check_sensors.py` for sensor changes; retain the
independent triangle-intersection, reset/cadence, finite-data and native/WASM
checks. Do not call scripted traffic a trained sensor-driven policy.

`motion.h` retains historical eased yaw/pitch helpers. Physical Map Lab vehicle
heading comes from `vehicles.h`, shared by rendering and sensors. Keep its
rate/acceleration bounds, shortest-angle turning, pause/reset behavior and
30/60/120 Hz tests (`scripts/check_motion.py`). Retain validated support paths
through tunnels and bridges when changing motion. The quadrotor's cruise pace
is deliberately much lower than either fixed-wing variant; preserve that order.

Map camera input is owned by `installMapInput` in the web shell and
`camera_zoom.h`. Keep pointer capture, outside-release/focus-loss cleanup and
fractional trackpad wheel deltas; do not reintroduce GLFW's per-event minimum
wheel tick or double-handle wheel events. Preserve bounded elastic zoom,
immediate reverse input and preset resets. Run `scripts/check_camera.py` and
verify outside-canvas release in Chrome for input changes.

Shared navigation v2 lives in `mission_routes.h`, `missions.h`, `command_fleet.h`
and `ocean/alienwars_shared/`. Preserve physical flight primitives, synchronized
collision proposals, separated parking goals, independent terminal/recurrent
state, policy-specific gather batches and no step/reset allocations. Equipment
must alter policy observations; overlay visibility must not. Full missions and
route availability are measured separately. Run `scripts/check_shared.py`, the
CUDA joint gather test, single-policy regression and actual Chrome destination/
endurance checks. Keep source/checkpoint contracts exact (645 inputs, 4/3/3/3
heads); do not preload the historical 96-input policies into this viewer. Group
commands are independent destinations, not learned formation coordination.

Version 12 shapes the global layout inside a seeded horizontal elliptical envelope.
Keep the road search and lowland corridors inside that footprint before pinning
heights; do not crop finished routes to an oval. Preserve noisy coastlines, lakes,
180-degree symmetry, floor-10 access and the existing cave/bridge validation.
The historical Navigation Lab stays on generator 11 through `nav_core.h`; its
evaluated maps must not silently change. The Flecs pre-port trace also compiles
with generator 11 to compare the original distribution.

User art direction: replace conventional broadleaf/conifer trees with original
Felucia-inspired alien flora: fungal parasols, membrane fans and pod towers.
Keep environmental emission subordinate to unit and sensor signals. These remain
viewer-only props until explicitly incorporated into collision and sensing.

Each biome has its own alien ecology: Temperate fungi and membrane canopies,
Desert mushroom-cacti, and Frozen frost-harp fins. Beach grades are shared
terrain geometry, not visual displacement. Keep the 1.44-quarter-floor sea datum
consistent with naval drafts, sensors and dry-shore navigation (q >= 2 in v12).
Run `beach_test.c` in native/WASM, preserving cliffs, roads, symmetry and linear
shore support. Never lift only the rendered ocean independently of simulation.
