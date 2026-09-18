# Historical local vehicle policies

This is the version 1 experiment. Current Map Lab uses the shared-world
[version 2 contract](SHARED_NAVIGATION.md), including actual attachable sensors,
heading-aware flight routes and five learners in the same rollout.

Contract version 1 introduces twelve units and five local-controller families:
ground, surface boat, quadcopter, fixed-wing, and submarine. Three variants in
each ground/water/air/submarine group have distinct dimensions, acceleration,
speed, turning limits, clearance and sensing range. All models use +Z forward. The tracked rover trades road speed for better
traction on mud, sand, snow and rock; A* uses the same surface-dependent travel
cost. Larger boats and submarines trade agility/access for longer sensor range.
The transport aircraft has wider sensor coverage than the faster recon wing.
These are navigation/sensing tradeoffs; payload and combat benefits are not implemented.

A* chooses global routes on separate land, sea, flight and underwater graphs.
The underwater graph samples four depth levels; flight samples sixteen altitude
levels. Connections are checked along their length, not only at endpoints.
Ground routes preserve separate cave/bridge floors. Surface boats require draft
and mast clearance. Submarines remain below water and above the seabed, within
sea-connected water. The local controller follows successive route targets.

The 96-float local input contains body-relative unit direction / distance to lookahead and next waypoint,
ideal local velocity/angular rate, vehicle limits and identity, ground traction, 24 terrain/body
range beams, and up to four visible body tracks. Tracks provide ideal relative
position and velocity; noisy velocity estimation is not implemented. Ranges and
tracks are headless queries, independent of overlay visibility. Global waypoint
localization is ideal and intentionally supplied by A*. There is no imitation
of optimal actions in the observation.

Four categorical action heads each have three choices: backward/neutral/forward,
left/neutral/right yaw, descend/neutral/ascend, and left/neutral/right strafe.
Only quadcopters have lateral thrust. Ground and surface boats ignore vertical
and lateral commands. Submarines have forward/reverse and vertical thrust.
Fixed-wing throttle selects minimum/cruise/maximum positive airspeed, never
reverse or hover; yaw acceleration/rate and climb/descent are bounded by physics.
Quadcopters can translate independently of their heading and remain slower than
fixed-wing aircraft. Models, sensor mounts and collision share the same pose.

PPO decides at 10 Hz; movement uses three 30 Hz substeps. Collision rejects a
blocked ground/water/quad move and reports contact; a fixed-wing impact ends the
episode. Oriented body boxes cover neighboring vehicles. Terrain, seabed and
water boundaries come from the shared world. Curriculum traffic includes fixed
obstacles and moving body proxies. These are kinematic game vehicles, not full
aerodynamic or hydrodynamic simulations.

Training uses short sections of prepared A* routes, randomized start headings
and variants, then traffic. Each family has a separately trained checkpoint.
Reward is route-distance progress times .08, minus .002 per decision and .15 per
contact, plus 2 for arrival or minus 2 for a fixed-wing collision. Episodes end
at arrival, fixed-wing collision or 500 decisions. The progress term is heuristic
shaping, not a claim of discount-invariant potential shaping. Ground/boat/sub
arrival tolerance is 1.15 world units, quad 1.7, and wing 6 with wider intermediate
route targets to permit continuous forward turns. Resets clear recurrent state.

Preparation owns all allocation and route searches; simulation stepping does
not allocate. Training bank generation is separate from rollout throughput.
Held-out maps and family-specific evaluation are required before a checkpoint
is described as trained and validated. The previous 621-input scout checkpoint
and Navigation Lab remain a separate historical experiment.


The flight graph searches position and altitude, not heading. Fixed-wing turn
limits are enforced by the vehicle physics, with wider waypoint acceptance to
permit curved flight. This is not a complete Dubins/heading-lattice planner;
tight combinations can still be infeasible for a local controller. Cosmetic
trees/boulders remain separate from authoritative terrain/body collision.

Map Lab uses 22-unit local route sections (40 for fixed wings), preserves pose
at section boundaries, resets per-unit recurrence, and returns or closes the
global route on arrival. A timed-out section retries from its current physical
pose; a fixed-wing collision stops that aircraft. Units never teleport to cover
failed motion. Five checkpoints share parameters within families, with a distinct
recurrent state for every vehicle. Missing development checkpoints use a visibly
labeled reference controller. This is not joint multiagent training: curriculum
traffic is kinematic, and other learned controllers are not optimized in the
same rollout.

To train a family on the GPU machine, first establish route following, then
initialize a separate traffic run from its exact checkpoint (weights only;
optimizer and recurrence restart):

```sh
./build.sh alienwars_local build/puffer-local
./build/puffer-local train --base.run_id=local-ground-clear-unique --env.family=0
./build/puffer-local train --base.run_id=local-ground-traffic-unique \
  --env.family=0 --env.difficulty=1 --train.learning_rate=0.012 \
  --base.load_model_path=checkpoints/alienwars_local/local-ground-clear-unique/0000000004194304.bin
```

Family IDs are 0 ground, 1 boats, 2 quadcopters, 3 fixed wings and 4 submarines.
Use a new run ID every time. The native evaluator accepts `reference`, `random`,
or an exact flat checkpoint; it uses a separate action RNG so all controllers
receive the same sequence of evaluation scenarios:

```sh
clang -O2 -std=c11 -I. -Isrc -Ivendor -Iraylib-5.5_linux_amd64/include \
  ocean/alienwars_local/local_eval.c ocean/alienwars_local/local_api.c -lm -o build/local-eval
./build/local-eval CHECKPOINT.bin 0 20001 8 128 1
uv run scripts/check_vehicles.py
```

The generic 24-beam/visible-neighbor controller input is separate from the
613-float attachable-sensor experiment and its overlay equipment controls.
Changing preview overlays or preview equipment does not change this policy's
fixed sensor profile. Sensor ranges still vary by vehicle profile. Shared ray
and body geometry keeps these headless queries inexpensive; RGB inference and
noisy tracking are not part of this task.
