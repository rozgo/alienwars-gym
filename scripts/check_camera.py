#!/usr/bin/env -S uv run
"""Check browser input boundaries and native/WASM zoom without rendering."""
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
Path('build').mkdir(exist_ok=True)
def run(args):
    return subprocess.check_output(args, text=True, timeout=120).strip()
source = 'tests/alienwars/camera_test.c'
emcc = shutil.which('emcc') or str(ROOT / '.local/emsdk/upstream/emscripten/emcc')
run(['clang', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined', '-I.',
     source, '-lm', '-o', 'build/camera-test'])
native = run(['build/camera-test'])
run([emcc, '-std=c11', '-O3', '-I.', source, '-lm', '-o', 'build/camera-test.js',
     '-sASSERTIONS=1', '-sENVIRONMENT=node'])
wasm = run(['node', 'build/camera-test.js'])
assert native == wasm, 'Native/WASM zoom checks disagree'
inputs = run(['node', 'tests/alienwars/input_test.cjs'])
report = {'native': native, 'wasm': wasm, 'input_events': inputs}
Path('build/camera-check.json').write_text(json.dumps(report, indent=2) + '\n')
for result in report.values():
    print(result)
