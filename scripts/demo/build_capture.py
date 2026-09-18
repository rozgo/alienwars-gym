#!/usr/bin/env python3
"""Build a local filming copy; leave production source and Pages untouched."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/demo-web'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'ocean/alienwars/alienwars.c').read_text()
anchor='    camera.target=focus;'
assert source.count(anchor)==1
source=source.replace(anchor,'    aw_demo_camera(dt);\n'+anchor)
source='static void aw_demo_camera(float dt);\n'+source+'\n'+(ROOT/'scripts/demo/capture_camera.inc').read_text()
(OUT/'viewer.c').write_text(source)

# Give thin RF links enough coverage to survive 1080p video compression. All
# endpoints still come from the unmodified cached bearing/range measurements.
sensor=(ROOT/'ocean/alienwars/sensor_render.h').read_text()
old='DrawLine3D(Vector3Lerp(origin,end,k/12.0f),Vector3Lerp(origin,end,(k+1)/12.0f),aw_sensor_color(type,opacity*(.35f+.5f*r->peers[j].strength)))'
new='DrawCylinderEx(Vector3Lerp(origin,end,k/12.0f),Vector3Lerp(origin,end,(k+1)/12.0f),.16f,.16f,5,aw_sensor_color(type,opacity*(.75f+.25f*r->peers[j].strength)))'
assert sensor.count(old)==1
sensor=sensor.replace(old,new).replace('id==selected?1:.42f','id==selected?1:.78f')
sensor=sensor.replace('type,.30f*opacity','type,.50f*opacity').replace('type,.28f*(1-pulse)*opacity','type,.45f*(1-pulse)*opacity')
(OUT/'sensor_render.h').write_text(sensor)
command=[str(ROOT/'.local/emsdk/upstream/emscripten/emcc'),str(OUT/'viewer.c'),'ocean/alienwars/flecs_runtime.c','-o',str(OUT/'index.html'),
    '-std=c11','-O3','-Wall','-Wextra','-Wno-unused-function','-I.','-Iocean/alienwars',
    '-Iraylib-5.5_webassembly/include','-Isrc','-Ivendor','raylib-5.5_webassembly/lib/libraylib.a',
    '-DPLATFORM_WEB','-DGRAPHICS_API_OPENGL_ES3','-sUSE_GLFW=3','-sUSE_WEBGL2=1',
    '-sMIN_WEBGL_VERSION=2','-sMAX_WEBGL_VERSION=2','-sALLOW_MEMORY_GROWTH=1',
    '-sINITIAL_MEMORY=64MB','-sSTACK_SIZE=1MB','-sASSERTIONS=1','-sENVIRONMENT=web,node',
    '--shell-file','web/maplab/shell.html','--preload-file','resources/alienwars/art@resources/alienwars/art']
subprocess.run(command,cwd=ROOT,check=True)
# The shell's locateFile hook uses the supplied asset version for both assets.
html=(OUT/'index.html').read_text().replace('__MAPLAB_ASSET_VERSION__','capture-v3')
(OUT/'index.html').write_text(html)
report={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
    'generator_version':10,'capture_only':True,'raylib':'5.5',
    'changes':['Frame-synchronous quintic camera orbit and damped follow',
               'Thicker cached RF links and stronger all-unit overlay opacity'],
    'artifacts':{p.name:{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}
                 for p in OUT.iterdir() if p.suffix in ['.c','.h','.wasm','.js','.html']}}
(OUT/'build.json').write_text(json.dumps(report,indent=2)+'\n')
print('Capture viewer:',OUT)
