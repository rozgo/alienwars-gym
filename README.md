# AlienWars Gym

Procedural 3D battlefields and traversal experiments, built on **PufferLib 5.0**
with native C, Raylib and WebAssembly.

**[Open AlienWars Map Lab](https://rozgo.github.io/alienwars-gym/?seed=73)**

[Watch the 59-second showcase](https://rozgo.github.io/alienwars-gym/demo/)
· [Navigation Lab](https://rozgo.github.io/alienwars-gym/navigation/?kind=1)
· [Training Observatory](https://rozgo.github.io/alienwars-gym/training/)

Explore terrain, sensors, ground/sea/air units, tunnels and bridges, then watch
the trained scout and inspect recorded native training metrics.

Map Lab generates symmetric or asymmetric worlds with continuous WFC terrain,
rolling hills, cliffs, beaches, lakes and an extended ocean. Roads reach bases
up to ten floors high. Volumetric tunnels descend through ramp entrances, while
regional WFC fits optional mountain passages and exposed bypasses to existing
terrain. Bridges cross suitable water gaps without changing the landscape.

Three ground, three naval and three air unit types patrol the world using
body clearance, water depth and terrain-aware flight routes. Tunnel isolation
and automatic cutaways expose the underground structure. These are scripted
patrols. A separate experimental navigation task trains one action-driven scout
with native PufferLib; combat remains future work.

Units now carry ideal odometry and attachable LiDAR, sonar, RF and depth-camera
modules. Toggle their range overlays, inspect cached returns through terrain
and water, or view the small live depth image. Sensing runs in shared C without
rendering, with fixed observation buffers used by the navigation RL task.

- [Agent instructions](AGENTS.md)
- [Map Lab specification and validation](docs/MAPLAB.md)
- [Sensor architecture, observations and benchmarks](docs/SENSORS.md)
- [Navigation RL contract, training statistics and policy viewer](docs/NAVIGATION_RL.md)
- [Development and GPU workflow](docs/DEVELOPMENT.md)
- [Raylib web build and GitHub Pages](docs/WEB.md)
- [Demo recording and editing](scripts/demo/README.md)
- [Generation research](docs/GENERATION_RESEARCH.md)
- [Upstream source and provenance](docs/UPSTREAM.md)
- [PufferLib documentation](https://puffer.ai/docs.html)

## Run locally

Run from the repository root. On macOS, the native build uses the Xcode
command-line tools and Homebrew `libomp`. Raylib 5.5 is downloaded on first use.

```sh
./scripts/check.sh
./build/maplab --seed=73
```

The smoke check builds Map Lab and the upstream Minimal interface example with
ASan/UBSan, generates a headless world and runs Minimal for 1,024 steps.

For the browser build, install the isolated Emscripten SDK described in
[the web guide](docs/WEB.md), then:

```sh
./scripts/build_maplab.sh
python3 scripts/check_maplab.py
python3 scripts/check_sensors.py
python3 -m http.server 8781 --bind 127.0.0.1 --directory docs
```

Open <http://127.0.0.1:8781/>. The checker covers native/WASM generation parity,
navigation, terrain and tunnel meshes, world variety, and patrol clearance.

## Navigation reinforcement learning

Generation, collision, navigation, sensors and action-driven scout motion share
a renderer-independent C model. `ocean/alienwars/alienwars.h` adapts it to the
native `src/pufferenv.h` interface. The navigation task has two action heads,
621 observation floats, arrival/contact/failure rewards and explicit episodes.

```sh
python3 scripts/check_navigation.py
./build.sh alienwars build/nav-viewer --cpu --rl
./build/nav-viewer --env.controller=4 --env.maps=1  # manual control
python3 scripts/nav_dashboard.py                 # live training JSONL charts
# NVIDIA machine, from this checkout:
CUDA_HOME=/usr/local/cuda ./build.sh alienwars build/puffer-alienwars
./build/puffer-alienwars train --base.run_id=nav-pilot-unique
```

The policy viewer shows sensor returns, goals, trails, reward distances, reference
routes, action probabilities and value estimates. The default `--cpu` and `--web`
paths still open Map Lab; add `--rl` for Navigation Lab. See the
[navigation guide](docs/NAVIGATION_RL.md) for checkpoint playback and evaluation.

Training requires NVIDIA CUDA hardware. Browser simulation runs locally in
WASM; the project does not use the older Python/Gymnasium training path.
GPU access and reproducible development practices are in
[the development guide](docs/DEVELOPMENT.md).

Based on [PufferAI/PufferLib](https://github.com/PufferAI/PufferLib), retained
under its [MIT license](LICENSE). Third-party asset notices remain with their
source files.
