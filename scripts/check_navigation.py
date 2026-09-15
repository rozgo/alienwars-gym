#!/usr/bin/env python3
"""Action-loop contract, shared-world immutability, and native/WASM parity."""
import json
import os
import re
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
Path('build').mkdir(exist_ok=True)
def run(args):
    return subprocess.check_output(args, text=True, timeout=240).strip()

emcc = shutil.which('emcc') or str(ROOT / '.local/emsdk/upstream/emscripten/emcc')
source = 'tests/alienwars/nav_test.c'
run(['clang', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined', '-I.',
     source, '-lm', '-o', 'build/nav-test'])
native = run(['build/nav-test'])
run([emcc, '-std=c11', '-O2', '-I.', source, '-lm', '-o', 'build/nav-test.js',
     '-sASSERTIONS=1', '-sSTACK_SIZE=2MB', '-sINITIAL_MEMORY=64MB', '-sENVIRONMENT=node'])
wasm = run(['node', 'build/nav-test.js'])
# libm steering differences can move a threshold crossing by a few ticks.
# Require identical task outcomes/contacts and <1% total rollout-length drift.
normalize = lambda s: re.sub(r' steps=\d+', '', s)
assert normalize(native) == normalize(wasm), f'Native/WASM navigation mismatch:\n{native}\n{wasm}'
native_steps, wasm_steps = (int(re.search(r' steps=(\d+)', s)[1]) for s in (native, wasm))
assert abs(native_steps-wasm_steps) <= max(native_steps,wasm_steps)*.01
raylib = 'raylib-5.5_macos' if os.uname().sysname == 'Darwin' else 'raylib-5.5_linux_amd64'
run(['clang', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined', '-I.', '-Isrc',
     '-I'+raylib+'/include', '-DAW_NAV_HEADLESS',
     'tests/alienwars/nav_adapter_test.c', 'ocean/alienwars/nav_api.c', '-lm', '-o', 'build/nav-adapter-test'])
adapter = run(['build/nav-adapter-test'])
report = {'contract_version': 1, 'native': native, 'wasm': wasm, 'adapter': adapter}
Path('build/navigation-check.json').write_text(json.dumps(report, indent=2)+'\n')
print(native)
print('Native/WASM parity: PASS')
print(adapter)
