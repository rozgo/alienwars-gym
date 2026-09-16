#!/usr/bin/env python3
"""Native/WASM seed parity, map invariants and compiled page checks."""
import hashlib
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
PAGES = ROOT / 'docs/maplab'
BUILD.mkdir(exist_ok=True)

def run(args, **kwargs):
    return subprocess.check_output(args, cwd=ROOT, text=True, timeout=600, **kwargs).strip()

emcc = shutil.which('emcc') or str(ROOT / '.local/emsdk/upstream/emscripten/emcc')
env = os.environ.copy()
if not shutil.which('emcc'):
    sdk = ROOT / '.local/emsdk'
    nodes = sorted((sdk / 'node').glob('*/bin'))
    if nodes:
        env['PATH'] = str(nodes[-1]) + os.pathsep + env.get('PATH', '')
    env['EMSDK'] = str(sdk)

artifacts_only = '--artifacts-only' in sys.argv
if not artifacts_only:
    source = 'tests/alienwars/map_test.c'
    run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined',
         '-fno-omit-frame-pointer','-Wall','-Wextra','-I.',source,'-lm','-o','build/map-test'])
    native = run(['build/map-test','256'])
    print(native)
    run([emcc,'-std=c11','-O3','-I.',source,'-lm','-o','build/map-test.js',
         '-sSTACK_SIZE=1MB','-sINITIAL_MEMORY=64MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
    wasm = run(['node','build/map-test.js','256'])
    print(wasm)
    assert native == wasm, 'Native and WASM seeded generation disagree'

    diversity_source = 'tests/alienwars/diversity_test.c'
    run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',diversity_source,'-lm','-o','build/diversity-test'])
    diversity_native = run(['build/diversity-test'])
    run([emcc,'-std=c11','-O3','-I.',diversity_source,'-lm','-o','build/diversity-test.js',
         '-sSTACK_SIZE=1MB','-sINITIAL_MEMORY=64MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
    diversity_wasm = run(['node','build/diversity-test.js'])
    assert diversity_native == diversity_wasm, 'Native/WASM global layout diversity disagree'
    print(diversity_native)

    mountain_source = 'tests/alienwars/mountain_test.c'
    run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',mountain_source,'-lm','-o','build/mountain-test'])
    mountain_native = run(['build/mountain-test'])
    run([emcc,'-std=c11','-O3','-I.',mountain_source,'-lm','-o','build/mountain-test.js',
         '-sSTACK_SIZE=1MB','-sINITIAL_MEMORY=64MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
    mountain_wasm = run(['node','build/mountain-test.js'])
    assert mountain_native == mountain_wasm, 'Native/WASM mountain topology and traversal disagree'
    print(mountain_native)

    relief_source = 'tests/alienwars/relief_test.c'
    run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',relief_source,'-lm','-o','build/relief-test'])
    relief_native = run(['build/relief-test'])
    run([emcc,'-std=c11','-O3','-I.',relief_source,'-lm','-o','build/relief-test.js',
         '-sSTACK_SIZE=1MB','-sINITIAL_MEMORY=64MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
    relief_wasm = run(['node','build/relief-test.js'])
    assert relief_native == relief_wasm, 'Native/WASM hills and bridge navigation disagree'
    print(relief_native)

    volume_source = 'tests/alienwars/volume_test.c'
    run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',volume_source,'-lm','-o','build/volume-test'])
    volume_native = run(['build/volume-test'])
    run([emcc,'-std=c11','-O3','-I.',volume_source,'-lm','-o','build/volume-test.js',
         '-sSTACK_SIZE=1MB','-sINITIAL_MEMORY=64MB','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
    volume_wasm = run(['node','build/volume-test.js'])
    assert volume_native == volume_wasm, 'Native/WASM volumetric meshing disagree'
    print(volume_native)

    occlusion_source = 'tests/alienwars/occlusion_test.c'
    run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',occlusion_source,'-lm','-o','build/occlusion-test'])
    occlusion_native = run(['build/occlusion-test'])
    run([emcc,'-std=c11','-O3','-I.',occlusion_source,'-lm','-o','build/occlusion-test.js',
         '-sSTACK_SIZE=1MB','-sINITIAL_MEMORY=64MB','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
    occlusion_wasm = run(['node','build/occlusion-test.js'])
    assert occlusion_native == occlusion_wasm, 'Native/WASM occlusion fixtures disagree'
    print(occlusion_native)

    detail_source = 'tests/alienwars/detail_test.c'
    run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',detail_source,'-lm','-o','build/detail-test'])
    detail_native = run(['build/detail-test'])
    run([emcc,'-std=c11','-O3','-I.',detail_source,'-lm','-o','build/detail-test.js',
         '-sSTACK_SIZE=1MB','-sINITIAL_MEMORY=64MB','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
    detail_wasm = run(['node','build/detail-test.js'])
    assert detail_native == detail_wasm, 'Native/WASM detail map fixtures disagree'
    print(detail_native)

    patrol_source = 'tests/alienwars/patrol_test.c'
    run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',patrol_source,'-lm','-o','build/patrol-test'])
    patrol_native = run(['build/patrol-test'])
    run([emcc,'-std=c11','-O3','-I.',patrol_source,'-lm','-o','build/patrol-test.js',
         '-sSTACK_SIZE=1MB','-sINITIAL_MEMORY=64MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
    patrol_wasm = run(['node','build/patrol-test.js'])
    assert patrol_native == patrol_wasm, 'Native/WASM patrol navigation disagrees'
    print(patrol_native)

manifest = json.loads((PAGES / 'build.json').read_text())
for name, expected in manifest['artifacts'].items():
    blob = (PAGES / name).read_bytes()
    assert len(blob) == expected['bytes'], name
    assert hashlib.sha256(blob).hexdigest() == expected['sha256'], name
    assert str(ROOT).encode() not in blob, f'Local path embedded in {name}'

class Assets(HTMLParser):
    def __init__(self):
        super().__init__()
        self.scripts = []
        self.inline = []
        self.in_script = False

    def handle_starttag(self, tag, attrs):
        if tag != 'script':
            return
        src = dict(attrs).get('src')
        if src:
            self.scripts.append(src)
        self.in_script = not src

    def handle_endtag(self, tag):
        if tag == 'script':
            self.in_script = False

    def handle_data(self, data):
        if self.in_script:
            self.inline.append(data)

html = (PAGES / 'index.html').read_text()
assert '{{{ SCRIPT }}}' not in html
parser = Assets()
parser.feed(html)
asset_version = hashlib.sha256(b''.join((PAGES / name).read_bytes() for name in ['maplab.js','maplab.wasm','maplab.data'] if (PAGES/name).exists())).hexdigest()[:16]
assert f'maplab.js?v={asset_version}' in parser.scripts
assert f'?v={asset_version}' in '\n'.join(parser.inline) and '__MAPLAB_ASSET_VERSION__' not in html
(BUILD / 'maplab-inline.js').write_text('\n'.join(parser.inline))
run(['node','--check','build/maplab-inline.js'])
run(['node','--check','docs/maplab/maplab.js'])
matches = []
for seed in [0, 1, 73, 4294967295]:
    native_line = run(['build/maplab','--headless',f'--seed={seed}','--symmetry=0','--floor-a=10','--floor-b=3','--tunnels=1'])
    web_line = run(['node','docs/maplab/maplab.js','--headless',f'--seed={seed}','--symmetry=0','--floor-a=10','--floor-b=3','--tunnels=1'])
    assert native_line == web_line, (native_line, web_line)
    matches.append(dict(re.findall(r'(\w+)=([^\s]+)',native_line)))
if artifacts_only:
    report = {'generator_version':10,'scope':'packaged artifacts only','packaged_viewer_seed_checks':matches,'artifact_hashes_match':True,'javascript_syntax':'passed'}
else:
    report = {'generator_version':10,'native':native,'wasm':wasm,'volume_native':volume_native,'volume_wasm':volume_wasm,
              'mountain_native':mountain_native,'mountain_wasm':mountain_wasm,
              'relief_native':relief_native,'relief_wasm':relief_wasm,
              'patrol_native':patrol_native,'patrol_wasm':patrol_wasm,
              'detail_native':detail_native,'detail_wasm':detail_wasm,
              'occlusion_native':occlusion_native,'occlusion_wasm':occlusion_wasm,
              'diversity_native':diversity_native,'diversity_wasm':diversity_wasm,
              'packaged_viewer_seed_checks':matches,'artifact_hashes_match':True,
              'javascript_syntax':'passed'}
(BUILD / 'maplab-check.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: packaged viewer, artifact hashes and JavaScript' if artifacts_only else 'PASS: 256 native/WASM seeds, 48 same-settings diversity worlds, mountain WFC / walkable spans, closed lakes/ocean boundary, cave clearance, manifold meshing, baked occlusion, seamless detail maps, deliberate failures, viewer parity, artifacts and JavaScript')
