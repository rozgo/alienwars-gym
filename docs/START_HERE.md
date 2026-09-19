# Start here

This guide covers running AlienWars, inspecting its sensors and building a source
change. The browser version includes trained navigation policies, so you can
start with the sensor exercise before setting up a development environment.

| I want to… | Start with | You need |
| --- | --- | --- |
| Explore and learn | [Browser lessons](https://rozgo.github.io/alienwars-gym/learn/) | A browser with WebGL 2 |
| Run my own copy | Preview below | Git and uv |
| Change the world or interface | [Build an edit](#build-an-edit) | macOS/Linux, compiler; Emscripten for web |
| Train policies | [Training](#training) | Linux NVIDIA GPU and CUDA development tools |

## Preview without compiling

Install [Git](https://git-scm.com/downloads) and
[uv](https://docs.astral.sh/uv/getting-started/installation/), then:

```sh
git clone https://github.com/rozgo/alienwars-gym.git
cd alienwars-gym
uv sync --locked
uv run scripts/doctor.py
uv run python -m http.server 8781 --bind 127.0.0.1 --directory docs
```

Open <http://127.0.0.1:8781/learn/>. Choose **Open the submarine exercise**.
When the map has loaded, select a unit and check that its sensor readings appear
in the inspector. The viewer loads five packaged trained policies. Generation can take
tens of seconds, especially on slower machines; leave the tab open while it loads.
Stop the local server with Ctrl+C. If port 8781 is occupied, use 8782 instead.

This serves the committed WebAssembly build. It needs no CUDA, Node, Emscripten
or Python ML packages. uv manages the pinned Python interpreter; Python only
serves files here. Editing C or `web/maplab/shell.html` needs a rebuild.

## Work with a coding agent

Open this checkout in your agent and paste:

> Read AGENTS.md and .agents/skills/alienwars-start/SKILL.md. Help me run this repo
> locally and walk me through docs/lessons/sensor-equipment.md. Explain what
> each control changes, help me check the readings, and show me the relevant code.

The repository skill follows the [Agent Skills format](https://agentskills.io/specification).
In Codex, start a session in the checkout and invoke `$alienwars-start`;
[repository discovery uses .agents/skills](https://developers.openai.com/codex/skills).
Other agents have different discovery paths: the explicit file-reading prompt
above works without copying skills into global directories. Automatic discovery
is client-dependent; a valid skill file is not a guarantee every client loads it.

## Your first experiment

[Sensor equipment and overlays](lessons/sensor-equipment.md) takes about five
minutes in the browser. Hide sonar, detach it, and compare the two operations.
Then trace one measurement through the C code. No training or source edits needed.

## Build an edit

Keep the same seed/settings while comparing before and after. On macOS, the
native build needs Xcode command-line tools and Homebrew libomp. Linux builds
need Clang, OpenMP and OpenGL development files. Run a focused preflight:

```sh
uv run scripts/doctor.py --target native
./scripts/check.sh
./build/maplab --seed=73 --biome=1
```

The smoke viewer is for native development; its controller mode depends on the
build. The packaged browser build is the simplest way to see the released PPO.
For browser source edits, install the pinned SDK using [WEB.md](WEB.md#install-the-compiler-once):

```sh
uv run scripts/doctor.py --target web
uv run scripts/build_fleet_site.py
uv run scripts/check_maplab.py --artifacts-only
```

Reload your local Map Lab. This build downloads and verifies the released five
policies if needed. The artifact check is appropriate for a presentation edit;
simulation changes need the relevant checks in [AGENTS.md](../AGENTS.md#choose-validation-for-the-change).
See [the release workflow](WEB.md#publish-from-main) before publishing artifacts.

## Training

PufferLib's native C/CUDA trainer runs on a Linux NVIDIA host; macOS supports
editing, builds and inference. Use your own host and SSH configuration. No
maintainer GPU access is required or included with this repository.

Run `uv run scripts/doctor.py --target train` on that host, then follow
[DEVELOPMENT.md](DEVELOPMENT.md) and [SHARED_NAVIGATION.md](SHARED_NAVIGATION.md).
Start with a bounded smoke run and evaluate before increasing the budget.
The current development contract is 3; published policies use contract 2.
Never silently load a checkpoint under a different contract. Published timings
are measurements of specific past runs, not a promise for a new experiment.

## How the pieces fit

| Piece | Its job | Read next |
| --- | --- | --- |
| C simulation | Generate terrain, move bodies, resolve collisions | [Map Lab](MAPLAB.md) |
| A* + route tracker | Plan routes and supply basic steering | [Shared navigation](SHARED_NAVIGATION.md) |
| Sensors + PPO | Measure surroundings and choose local control adjustments | [Sensors](SENSORS.md) |
| Flecs | Store live unit components | [Flecs](FLECS.md) |
| Raylib + WebAssembly | Render and run the simulation/inference in the browser | [Web guide](WEB.md) |
| PufferLib 5 | Collect shared-world experience and train five family policies | [Training results](runs/shared-navigation-2026-09-16.md) |

## If something gets stuck

- **Missing uv:** use its installation guide above, then reopen your terminal.
- **Blank page from a file URL:** use the HTTP server; do not open HTML with `file://`.
- **Runtime still loading:** allow generation to finish. Check WebGL 2 support and
  browser console errors if it fails. `doctor.py` verifies packaged artifact hashes.
- **My source edit is invisible:** rebuild, then reload. Serving files does not compile.
- **Units stop or restart:** navigation is experimental. Demo patrols restart failed
  or stuck episodes; pauses and manual missions have different lifecycle behavior.
- **No GPU:** every browser experiment and the pretrained preview still works.

For contribution instructions, see [CONTRIBUTING.md](../CONTRIBUTING.md).
