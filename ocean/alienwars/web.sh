# Sourced by the upstream build.sh --web hook. No trained weights are required
# by the Map Lab; this viewer runs the shared procedural terrain core.
mkdir -p build/web/alienwars
emcc ocean/alienwars/alienwars.c -o build/web/alienwars/maplab.html \
    -std=c11 -O3 -Wall -Wextra -Wno-unused-function \
    -I. -Iocean/alienwars "${INCLUDES[@]}" "${LINK_ARCHIVES[@]}" \
    -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES3 \
    -sUSE_GLFW=3 -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=1MB \
    -sASSERTIONS=1 -sENVIRONMENT=web,node \
    --shell-file web/maplab/shell.html
# Keep each page paired with its compiled runtime even when Pages or the
# browser still caches the previous build under the same asset filenames.
python3 - <<'PY'
import hashlib,re
from pathlib import Path
root=Path('build/web/alienwars')
version=hashlib.sha256((root/'maplab.js').read_bytes()+(root/'maplab.wasm').read_bytes()).hexdigest()[:16]
page=root/'maplab.html'
html=page.read_text()
assert '__MAPLAB_ASSET_VERSION__' in html
html,count=re.subn(r'src=["\']?maplab\.js["\']?(?=[\s>])',f'src="maplab.js?v={version}"',html)
assert count==1
html=html.replace('__MAPLAB_ASSET_VERSION__',version)
page.write_text(html)
PY
echo 'Built: build/web/alienwars/maplab.html'
