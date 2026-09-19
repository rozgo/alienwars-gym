# Engineering invariants

Read the sections relevant to a simulation or renderer change. These constraints
preserve the accepted terrain, physical behavior and checkpoint contracts.
The root [AGENTS.md](../AGENTS.md) selects workflows and checks; detailed
architecture remains in the linked subsystem documents. Generator version notes
explain compatibility requirements, not an instruction to restore old behavior.

## Native environment interface

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

## Map changes

For Map Lab changes, follow `MAPLAB.md`. Keep the strategic route plan,
road/tunnel/material WFC constraints, height surfaces, navigation and renderer coherent. Record a
generator-version change when seed outputs change. Validate rotational symmetry, cardinal edge
connections, all road lanes through floor 10, tunnel clearance and portals, resources and paths; test contradictions and
disconnected maps. Keep cosmetic props separate from authoritative collision.
The assembly view replays recorded cell resolution; do not call it a live solver
or claim it visualizes retries. New global-layout, combat or RL behavior needs
an explicit environment contract and appropriate validation.

## Terrain continuity

Natural terrain must retain connected 3D tile geometry: shared corner/edge
profiles, shaped cliff transitions, slopes and continuous shorelines. Do not
replace it with independent flat-topped columns or use material-only WFC as a
substitute for geometric tile constraints. Compare real browser renders with the
accepted Map Lab v1 contour quality (source/artifacts at `03b52577`) when changing
terrain topology. Test complete boundary profiles and terrain/road/tunnel joins;
passing pathfinding tests alone is not sufficient visual acceptance. Use the
Tile boundaries inspection mode to check actual mesh continuation.

## Caves and shared geometry

Caves use the shared implicit solid and fixed tetrahedral lattice in `caves.h`
and `volume.h`. Do not reintroduce a roof cap, a fixed tunnel floor, or exceptions
to shared surface-edge matching. A* owns global passage connectivity; passage
WFC owns compatible arch sockets. Collision and support queries must sample the
same tetrahedra as rendering. Preserve ramp entrances and allow intentional
surface breaches. Validate stacked passages, body clearance, support, portal
references and internal mesh-edge pairing in both native and WASM builds.

## World variety and ocean

Global world variety is part of correctness. Preserve seeded landform count and
shape, variable base sites, generated road walks, enclosed lake basins and
procedural chamber routes. Keep the same-settings diversity regression in the
native/WASM release checks; unique hashes or prop/material changes alone do not
prove world variety. Respect the two diagonals and rotational symmetry.

The ocean grid in `ocean.h` spans 96 × 96 cells around the 64 × 64 land region.
Preserve the submerged outer land sockets, continuous shelf, draft-aware naval
routing and separation from enclosed lakes. Naval IDs are not surface/cave IDs.

## Mountain passages (generator 8 onward)

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

## Hills and bridges (generator 9 onward)

Version 9 combines pinned rolling lowland sockets with the existing cliff WFC.
Keep bank and road shoulders continuous, and retain relief/bridge coverage tests.
Bridges fit the completed terrain through `bridges.h`; deck geometry in
`bridge_field.h` is part of the shared solid, with separate walkable spans above
unchanged water beds. Validate both bank joins, both body sizes, headroom below
low decks, meaningful water gaps and rotational pairs. Never fill the channel
or reposition a lake to force a bridge. Decorative trusses are not collision.

## Bodies and movement

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

## Single biomes (generator 11 onward)

Version 11 removes mixed biomes. Every world uses one palette: Temperate (1),
Desert (2), or Frozen (3), with Temperate as the default. Preserve these IDs;
retired Mixed (0), volcanic (4), and invalid palette IDs fall back to Temperate
in native generation and browser URLs. Keep fallback and single-climate material
checks in native/WASM validation. Version 10 already removed lava. Historical
training reports and recordings retain their original generator provenance.

## Sensors

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

## Motion and camera

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

## Shared navigation and policy compatibility

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

Navigation contract 3 adds `navigation_tracking.h`, `navigation_control.h` and
`navigation_recovery.h`. Tracks must come from actual timestamped RF or visible
range/depth returns; never substitute hidden neighbor pose or velocity. Preserve
body-center uncertainty, physical braking probes, forward-only wing motion and
bounded allocation-free recovery. This assistance is deterministic, separate
from learned PPO behavior. Use explicit checkpoint contract metadata despite
unchanged tensor dimensions. Measure collision-free requested arrivals, contact
episodes, unavailable routes, yielding and physical deadlocks separately. Keep
the selection/final-world split in `docs/NAVIGATION_RELIABILITY.md`. Historical
traffic is pinned in `config/alienwars_shared_frozen.json` and occupies extra
physical slots, never learner PPO rows. Require measured improvement before
replacing published policies.

## Coastlines and biological art (generator 12 onward)

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

## Bathymetry (generator 13 onward)

Version 13 caches coast-relative bathymetry in `bathymetry.h`. Keep the shared
seabed continuous across the land/ocean boundary and consistent with water depth,
sonar and submarine collision. Preserve lake beds and dry beach sockets. The
cache is reset-time only; run bathymetry, sensor and shared-navigation parity
checks when changing it. Never restore a flat square seabed inside the land grid.

## Viewer episode lifecycle

Automatic viewer patrols loop failed/stalled episodes through `command_fleet.h`;
this is demo lifecycle, not learned recovery. Preserve pause/manual-command
semantics, hull-clear unoccupied respawn anchors, per-unit terminal/sensor resets
and separate restart/arrival counters. Never enable demo respawns in training or
held-out evaluation, or claim looping as collision-free navigation. The endurance
test defaults to no respawns; its explicit `loop` mode verifies the demo lifecycle.
