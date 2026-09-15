# Unit sensors

Sensor contract v1 is a renderer-independent C module in
[`sensors.h`](../ocean/alienwars/sensors.h). The nine scripted Map Lab units carry
the same modules that a future PufferLib environment can sample without Raylib,
WebGL, image readback or Python. This is not yet an AlienWars training task.

## Equipment and measurements

Every unit has exact simulator pose and ideal odometry. Pose is a separate
privileged output: an experiment can expose it to a policy, use it only for the
critic, or omit it. Odometry measures displacement in the previous body frame,
Euler-angle increments, linear/angular velocity and distance travelled. It is
currently noiseless; it is not a simulated wheel encoder, IMU or drifting SLAM
estimate. Teleporting or changing the inspection tour resets odometry and
invalidates the old measurements.

Each unit has four independent module slots. Any of the four types can be
attached to any ground, naval or air unit. Each mount has local translation,
yaw, pitch and roll, plus range, field of view and sampling period. The UI
changes attachment and visibility; mount calibration is configured in C.

| Module | Default measurement | Range | Rate |
| --- | --- | --- | --- |
| LiDAR | 32 horizontal, evenly spaced range beams, 360° | 24 world units | 5 Hz |
| Sonar | 8 azimuths × 3 elevations, downward fan | 32 | 2 Hz |
| RF | Equipped peer detections, range, bearing, elevation and strength | 64 | 2 Hz |
| Depth camera | 8 × 6 pinhole image, 90° horizontal FOV | 36 | 2 Hz |

Ground and air units start with LiDAR, RF and a depth camera. Naval units start
with sonar, RF and a depth camera. Sonar requires a mount below the water surface
and above the bed. A dry cave below sea level is not treated as water. Optical
rays stop at the water surface; underwater sonar rays reach bathymetry and hull
proxies. Range beams return the nearest terrain, water, unit or domain-boundary
intersection, or maximum range for no return. Camera pixels store **radial
range**, not camera-axis depth or RGB. Air cameras look downward.

RF is a disclosed game abstraction: inverse-square path loss with one 18 dB
penalty for terrain obstruction, and a range-dependent receiver budget. It does
not model multipath, frequency bands, interference or an actual antenna pattern.
Only active units with an attached RF module transmit. Sonar similarly models
geometric range, not acoustic propagation. Decorative trees, rocks and bridge
trusses are not authoritative collision objects and do not occlude sensors.
Unit detection currently uses spherical body proxies; patrols still do not
avoid one another.

## Geometry and runtime cost

[`sensor_rays.h`](../ocean/alienwars/sensor_rays.h) queries the same piecewise
linear solid as collision and marching tetrahedra. A conservative per-tile top
bound skips air above terrain. Near surfaces, voxel DDA visits the shared
lattice; fractional-coordinate equality planes split each interval into its
Freudenthal tetrahedra. Each interval has a linear field, so its first zero can
be solved directly. This avoids raymarch step sizes skipping thin bridge decks,
cave ceilings or stacked floors. The outer ocean shelf uses analytic plane
intersections. No second triangle mesh or full-volume BVH is stored per world.

`AwSensors` owns fixed-capacity arrays (16 units, four slots, up to 48 rays per
slot) and contiguous float observations. Initialization builds the small terrain
height-bound array. Stepping performs no allocations, rendering, network I/O or
map/RNG mutations. All unit poses must be supplied before a step. Scheduled
samples are cached with timestamps; module phases spread work after the initial
snapshot. A late step samples once and advances its deadline without a catch-up
burst. The training caller should use a fixed simulation timestep; Map Lab uses
its bounded frame timestep.

```c
static AwSensors sensors;  // Per environment; allocate once, not on the step stack.
aw_sensors_init(&sensors, &map, agent_count);
aw_sensor_equip(&sensors, 0, 0, .65f);  // Ground defaults and body radius.

AwSensorConfig camera = aw_sensor_default(AW_SENSOR_CAMERA, 0);
camera.mount.position = (AwSVec){0, 1.1f, .2f};
camera.period = .5f;
aw_sensor_attach(&sensors, 0, AW_SENSOR_CAMERA, camera);

// Every simulation step, populate poses for every active unit first.
sensors.units[0].pose = (AwSensorPose){.position={x,y,z}, .yaw=yaw};
aw_sensors_step(&sensors, &map, 1.0f/60);
// sensors.observations[agent] is fixed-size, renderer-independent policy input.
// sensors.privileged_pose[agent] is separately available for pose-aware tasks.
```

