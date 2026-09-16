# Shared-world navigation

Contract version 2 runs twelve physical agents and five independent PPO learners
in the same worlds: ground, boats, quadcopters, fixed wings and submarines.
Every agent has its own destination, observation, terminal state and recurrent
memory. Variants within a movement family share parameters. Training is native
PufferLib 5 C/CUDA; browser inference uses the same C simulation and Raylib/WASM.
The historical single-family experiment remains in [LOCAL_NAVIGATION.md](LOCAL_NAVIGATION.md).

## Global routes and physical control

`mission_routes.h` prepares separate land, ocean, air-volume and submarine
traversability graphs. A* connects nearby body-clear endpoints. Ground floors
remain distinct through caves and bridges; boats need continuous draft and mast
clearance; submarines remain in sea-connected water below the surface. Edges and
shortcuts are sampled at 0.25 world-unit intervals against shared terrain and
oriented hulls. Ground/boat goals may project up to four world units to nearby
valid support; the displayed destination is the actual accepted endpoint.

Fixed wings use bounded, weighted A* over forward-simulated motion primitives.
The bin includes position, altitude, heading, yaw rate and climb rate. Every
primitive executes the actual 30 Hz acceleration, turn, climb and collision
model for 1.5 seconds. Immutable parent labels preserve the physical trajectory;
tests replay every exported point with the same actuator inputs. Search uses a
1.8-weight heuristic and at most 48,000 labels. It is neither optimal nor
complete: unavailable destinations are explicit, never unchecked straight lines.
The tracker and a learned deviation can still fail despite a feasible route.

A deterministic low-level route tracker provides continuous setpoints. PPO
selects speed and overrides local turn, climb or strafe to negotiate obstacles
and other vehicles. Neutral actions follow the route; this tracker is an explicit
evaluation baseline. The four categorical heads have sizes **4, 3, 3, 3**:

- Longitudinal: reverse, stop, cruise, full. Wing reverse/stop mean minimum
  positive airspeed; wings cannot hover or reverse.
- Yaw: maximum left, route tracking, maximum right.
- Vertical: descend, route tracking, ascend. Ground and boats ignore this head.
- Lateral: left, route tracking, right. Only quadcopters use lateral thrust.

Physics bounds all commands. Quadcopters can hover/strafe and are slower than
fixed wings. Hull dimensions, turn rates, traction, draft and sensor reach remain
variant-specific. All rendered models use +Z forward and the physical pose.
These are kinematic vehicle models, not aerodynamic/hydrodynamic simulations.

## Simultaneous stepping and episodes

Decisions run at 10 Hz with three 30 Hz physical substeps. Every agent observes
the same pre-action world. Proposed moves are resolved synchronously against
oriented body boxes; rejected positions propagate until stable without giving
array-order priority. A blocked move returns contact; a wing collision terminates
that aircraft. This rejects penetration rather than modeling physical impacts.

Reward per decision is `0.06 * change(along-route distance - cross-track distance)
- 0.002 - 0.3 * contact + 3 * arrival - 3 * wing impact`. This is heuristic shaping,
not discount-invariant potential shaping. Arrival requires remaining path length
below twice the tolerance and endpoint distance below the tolerance: one world
unit, or four for a wing fly-through. Limits are 40, 100 and 240 simulated seconds
for clear, traffic and full-mission curricula respectively.

A world resets after all available participants finish. Completed ground/water
vehicles remain parked; completed wings depart the episodic scenario. Agent
terminals mask completed actors to neutral and reset their recurrent memory.
Parking goals are separated by hull lengths plus a margin. Failed searches and
unavailable spawns are reported, not counted as successful missions. Map/route
banks and all search/rollout buffers are prepared before stepping; step/reset
allocate nothing. Generation remains version 10 with unchanged map hashes.

## Actual sensor observations

