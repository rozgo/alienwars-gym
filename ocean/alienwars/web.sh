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
echo 'Built: build/web/alienwars/maplab.html'