Coordinates use two world units per tile and `y = .75*q - 1.2`. Body axes are
right/up/forward = +x/+y/+z. Positive yaw points toward +x, positive pitch raises
the nose, and positive roll raises the right side. Poses must be finite and body
radii positive. Mount translation and rotation compose with the full unit pose.
Readings keep their acquisition pose and beam directions; overlays do not move
old returns with the live unit.

## Fixed observation layout

`AW_SENSOR_OBS` is **613 floats per unit**, independent of attached equipment.
Inactive units and unused payload entries are zeroed. All values are finite and
scaled/clipped to [-1, 1] or [0, 1]. Missing equipment and invalid measurements
have explicit flags; an invalid reading is not a zero-distance obstacle.

| Offset | Count | Contents |
| --- | --- | --- |
| 0 | 3 | Local displacement / 4 |
| 3 | 3 | Pitch, yaw, roll increments / π |
| 6 | 3 | Local velocity / 16 |
| 9 | 3 | Pitch, yaw, roll rate / 8 |
| 12 | 1 | Travelled distance / 1024 |
| 13 + 150 × type | 6 | Attached, valid, sample age / period (clipped to 1), range / 128, horizontal FOV / 2π, vertical FOV / π |
| Slot + 6 | 144 | Type-specific payload with zero padding |

Slot order is LiDAR, sonar, RF, camera. Each range beam uses three floats:
distance / range, hit kind / 4, and (entity ID + 1) / 16. Hit kinds are no return
(0), terrain (1), unit (2), water (3), boundary (4). Terrain/water/no-return IDs
are -1 before encoding. Unused LiDAR/sonar beam entries are zero; the contract's
beam counts distinguish them from actual readings.

RF uses six floats per stable peer slot: detected, distance / range, sine and
cosine of body-relative azimuth, sine of world elevation, and signal strength.
Undetected and unused peers are zero. The nine privileged floats are normalized
world position, then sine/cosine pairs for yaw, pitch and roll. They are never
appended to the 613 perceptual/odometry floats implicitly. Configured mounts and
body class are known calibration; changing equipment during training may require
explicit calibration features in a future version of the observation contract.

## Browser overlays

The **Unit sensors** panel selects any of the nine units. **Focus** is an explicit
camera action; changing sensor visibility does not change the camera. Four
independent colored toggles show LiDAR scans, sonar fans, RF range/links and
camera frustums/returns. **All units** expands the display; **See through** adds a
faint underlay through terrain and water without changing terrain or shadows.
Dots are measured returns. Dashed range guides describe the instrument limit;
the sampled fan is not proof of continuous visibility between beams.

The camera preview shows the actual cached 8 × 6 ranges, with brighter pixels
closer, blue water and mint unit hits. It is not a rendered color camera. Opening
**Equipment on selected unit** attaches/detaches modules. Equipment choices
persist across new worlds in the current session. Seed links preserve the
selected unit and overlay settings, but not equipment edits.

## Validation and next stages

Run `python3 scripts/check_sensors.py`. Native ASan/UBSan and WASM tests compare
704 rays against independently intersected terrain triangles across caves,
roofs, bridges, breaches and slopes. They also cover sea boundaries, bathymetry,
body hits, mount transforms, RF attenuation, odometry wrap/reset, fixed cadence,
cache immutability, invalid configuration, finite observations, disabled slots,
determinism and an allocation guard during stepping. The benchmark samples nine
units in three generated worlds for 1,800 steps, including an underground scout;
generation, pose preparation and rendering are outside its timing. Results go to
`build/sensor-check.json`; published measurements are in `docs/runs/`.

Next: define the actual AlienWars observation/action/reward/reset contract,
add authoritative prop bodies where gameplay needs them, then introduce seeded
sensor noise/dropout and task-specific resolution. Keep exact pose available for
debugging and privileged training without accidentally granting it to a partially
observed policy. RGB should be an optional, separately budgeted batched render
path if a task requires visual appearance. A CUDA sensor backend should follow
measured CPU bottlenecks and matched-result tests, not precede the baseline.

Design references: [PufferLib's native contiguous-buffer architecture](https://puffer.ai/docs.html),
[Isaac Lab ray-caster separation and update periods](https://isaac-sim.github.io/IsaacLab/main/source/overview/core-concepts/sensors/ray_caster.html),
and [Isaac Lab's batched camera discussion](https://isaac-sim.github.io/IsaacLab/main/source/overview/core-concepts/sensors/camera.html).
These inform the separation of simulation, sensing and display; Isaac Lab is
not a dependency of this implementation.