Each policy receives **645 floats**: 32 fields of body-relative route guidance,
vehicle limits, family/variant identity and mission state, followed by the
[613-float sensor contract](SENSORS.md). The latter contains ideal local odometry
and actual scheduled LiDAR, sonar, RF and 8 × 6 depth-camera samples. There is no
ideal neighbor-velocity channel. Disabled/invalid modules have explicit masks;
readings remain cached according to their sample periods. Traffic curricula
remove one randomly chosen module on 20% of agents at reset.

The viewer's equipment switches change these policy inputs. Overlay switches
only affect rendering. Global route localization and odometry are ideal. Camera
samples are radial geometric depth, not RGB; RF uses the documented game
propagation model. Terrain, ocean and oriented vehicle hulls occlude sensors;
cosmetic trees/boulders still do not.

## Five native PPO learners

The opt-in `vec.train_all_policies=1` extension preserves each `Agent.policy`
assignment. All five policies contribute actions to each shared rollout, then
receive separate compact PPO batches, recurrent initial states, gradients and
optimizers. No family's samples enter another family's learner. Each network
has two recurrent layers of width 128 and 182,656 parameters. This is independent
PPO in a shared world, with decentralized family policies; there is no centralized
critic, team reward or learned tactical coordination.

This path currently supports synchronous, single-GPU CPU environments. The
normal single-policy/self-play path remains selected when the flag is zero.
The joint gather CUDA test compares every field against independent sentinel
indices across two buffers and unequal family sizes. Smoke training verifies
that every family's weights change and every learner reports finite losses.

```sh
./build.sh alienwars_shared build/puffer-shared
python3 scripts/train_shared.py --prefix UNIQUE_RUN_NAME
python3 scripts/check_shared.py
```

Run training from a clean isolated GPU checkout. The curriculum script records
three independent seeds, exact source/config/commands, steps, timings and all
five hashes for each stage. `base.load_model_dir` requires `mission-0.bin` through
`mission-4.bin`; it initializes weights only, with fresh optimizer and recurrence.
Do not mix these 645-input checkpoints with the old 96-input local policies.

## Interactive missions

Click a vehicle to select it; Command/Ctrl-click adds or removes a selection.
“Select family” selects all available units in the same movement family.
Right-click a destination, or arm “Set destination” and left-click the map.
Drag pans; Shift-drag orbits; releasing outside the canvas cannot issue a move.
Air/submarine commands use current height/depth unless an explicit world-space
height is entered. Boats stay at sea level. Tunnel isolation permits selecting
underground floor endpoints without moving the camera.

Group goals spread along X, with each hull independently validated; rejected
members retain their existing mission and the UI reports the accepted count.
This is group destination assignment, not a formation or reservation planner.
Mixed groups retain each other air/submarine family's current vertical level.
Green selection and gold destination rings remain visible through terrain/water.
“Follow selected unit” tracks whichever vehicle is selected.

Commands preserve pose and momentum. Recurrent state and the policy’s mission-distance reference reset for a new goal;
the displayed lifetime odometer continues. This matches the episodic training
reference without teleporting a pose or clearing real sensor samples.
Automatic demo patrols plan their return trip from the actual arrival pose.
Manual ground/boat/quad/sub missions stop after arrival. Fixed wings continue to
a return destination because stopping is physically invalid. Failed aircraft
stop and remain visibly marked; Restart vehicles explicitly respawns the fleet.

## Evaluation and limits

Training worlds begin at seed 301; selection worlds at 11001; final held-out
worlds at 21001. Evaluate the same ordered map/scenario pairs for PPO, neutral
tracker and random actions. Report requested and available-route arrival rates,
route failures, contact decisions, collision episodes and blocked decisions.
A blocked decision means less than 0.01 world units of potential gain, including
deliberate waiting/backing up; it is not automatically a deadlock.
Three training seeds are required before claiming robustness. **95% full-mission
arrival per family is the target, not an assumed result.** Browser endurance and
final measured results are recorded in the release report.

Joint learning is now supported; reliable indefinite driving, narrow-passage
right-of-way, dynamic global replanning and tactical coordination are not
established merely by completing training. Cosmetic props, noisy perception,
combat and payload objectives remain separate work.
