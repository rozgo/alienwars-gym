# Raylib web build and GitHub Pages

The public experience is **AlienWars Map Lab**, built with Raylib and
Emscripten. Generation, simulation and scripted patrols execute locally in
WebAssembly; Raylib renders through WebGL 2. AlienWars training is not yet
implemented, and future CUDA training runs on the GPU machine.

- Public repository: <https://github.com/rozgo/alienwars-gym>
- Site: <https://rozgo.github.io/alienwars-gym/>
- Pages source: **Deploy from a branch → `main` → `/docs`**
- Authored interface: `web/maplab/shell.html`
- Authored entry point: `docs/index.html`
- Build: `scripts/build_maplab.sh`
- Check: `python3 scripts/check_maplab.py`
- Compiled output: `docs/maplab/index.html`, `maplab.js`, `maplab.wasm`,
  and `build.json`

The root page redirects to `maplab/`, retaining the query string and fragment.
Existing `/maplab/` seed URLs remain valid. The former bootstrap demo, its
packaged weights and its dedicated build/playback/training helpers are retired.
Share links also preserve patrol traffic with `patrol=0` (live, the default),
`patrol=1` (paused), or `patrol=2` (hidden).

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
python3 scripts/check_maplab.py
python3 -m http.server 8781 --bind 127.0.0.1 --directory docs
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
`python3 scripts/check_camera.py` for input-boundary and native/WASM zoom checks;
also review drag release and wheel behavior in Chrome.

## Publish from main

Commit authored source first, then rebuild so `docs/maplab/build.json` records
a clean source revision. Stage only the intended compiled artifacts and related
documentation. Other build products remain ignored.

```sh
./scripts/build_maplab.sh
python3 scripts/check_maplab.py
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
