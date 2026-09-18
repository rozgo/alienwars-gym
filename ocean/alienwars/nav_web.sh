# Sourced by build.sh alienwars --web --rl. Explicit policy; no fallback weights.
mkdir -p build/web/alienwars-nav
cp web/navigation/theme.css build/web/alienwars-nav/theme.css
NAV_PRELOAD=(--preload-file resources/alienwars/art@resources/alienwars/art
             --preload-file config/default.ini@config/default.ini
             --preload-file config/alienwars.ini@config/alienwars.ini)
if [ -n "${AW_NAV_MODEL:-}" ]; then
    test -f "$AW_NAV_MODEL"
    NAV_PRELOAD+=(--preload-file "$AW_NAV_MODEL@policy.bin")
fi
emcc ocean/alienwars/nav_viewer.c ocean/alienwars/nav_api.c \
    -o build/web/alienwars-nav/index.html -std=c11 -O3 \
    -I. -Isrc -Iocean/alienwars "${INCLUDES[@]}" "${LINK_ARCHIVES[@]}" \
    -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES3 \
    -sUSE_GLFW=3 -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
    -sASYNCIFY --js-library vendor/puf_web_vsync.js \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB -sSTACK_SIZE=2MB \
    -sASSERTIONS=1 -sENVIRONMENT=web \
    --shell-file web/navigation/shell.html "${NAV_PRELOAD[@]}"
python3 - <<'PY'
import hashlib,json,os,subprocess
from pathlib import Path
root=Path('build/web/alienwars-nav')
model=os.environ.get('AW_NAV_MODEL')
manifest={'contract':1,'checkpoint':Path(model).name if model else None,
          'checkpoint_sha256':hashlib.sha256(Path(model).read_bytes()).hexdigest() if model else None,
          'hidden':int(os.environ.get('AW_NAV_HIDDEN','128')),
          'layers':int(os.environ.get('AW_NAV_LAYERS','2')),
          'source':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
          'dirty':bool(subprocess.check_output(['git','status','--porcelain','--','resources/alienwars/art','scripts/art','build.sh','ocean/alienwars','src','vendor','config','web/navigation']))}
if os.environ.get('AW_NAV_PUBLIC')=='1':
    manifest['site_links']={'maplab':'../maplab/','training':'../training/','demo':'../demo/'}
manifest['files']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir() if p.suffix in ('.html','.css','.js','.wasm','.data')}
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
PY
echo 'Built: build/web/alienwars-nav/index.html'
