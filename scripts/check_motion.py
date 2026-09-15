#!/usr/bin/env python3
"""Validate rate-limited steering without launching a graphics context."""
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
source = 'tests/alienwars/motion_test.c'
emcc = shutil.which('emcc') or str(ROOT / '.local/emsdk/upstream/emscripten/emcc')
run(['clang', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined', '-I.',
     source, '-lm', '-o', 'build/motion-test'])
native = run(['build/motion-test'])
run([emcc, '-std=c11', '-O3', '-I.', source, '-lm', '-o', 'build/motion-test.js',
     '-sASSERTIONS=1', '-sENVIRONMENT=node'])
wasm = run(['node', 'build/motion-test.js'])
assert native == wasm, 'Native/WASM steering checks disagree'
report = {'motion_version': 1, 'native': native, 'wasm': wasm}
Path('build/motion-check.json').write_text(json.dumps(report, indent=2) + '\n')
print(native)
print(wasm)
