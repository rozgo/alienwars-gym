#!/bin/bash
# Build the trained baseline through PufferLib's existing Raylib/Emscripten path.
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v emcc >/dev/null && [ -f .local/emsdk/emsdk_env.sh ]; then
    source .local/emsdk/emsdk_env.sh >/dev/null 2>&1
fi
command -v emcc >/dev/null || { echo 'Install Emscripten 6.0.9; see docs/WEB.md.' >&2; exit 2; }
emcc --version | head -1
checkpoint="${1:-outputs/breakout-demo.bin}"
if [ ! -f "$checkpoint" ]; then
    if [ "$#" -gt 0 ]; then echo "Checkpoint not found: $checkpoint" >&2; exit 2; fi
    mkdir -p outputs
    curl --fail --location --retry 2 \
      https://github.com/rozgo/alienwars-gym/releases/download/breakout-baseline-20260914/breakout-demo.bin \
      -o "$checkpoint"
fi
python3 - "$checkpoint" <<'PY'
import hashlib, pathlib, sys
p = pathlib.Path(sys.argv[1])
expected = '868fb8b97a85946141bdc4a50b2e6bd20b0d9be3c4df114aaa1fb2aed95f08d4'
actual = hashlib.sha256(p.read_bytes()).hexdigest()
if actual != expected:
    raise SystemExit(f'Checkpoint SHA-256 mismatch: {actual}')
print(f'Verified trained policy: {p.stat().st_size:,} bytes, SHA-256 {actual}')
PY
staging="$PWD/build/pages-input"
mkdir -p "$staging/docs/assets/models" docs
cp "$checkpoint" "$staging/docs/assets/models/breakout_weights.bin"
PUFFER_WEBSITE_DIR="$staging" PUFFER_WEB_SHELL=web/shell.html ./build.sh breakout --web
cp build/web/breakout/game.html docs/index.html
for extension in js wasm data; do
    cp "build/web/breakout/game.$extension" "docs/game.$extension"
done
chmod 644 docs/index.html docs/game.js docs/game.wasm docs/game.data
touch docs/.nojekyll
python3 - <<'PY'
import hashlib, json, pathlib, subprocess
artifacts = {}
for name in ('index.html', 'game.js', 'game.wasm', 'game.data'):
    p = pathlib.Path('docs') / name
    if not p.is_file() or not p.stat().st_size:
        raise SystemExit(f'Missing web artifact: {p}')
    artifacts[name] = {'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
report = {
    'environment': 'breakout',
    'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
    'source_dirty': bool(subprocess.check_output(['git', 'diff', 'HEAD', '--', 'build.sh', 'src', 'ocean', 'config', 'web', 'scripts/build_web.sh'])),
    'emscripten': subprocess.check_output(['emcc', '--version'], text=True).splitlines()[0],
    'raylib': '5.5',
    'checkpoint_sha256': '868fb8b97a85946141bdc4a50b2e6bd20b0d9be3c4df114aaa1fb2aed95f08d4',
    'checkpoint_training_source': 'd4e2602c33cfbe98da17b2d7ceebea8874dd38a5',
    'artifacts': artifacts,
}
pathlib.Path('docs/web-build.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(artifacts, indent=2))
PY
echo 'Web build ready: docs/index.html. Serve docs/ over HTTP; publish main:/docs.'
