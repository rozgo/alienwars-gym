# Raylib web build and GitHub Pages

The public experience is **AlienWars Map Lab**, built with Raylib and
Emscripten. Generation, simulation, sensors and policy inference execute locally in
WebAssembly; Raylib renders through WebGL 2. The separate Navigation Lab runs
the trained scout checkpoint in the browser. Native CUDA training runs on the
GPU machine; the public Training Observatory serves recorded runs and evaluations.

- Public repository: <https://github.com/rozgo/alienwars-gym>
- Site: <https://rozgo.github.io/alienwars-gym/>
- Trained navigation: <https://rozgo.github.io/alienwars-gym/navigation/?kind=1>
- Recorded training: <https://rozgo.github.io/alienwars-gym/training/>
- Showcase: <https://rozgo.github.io/alienwars-gym/demo/>
- Pages source: **Deploy from a branch → `main` → `/docs`**
- Authored interface: `web/maplab/shell.html`
- Authored entry point: `docs/index.html`
- Build: `scripts/build_maplab.sh`
- Check: `uv run scripts/check_maplab.py`
- Compiled output: `docs/maplab/index.html`, `maplab.js`, `maplab.wasm`,
  and `build.json`

The root page redirects to `maplab/`, retaining the query string and fragment.
Existing `/maplab/` seed URLs remain valid. The former bootstrap demo, its
packaged weights and its dedicated build/playback/training helpers are retired.
Share links also preserve patrol traffic with `patrol=0` (live, the default),
`patrol=1` (paused), or `patrol=2` (hidden).

## Navigation and training release

`scripts/build_navigation_site.py` packages the exact evaluated checkpoint from
`web/navigation/release.json`, builds the Raylib/WASM Navigation Lab, and exports
the listed training logs and held-out summaries as static JSON. It rejects a
checkpoint whose SHA-256 differs from the release selection. Place the released
`final-s173.bin` in ignored `outputs/navigation/checkpoints/`; keep the listed
training JSONL under `logs/alienwars/` and final evaluation JSONL under
`outputs/navigation/evaluations/final-s173/` when rebuilding recorded data.

```sh
uv run scripts/build_navigation_site.py
```

Generated `docs/navigation/` includes the small evaluated policy in the
Emscripten data bundle. `docs/training/` contains the recorded metrics and
evaluation summaries, with local checkpoint paths reduced to filenames.
Both directories and their manifests are deliberate Pages release artifacts.
The live local dashboard still uses its Python API; the public dashboard uses
relative static data URLs and identifies its recorded runs. Optional replay
queries remain available. Navigation runtime URLs carry their artifact digests.

The September 18 fleet candidate did not pass its navigation release gates.
Publish its measurements independently of the deployed checkpoint manifest:

```sh
uv run scripts/build_fleet_report.py \
  docs/runs/navigation-reliability-2026-09-18.json --name reliability
```

This generates `docs/training/reliability.html` and its static JSON without
rebuilding Map Lab or replacing its five public policies. The page identifies
candidate results explicitly; experimental weights are prerelease assets.

`docs/demo/` is an authored video page with a small poster. Its MP4 is a GitHub
Release asset, alongside the selected checkpoint, configuration and training
data bundle. Raw videos, logs and checkpoints remain outside Git history.
Commit authored source before rebuilding, check browser inference and static
data on a project subpath, then commit the generated release artifacts.

## Install the compiler once

The verified build uses **Emscripten 6.0.9** and **Raylib 5.5**. Keep the SDK
isolated in ignored `.local/`; the build wrapper discovers it automatically.

```sh
git clone --depth 1 https://github.com/emscripten-core/emsdk.git .local/emsdk
./.local/emsdk/emsdk install 6.0.9
./.local/emsdk/emsdk activate 6.0.9
```

Use an existing SDK if present. The native build uses macOS `libomp`; install
it if the dependency lookup fails. Node.js and Python 3 are used for validation.
No Python ML packages or npm application dependencies are needed.

## Build and inspect

```sh
./build.sh alienwars build/maplab --cpu --debug
./scripts/build_maplab.sh
uv run scripts/check_maplab.py
uv run python -m http.server 8781 --bind 127.0.0.1 --directory docs
```

Open <http://127.0.0.1:8781/>. The checker compares native and WASM generation,
navigation, mesh, occlusion, detail and patrol results; checks packaged viewer
output; and verifies artifact hashes and JavaScript syntax. Inspect the browser
for loading, controls, terrain joins, tunnel visibility, patrols and layout.
See [the Map Lab contract](MAPLAB.md) for the full validation scope.

Drag pans, Shift-drag orbits, and right-drag also pans. The canvas captures a
drag through release over the sidebar; cancellation and focus loss release any
held mouse buttons. Browser wheel input preserves pixel deltas, normalizes line
and page units, and handles Chrome's Ctrl+wheel pinch without page zoom or a
second GLFW wheel update. `camera_zoom.h` applies logarithmic scale changes with
resistance at 16/280 and at most about 13% elastic stretch, then settles to the
limit when input stops. Focus/reset controls clear pending zoom. Run
`uv run scripts/check_camera.py` for input-boundary and native/WASM zoom checks;
also review drag release and wheel behavior in Chrome.

## Publish from main

Commit authored source first, then rebuild so `docs/maplab/build.json` records
a clean source revision. Stage only the intended compiled artifacts and related
documentation. Other build products remain ignored.

```sh
./scripts/build_maplab.sh
uv run scripts/check_maplab.py
git add docs/maplab/index.html docs/maplab/maplab.js docs/maplab/maplab.wasm docs/maplab/build.json
git diff --cached --check
git commit -m "Publish verified Map Lab build"
git push origin main
```

For presentation-only changes, matching compiled JS/WASM bytes against the
previous validated release plus artifact, syntax and browser checks is sufficient.

Inspect deployment with:

```sh
gh api repos/rozgo/alienwars-gym/pages
gh api repos/rozgo/alienwars-gym/pages/builds/latest
```

After Pages completes, open the root URL and an existing `/maplab/` seed URL.
Verify the root HTML and compiled public files against the local checkout and
`docs/maplab/build.json`. Runtime URLs include a content digest so a new page
loads the matching JavaScript and WASM. Existing tabs need a reload to update.
Keep asset URLs relative to support the `/alienwars-gym/` project path.
`.nojekyll` serves compiled files directly; no custom server or cross-origin
isolation headers are required by this single-threaded build.

## Flecs Explorer webview

The Map Lab header opens the official Flecs Explorer against its live world.
`?explorer=1` opens it on load. The fleet build also runs
`scripts/build_explorer.py`, packaging the pinned frontend into `docs/explorer/`.
These are deliberate Pages artifacts. The bridge is same-origin, in-process and
read-only; no REST port or service is deployed. See [FLECS.md](FLECS.md).
