# Shared-world navigation contract (work in progress)

This milestone replaces isolated local route sections with complete missions
in a shared physical world. It is under development; the published v1 local
controllers and their measured results remain documented in LOCAL_NAVIGATION.md.

## Planner and control

Ground, boat, quadcopter and submarine destinations use their own traversable
graphs with segment/body validation. Fixed wings use forward simulation search
over position, altitude, heading and turn rate. Search primitives obey the same
actuator limits and collision queries as execution. A failed or exhausted search
returns an unavailable destination; it never substitutes an unchecked straight
line. Prepared graph/search storage is reused; simulation ticks do not allocate.

A deterministic low-level route tracker supplies continuous actuator setpoints.
PPO chooses speed and local steering, climb and strafe adjustments around that
tracker. This is residual local navigation, not direct motor control, and the
neutral-action tracker is an explicit evaluation baseline. Fixed wings always
retain positive airspeed. Arrival is a fly-through for wings; other units can
stop at their destination. No pose teleports are allowed during a mission.

All agents observe the same pre-action world. Proposals are integrated at 30 Hz
and body conflicts are resolved synchronously, without array-order priority.
Decisions run at 10 Hz. Five independent PPO learners share rollout worlds;
parameters are shared within a movement family and recurrent state belongs to
each agent. Updates occur between shared rollouts. This requires an explicit
multi-learner extension to the native trainer, not frozen traffic proxies.

## Perception

Policy observations include ideal odometry, body-relative route guidance,
vehicle limits, and the actual scheduled LiDAR, sonar, RF and depth-camera
measurements from the attachable modules. Missing/invalid modules have explicit
masks. Equipment affects perception; visualization toggles do not. There is no
privileged neighbor-velocity channel. Depth is geometric range, not RGB, and
RF retains the documented game propagation model. Rendering is never involved
in training observations.

## Validation and release gates

Use separate training, checkpoint-selection and final held-out map seeds.
Train at least three independent seeds. Report mission arrivals, collisions,
blocked time, route availability, and runtime per family and traffic setting;
invalid routes must not disappear from the report. Compare neutral tracker,
random actions and PPO with matching scenarios. The target is 95% complete
mission arrival per family; this is a target, not a claimed result.

Verify native/WASM physics and sensor parity, independent terminal/reset state,
no step/reset allocation, all five optimizer updates and policy-specific rollout
slices. Inspect destination selection, group commands, sensor equipment and
overlays in Chrome, then run a ten-minute mixed-fleet endurance scenario. Publish
only after measured results, checkpoint hashes and limitations are recorded.
