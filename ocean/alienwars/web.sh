# Sourced by build.sh --web. Optional development builds disclose reference
# controllers; a published policy release supplies all five family checkpoints.
rm -f build/web/alienwars/maplab.data
LOCAL_PRELOAD=(--preload-file resources/alienwars/art@resources/alienwars/art)
if [ -n "${AW_MISSION_MODELS:-}" ]; then
    for family in 0 1 2 3 4; do
        test -f "$AW_MISSION_MODELS/mission-$family.bin"
        LOCAL_PRELOAD+=(--preload-file "$AW_MISSION_MODELS/mission-$family.bin@resources/alienwars/mission-$family.bin")
    done
fi
# Flat files carry no semantic metadata. Select the contract explicitly, and
# reject a mismatch even though versions 2 and 3 have the same tensor shape.
if [ -n "${AW_MISSION_MODELS:-}" ]; then
    MODEL_CONTRACT=$(uv run python -c 'import json,sys,pathlib; p=pathlib.Path(sys.argv[1])/"contract.json"; print(json.loads(p.read_text())["contract"] if p.exists() else 2)' "$AW_MISSION_MODELS")
    test "${AW_MISSION_CONTRACT:-$MODEL_CONTRACT}" = "$MODEL_CONTRACT" || { echo 'Policy contract mismatch' >&2; exit 2; }
    AW_MISSION_CONTRACT=$MODEL_CONTRACT
else
    AW_MISSION_CONTRACT=${AW_MISSION_CONTRACT:-3}
fi
case "$AW_MISSION_CONTRACT" in 2|3) ;; *) echo 'Invalid navigation contract' >&2; exit 2 ;; esac
mkdir -p build/web/alienwars
printf '{"contract":%s}\n' "$AW_MISSION_CONTRACT" > build/web/alienwars/contract.json
LOCAL_PRELOAD+=(--preload-file build/web/alienwars/contract.json@resources/alienwars/contract.json)
emcc ocean/alienwars/alienwars.c ocean/alienwars/flecs_runtime.c -o build/web/alienwars/maplab.html \
    -std=c11 -O3 -DAW_NAV_VERSION="$AW_MISSION_CONTRACT" -Wall -Wextra -Wno-unused-function -DAW_FLECS_EXPLORER \
    -I. -Iocean/alienwars "${INCLUDES[@]}" "${LINK_ARCHIVES[@]}" \
    -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES3 \
    -sUSE_GLFW=3 -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=1MB \
    -sASSERTIONS=1 -sENVIRONMENT=web,node \
    -sEXPORTED_RUNTIME_METHODS=cwrap \
    --shell-file web/maplab/shell.html "${LOCAL_PRELOAD[@]}"
# Keep each page paired with its compiled runtime even when Pages or the
# browser still caches the previous build under the same asset filenames.
uv run python - <<'PY'
import hashlib,re
from pathlib import Path
root=Path('build/web/alienwars')
version=hashlib.sha256(b''.join((root/name).read_bytes() for name in ['maplab.js','maplab.wasm','maplab.data'] if (root/name).exists())).hexdigest()[:16]
page=root/'maplab.html'
html=page.read_text()
assert '__MAPLAB_ASSET_VERSION__' in html
html,count=re.subn(r'src=["\']?maplab\.js["\']?(?=[\s>])',f'src="maplab.js?v={version}"',html)
assert count==1
html=html.replace('__MAPLAB_ASSET_VERSION__',version)
page.write_text(html)
PY
echo 'Built: build/web/alienwars/maplab.html'
