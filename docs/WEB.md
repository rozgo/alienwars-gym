# Raylib web build and GitHub Pages

The public browser experience uses **PufferLib's existing Raylib/Emscripten
path**. The first demo is the previously trained Breakout baseline. Simulation
and inference execute locally in WebAssembly; Raylib renders through WebGL 2.
The browser does not run CUDA training, and the game is not streamed video.

- Public repository: <https://github.com/rozgo/alienwars-gym>
- Site: <https://rozgo.github.io/alienwars-gym/>
- Pages source: **Deploy from a branch → `main` → `/docs`**
- Authored page: `web/shell.html`
- Build: `scripts/build_web.sh`
- Check: `python3 scripts/check_web.py`
- Published files: `docs/index.html`, `game.js`, `game.wasm`, `game.data`,
  `.nojekyll`, and `web-build.json`

## Install the compiler once

The verified build uses **Emscripten 6.0.9** and **Raylib 5.5**. Keep the SDK
isolated in ignored `.local/`; the build wrapper discovers it automatically.

```sh
git clone --depth 1 https://github.com/emscripten-core/emsdk.git .local/emsdk
./.local/emsdk/emsdk install 6.0.9
./.local/emsdk/emsdk activate 6.0.9
```

If the SDK already exists, use it rather than cloning over it. The current web
build inherits the upstream macOS `libomp` dependency lookup even though WASM
inference itself is single-threaded. Install `libomp` if that lookup fails.
Node.js and Python 3 are needed for the checks. No Python ML packages or npm
application dependencies are needed.

## Build the exact trained baseline

```sh
./scripts/build_web.sh
python3 scripts/check_web.py
python3 -m http.server 8781 --bind 127.0.0.1 --directory docs
```

Open <http://127.0.0.1:8781/>. The wrapper downloads the selected public release
checkpoint if it is absent and verifies SHA-256 before packaging. It supplies
the policy through the upstream `PUFFER_WEBSITE_DIR` asset mechanism, then runs:

```sh
PUFFER_WEBSITE_DIR="$PWD/build/pages-input" \
PUFFER_WEB_SHELL=web/shell.html ./build.sh breakout --web
```

`PUFFER_WEB_SHELL` is a small project addition; the default remains upstream's
shell. Published builds omit source maps. The simulation, policy architecture,
gameplay, action handling and rendering implementation are unchanged.

The policy is 64,768 bytes, with SHA-256
`868fb8b97a85946141bdc4a50b2e6bd20b0d9be3c4df114aaa1fb2aed95f08d4`.
The web package includes the matching configuration and shared Raylib assets.
No secrets or machine-specific GPU connection details are bundled.

The checker verifies artifact hashes and runs the exact WASM module in Node.js
for ten headless episodes, requiring loaded weights and the correct network
shape. This validates simulation/inference; inspect the browser separately for
loading, visible gameplay, keyboard control, restart, fullscreen and layout.
WASM and native CPU builds may use different C-library RNG implementations;
do not interpret their scores as exact cross-device equivalence.

## Publish from main

Commit authored source first, then rebuild so `web-build.json` records a clean
source revision. Stage only the deliberate generated artifacts and related
documentation. These modest compiled files are committed because branch-based
Pages hosting serves `main:/docs` directly; other build products stay ignored.

```sh
./scripts/build_web.sh
python3 scripts/check_web.py
git add docs/index.html docs/game.js docs/game.wasm docs/game.data \
  docs/.nojekyll docs/web-build.json
git diff --cached --check
git commit -m "Publish verified Raylib web build"
git push origin main
```

Pages configuration can be inspected with:

```sh
gh api repos/rozgo/alienwars-gym/pages
gh api repos/rozgo/alienwars-gym/pages/builds/latest
```

After the Pages build completes, open the public URL and verify the deployed
files against `web-build.json`, including the `application/wasm` content type.
Keep asset URLs relative so the `/alienwars-gym/` project path works. `.nojekyll`
serves the compiled files directly. No custom domain, server, credential or
cross-origin isolation headers are required by this single-threaded build.

## References

- [PufferLib web build](https://puffer.ai/docs.html)
- [Emscripten installation](https://emscripten.org/docs/getting_started/downloads.html)
- [GitHub Pages branch publishing](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)
