# AlienWars Gym

A gym for training AlienWars agents in procedural 3D worlds. **PufferLib 5** trains
native PPO controllers; **Flecs** manages live unit state; **Raylib** renders the world; **WebAssembly** runs the
simulation, sensing and policy inference in your browser.

**[Explore Map Lab](https://rozgo.github.io/alienwars-gym/?seed=73)** ·
[Live Flecs Explorer](https://rozgo.github.io/alienwars-gym/maplab/?seed=73&explorer=1) ·
[Watch the showcase](https://rozgo.github.io/alienwars-gym/demo/) ·
[Fleet training results](https://rozgo.github.io/alienwars-gym/training/fleet.html) ·
[Training Observatory](https://rozgo.github.io/alienwars-gym/training/)

[![Temperate battlefield in AlienWars Map Lab, with raised bases, roads, lakes and an extended ocean](docs/demo/images/temperate.webp?v=d509f2556754)](https://rozgo.github.io/alienwars-gym/maplab/?seed=73&biome=1)

## Worlds worth exploring

**Wave Function Collapse** connects terrain tiles, road grades and tunnel
profiles. Irregular oval coastlines, seeded landforms, rolling hills, beaches, ocean cliffs and lakes create
varied terrain; bridges and mountain passages fit the generated landscape.
The ocean floor follows the coastline, descending smoothly from shallow shelves
into deep water with the same geometry used by sonar and submarine navigation.
Choose symmetric or asymmetric layouts, bases up to ten floors high, and one
coherent terrain palette: **Temperate, Desert or Frozen**.

| Asymmetric desert | Frozen frontier |
| --- | --- |
| [![Asymmetric desert with different base heights and a winding coastline](docs/demo/images/desert.webp?v=e7342be1e120)](https://rozgo.github.io/alienwars-gym/maplab/?seed=175847449&sym=0&a=2&b=9&biome=2&sensors=0) | [![Frozen landscape with snow, ice, roads and coastal cliffs](docs/demo/images/frozen.webp?v=0e5501f9a145)](https://rozgo.github.io/alienwars-gym/maplab/?seed=2279248715&sym=1&a=6&b=6&biome=3&sensors=0) |

## A living art direction

The biological kit brings distinct grown bodies for **all twelve fleet roles**, plus
nursery forms at both bases. Alien fungal parasols, membrane fans and pod towers
replace traditional trees, with mushroom-cacti in Desert and frost harps in Frozen. Four 1K material scans, stronger baked and cast shadows,
shore foam, boat wakes and contact shadows give the world more physical
presence. **Play view** expands the battlefield and tucks away the inspector.

[![Biological scout among alien fungi in the live Temperate world](docs/demo/images/biological.webp?v=75d1951920c2)](https://rozgo.github.io/alienwars-gym/maplab/?seed=73&biome=1&view=play&sensors=0)

The [art pipeline](docs/ART_PIPELINE.md) includes reproducible Blender sources and
asset licenses. Cultivation and combat remain planned. Rendering changes preserve
the trained navigation policies; generator-v13 terrain has not been reevaluated
for the historical training scores.

## Twelve vehicles. Five learning families. One shared world.

Three ground vehicles, three boats, three aircraft and three submarines have
different hulls, speeds, turning limits and sensor reach. Fixed wings maintain
forward airspeed; the quadcopter can hover and strafe. Boats respect draft;
submarines navigate below the surface.

**A\* plans global routes. PPO controls local navigation.** Five family policies
train simultaneously in shared worlds, with individual destinations and recurrent
memory for each vehicle. The browser loads the selected trained checkpoints.
Select a unit or family and issue destinations, then follow their progress.

| See beneath the landscape | Navigate beneath the ocean |
| --- | --- |
| [![Isolated volumetric tunnel network with ramps and underground chambers](docs/demo/images/tunnels.webp?v=09fba0d99610)](https://rozgo.github.io/alienwars-gym/maplab/?seed=2279248715&biome=3&isolate=1&sensors=0) | [![Heavy submarine visible through water with its sonar range](docs/demo/images/submarine.webp?v=770b0298e4d5)](https://rozgo.github.io/alienwars-gym/maplab/?seed=73&biome=1&unit=11&sensors=2) |

The [recorded evaluation](docs/runs/shared-navigation-2026-09-16.md) covers three
training seeds and 49.5 million steps. Local navigation remains experimental;
combat and tactical coordination are future work.

The long-term goal is warfare between alien factions whose armies are **grown**:
prepare soil and water nurseries, seed and nurture organisms, then harvest
ammunition and awaken living craft. Humans do not exist in this world. Cultivation
and combat are planned; see the [biological warfare direction](docs/BIOLOGICAL_WARFARE.md)
and the proposed [Cultivate and Defend slice](docs/CULTIVATION_SLICE.md).
The next RL iteration will explicitly consider implementing self-play; see the
[self-play development direction](docs/SHARED_NAVIGATION.md#self-play-and-faction-warfare).

## See what the agents sense

Attach **LiDAR, sonar, RF and depth cameras** to units, with ideal local odometry
for motion. Toggle sensor overlays for one unit or the whole fleet, inspect
returns through terrain and water, and view the live depth image. Equipment
changes policy inputs; overlay visibility only changes the display.

[![All-unit sensor overlays showing range fans, camera fields and radio connections across the battlefield](docs/demo/images/sensors.webp?v=a3467a85de7c)](https://rozgo.github.io/alienwars-gym/maplab/?seed=73&biome=1&sensors=15&sensorsAll=1&unit=7)

Generation, collision, navigation and sensing share a renderer-independent C
model. Sensor sampling uses fixed buffers and runs without graphics during
training. The depth camera measures geometric depth; trees and rocks are
currently decorative rather than sensor occluders.

## Inspect the living simulation

Open **Flecs Explorer** in Map Lab to inspect the selected unit's live body state,
mission, odometry, sensors and PPO inputs. The official Explorer reads the same
Flecs world used by the viewer; the training build keeps these inspection addons disabled.

[![Official Flecs Explorer inspecting a live unit beside the Temperate battlefield](docs/demo/images/explorer.webp?v=472d133b82ff)](https://rozgo.github.io/alienwars-gym/maplab/?seed=73&biome=1&unit=7&explorer=1)

## Try it

Open **[Map Lab](https://rozgo.github.io/alienwars-gym/?seed=73)** — no installation.

- Drag to pan; Shift-drag to orbit; scroll to zoom.
- Click a vehicle; Command/Ctrl-click to add units; right-click a destination.
- Toggle sensor layers, select **All units**, or **Follow selected unit**.
- **Isolate tunnels** reveals underground routes without moving the camera.
- Choose a terrain palette and **New world** to explore another seed.

## Run locally

Run from the repository root. Install [uv](https://docs.astral.sh/uv/getting-started/installation/),
then run `uv sync --locked`. `.python-version` pins Python 3.12.12; uv manages its
isolated tooling environment. Core build/check/training-launch scripts use only
the standard library. Use `uv sync --locked --group art` for the optional Pillow
image tools; Blender scripts run inside Blender. On macOS, use the Xcode command-line tools and
Homebrew `libomp`. Raylib 5.5 downloads on first use.

```sh
./scripts/check.sh
./build/maplab --seed=73 --biome=1
```

The smoke check builds Map Lab and the upstream Minimal interface example with
ASan/UBSan, generates a headless world and runs Minimal for 1,024 steps.

For the browser build, install the isolated Emscripten SDK in the
[web guide](docs/WEB.md), then:

```sh
uv run scripts/build_fleet_site.py  # five verified PPO checkpoints
uv run scripts/check_maplab.py
uv run scripts/check_shared.py
uv run python -m http.server 8781 --bind 127.0.0.1 --directory docs
```

Open <http://127.0.0.1:8781/>. Training runs on an NVIDIA CUDA machine:

```sh
./build.sh alienwars_shared build/puffer-shared
gh release download shared-navigation-2026-09-16 --pattern 'mission-*.bin' \
  --dir outputs/shared/deployed
uv run scripts/train_shared.py --prefix UNIQUE_RUN_NAME \
  --frozen outputs/shared/deployed
```

See the [shared-world training contract](docs/SHARED_NAVIGATION.md) and
[GPU workflow](docs/DEVELOPMENT.md) for setup, curricula and reproducibility.
The project uses PufferLib's native C/CUDA API. Browser simulation and inference
run locally in WebAssembly.

## Go deeper

- [Biological warfare and cultivation](docs/BIOLOGICAL_WARFARE.md) · [Next development slice](docs/CULTIVATION_SLICE.md)
- [Map generation and validation](docs/MAPLAB.md) · [Generation research](docs/GENERATION_RESEARCH.md)
- [Sensor contract and benchmarks](docs/SENSORS.md)
- [Shared-world PPO, vehicle profiles and missions](docs/SHARED_NAVIGATION.md)
- [Flecs C runtime, ownership and validation](docs/FLECS.md)
- [Historical trained-scout Navigation Lab](https://rozgo.github.io/alienwars-gym/navigation/?kind=1) · [Its training contract](docs/NAVIGATION_RL.md)
- [Raylib web builds and GitHub Pages](docs/WEB.md)
- [Showcase recording and screenshots](scripts/demo/README.md)
- [Agent instructions](AGENTS.md) · [Upstream provenance](docs/UPSTREAM.md)
- [PufferLib](https://github.com/PufferAI/PufferLib) · [Raylib](https://github.com/raysan5/raylib)

Based on [PufferAI/PufferLib](https://github.com/PufferAI/PufferLib), retained
under its [MIT license](LICENSE). Third-party asset notices remain with their
source files.
