#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v emcc >/dev/null && [ -f .local/emsdk/emsdk_env.sh ]; then
    source .local/emsdk/emsdk_env.sh >/dev/null 2>&1
fi
command -v emcc >/dev/null || { echo 'Install Emscripten 6.0.9; see docs/WEB.md.' >&2; exit 2; }
./build.sh alienwars --web
mkdir -p docs/maplab
cp build/web/alienwars/maplab.html docs/maplab/index.html
cp build/web/alienwars/maplab.js build/web/alienwars/maplab.wasm docs/maplab/
if [ -f build/web/alienwars/maplab.data ]; then
    cp build/web/alienwars/maplab.data docs/maplab/
    chmod 644 docs/maplab/maplab.data
else
    rm -f docs/maplab/maplab.data
fi
chmod 644 docs/maplab/index.html docs/maplab/maplab.js docs/maplab/maplab.wasm
python3 - <<'PY'
import hashlib, json, pathlib, subprocess
paths=['resources/alienwars/art','scripts/art','build.sh','ocean/alienwars','vendor/flecs','vendor/flecs-explorer','web/explorer','scripts/build_explorer.py','web/maplab','scripts/build_maplab.sh','scripts/build_fleet_site.py','web/training']
dirty=bool(subprocess.check_output(['git','status','--porcelain','--',*paths]))
artifacts={}
for name in ['index.html','maplab.js','maplab.wasm','maplab.data']:
    if not (pathlib.Path('docs/maplab')/name).exists(): continue
    data=(pathlib.Path('docs/maplab')/name).read_bytes()
    artifacts[name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
report={'generator_version':11,'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
    'source_dirty':dirty,'emscripten':subprocess.check_output(['emcc','--version'],text=True).splitlines()[0],
    'raylib':'5.5','flecs':json.loads(pathlib.Path('vendor/flecs/version.json').read_text()),'explorer':json.loads(pathlib.Path('vendor/flecs-explorer/version.json').read_text()),'viewer_inspection':True,'artifacts':artifacts}
pathlib.Path('docs/maplab/build.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
PY

python3 scripts/build_explorer.py
