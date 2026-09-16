# A* routes and local vehicle policies

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
