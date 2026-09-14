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

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
PAGES = ROOT / 'docs/maplab'
BUILD.mkdir(exist_ok=True)

def run(args, **kwargs):
    return subprocess.check_output(args, cwd=ROOT, text=True, timeout=90, **kwargs).strip()

emcc = shutil.which('emcc') or str(ROOT / '.local/emsdk/upstream/emscripten/emcc')
env = os.environ.copy()
if not shutil.which('emcc'):
    sdk = ROOT / '.local/emsdk'
    nodes = sorted((sdk / 'node').glob('*/bin'))
    if nodes:
        env['PATH'] = str(nodes[-1]) + os.pathsep + env.get('PATH', '')
    env['EMSDK'] = str(sdk)

source = 'tests/alienwars/map_test.c'
run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined',
     '-fno-omit-frame-pointer','-Wall','-Wextra','-I.',source,'-lm','-o','build/map-test'])
native = run(['build/map-test','256'])
print(native)
run([emcc,'-std=c11','-O3','-I.',source,'-lm','-o','build/map-test.js',
     '-sSTACK_SIZE=1MB','-sASSERTIONS=1','-sENVIRONMENT=node'],env=env)
wasm = run(['node','build/map-test.js','256'])
print(wasm)
assert native == wasm, 'Native and WASM seeded generation disagree'

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
assert 'maplab.js' in parser.scripts
(BUILD / 'maplab-inline.js').write_text('\n'.join(parser.inline))
run(['node','--check','build/maplab-inline.js'])
run(['node','--check','docs/maplab/maplab.js'])
matches = []
for seed in [0, 1, 73, 4294967295]:
    native_line = run(['build/maplab','--headless',f'--seed={seed}','--symmetry=0','--floor-a=10','--floor-b=3','--tunnels=1'])
    web_line = run(['node','docs/maplab/maplab.js','--headless',f'--seed={seed}','--symmetry=0','--floor-a=10','--floor-b=3','--tunnels=1'])
    assert native_line == web_line, (native_line, web_line)
    matches.append(dict(re.findall(r'(\w+)=([^\s]+)',native_line)))
report = {'generator_version':2,'native':native,'wasm':wasm,
          'packaged_viewer_seed_checks':matches,'artifact_hashes_match':True,
          'javascript_syntax':'passed'}
(BUILD / 'maplab-check.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: 256 native/WASM seeds, navigation invariants, deliberate failure cases, viewer parity, artifacts and JavaScript')
