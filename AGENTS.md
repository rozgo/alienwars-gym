# Working on AlienWars Gym

## Start here

Read `git status` and preserve unrelated work. User instructions take precedence.
For onboarding, use [START_HERE.md](docs/START_HERE.md) and the repository skill
[alienwars-start](.agents/skills/alienwars-start/SKILL.md). Read only the subsystem
docs needed for the task. Commands below run from the repository root.

Write learning material in a teacher's voice: explain what the reader will do,
what to observe and why it happens. Define unfamiliar terms when first used.
Use concrete explanations instead of marketing slogans or motivational filler.

```sh
uv sync --locked
uv run scripts/doctor.py                       # Preview prerequisites; no installs
uv run python -m http.server 8781 --bind 127.0.0.1 --directory docs
```

Open `http://127.0.0.1:8781/learn/`. The committed browser build runs without
compilers or a GPU. Serving it does not rebuild source edits. To change the app,
check `uv run scripts/doctor.py --target web`, then follow [WEB.md](docs/WEB.md).

## What this project is

- PufferLib 5 native C/CUDA trains five PPO family policies in shared worlds;
  twelve biological unit types have individual destinations and recurrent state.
- A* supplies global routes; a route tracker and learned overrides control local
  navigation. Flecs owns live state. Raylib renders; WebAssembly runs the browser.
- Public policies are contract 2. Contract 3 candidates failed release gates;
  equal tensor dimensions do not imply compatible checkpoint semantics.
- Automatic viewer patrols restart failed/stalled episodes. This is demo lifecycle,
  not learned recovery; disable it for training/evaluation. Count restarts separately.
- Combat, cultivation and faction self-play are planned. All beings, craft and
  infrastructure are biological; no humans, pilots, crew or riders. See
  [BIOLOGICAL_WARFARE.md](docs/BIOLOGICAL_WARFARE.md).

## Find the right code

| Task | Code / source of truth |
| --- | --- |
| Terrain, Wave Function Collapse, caves, ocean | `ocean/alienwars/`; [MAPLAB.md](docs/MAPLAB.md) |
| Unit physics, missions, five-family training | `vehicle_profiles.h`, `vehicles.h`, `missions.h`, `command_fleet.h`, `ocean/alienwars_shared/`; [SHARED_NAVIGATION.md](docs/SHARED_NAVIGATION.md) |
| Sensors and policy observations | `sensors.h`, `sensor_rays.h`; [SENSORS.md](docs/SENSORS.md) |
| Flecs ownership / inspection | `flecs_runtime.c`; [FLECS.md](docs/FLECS.md) |
| Rendering and biological assets | `render.h`, `props.h`, `shaders.h`, `resources/alienwars/art/`; [ART_PIPELINE.md](docs/ART_PIPELINE.md) |
| Browser controls / Pages | `web/maplab/shell.html`; [WEB.md](docs/WEB.md) |
| Guided experiments | `docs/learn/`, `docs/START_HERE.md`, `docs/lessons/` |
| Native trainer interface / configuration | `src/pufferenv.h`, `ocean/minimal/`, `config/`; [DEVELOPMENT.md](docs/DEVELOPMENT.md) |

Header names above are relative to `ocean/alienwars/`. `alienwars_local` and the
single-scout Navigation Lab are historical tasks; preserve their provenance.
The retired Breakout bootstrap is not part of the AlienWars site.

## Essential constraints

Read [ENGINEERING_INVARIANTS.md](docs/ENGINEERING_INVARIANTS.md) before changing
simulation or rendering; select the relevant sections rather than loading all docs.

- Rendering, collision, support and sensing must agree on terrain geometry.
  Preserve tile continuity, seeded variety, symmetry, clearance and reachable routes.
- Preserve allocation-free step/reset, fixed buffers and deterministic seeds.
  Use `aw_mission_world_init/reset/close`; never copy or memset a live owning Flecs
  world. `AwSensors` borrows storage. Link `flecs_runtime.c` in every fleet consumer.
- Keep +Z forward, variant-specific hulls and physical motion. Wings must keep
  forward airspeed; quadcopters can hover/strafe; boats/subs respect water depth.
- Sensor equipment changes observations; overlays only change rendering.
  Keep observation/action layouts and recurrent resets compatible with checkpoints.
- Explorer reflection is viewer-only and read-only. Keep it out of training.
- Use `uv run` with the committed Python pin and lock; core tooling is standard
  library only. Art tooling uses `uv run --group art`; Blender uses its interpreter.
  Keep the native C/CUDA training path; do not introduce a replacement Python trainer.

## Choose validation for the change

| Changed area | Relevant checks |
| --- | --- |
| Docs, skills, Learn page, doctor | `uv run scripts/check_onboarding.py`; verify changed UI in a browser |
| Native environment setup | `./scripts/check.sh` (ASan/UBSan + bounded headless smoke) |
| Terrain, caves, bridges, ocean | `uv run scripts/check_maplab.py`; inspect actual terrain joins |
| Vehicle physics / motion | `uv run scripts/check_vehicles.py`; `uv run scripts/check_motion.py` |
| Sensors | `uv run scripts/check_sensors.py`; shared checks if policy inputs change |
| Missions / policy inference | `uv run scripts/check_shared.py`; bounded no-respawn endurance |
| Flecs ownership / Explorer | `uv run scripts/check_flecs.py`; `uv run scripts/check_explorer.py` |
| Camera input | `uv run scripts/check_camera.py`; Chrome drag-release / zoom checks |
| Presentation-only Map Lab | Rebuild; `uv run scripts/check_maplab.py --artifacts-only`; browser inspection |
| PPO / shared trainer | [GPU workflow](docs/DEVELOPMENT.md), CUDA gather and single-policy regression, finite bounded training, held-out evaluation |

Run relevant checks once; broaden for failures or unresolved risks. Passing a build
is not proof of learning. Record source, seeds, backend, actual steps, timings and
checkpoint hashes. Keep training/selection/test seeds distinct and compare a baseline.
At each RL iteration assess self-play explicitly; see [reliability](docs/NAVIGATION_RELIABILITY.md).

## Git and publishing

`origin` is this project; preserve upstream history, MIT license and asset notices.
Use isolated GPU checkouts, do not change source under jobs or stop unrelated work.
Machine access belongs in ignored `.local/` notes, never committed docs or skills.
Raw logs, checkpoints and SDKs stay ignored; reports belong in `docs/runs/`.

GitHub Pages uses `main:/docs`. Keep the root redirect and seed URLs working.
`docs/learn/`, `docs/demo/` and the root redirect are authored pages.
`docs/maplab/` and `docs/explorer/` are deliberate generated artifacts: edit their
sources, not their output. For Map Lab releases, commit authored source, build
with `uv run scripts/build_fleet_site.py` (verified public policies), validate and
inspect Chrome, then commit generated artifacts. Verify deployment and public
hashes after pushing. Never replace evaluated public policies with a fallback.
Follow [CONTRIBUTING.md](CONTRIBUTING.md) for a small, reviewable contribution.
