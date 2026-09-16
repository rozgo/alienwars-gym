#!/usr/bin/env python3
"""Renderer-independent sensor contracts, native/WASM parity and CPU cost."""
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
Path('build').mkdir(exist_ok=True)

def run(args):
    return subprocess.check_output(args, text=True, timeout=240).strip()

emcc = shutil.which('emcc') or str(ROOT / '.local/emsdk/upstream/emscripten/emcc')
source = 'tests/alienwars/sensor_test.c'
run(['clang', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
     '-fno-omit-frame-pointer', '-I.', source, '-lm', '-o', 'build/sensor-test'])
native = run(['build/sensor-test'])
print(native, flush=True)
run([emcc, '-std=c11', '-O3', '-I.', source, '-lm', '-o', 'build/sensor-test.js',
     '-sSTACK_SIZE=1MB', '-sINITIAL_MEMORY=64MB', '-sALLOW_MEMORY_GROWTH=1',
     '-sASSERTIONS=1', '-sENVIRONMENT=node'])
wasm = run(['node', 'build/sensor-test.js'])
assert native == wasm, 'Sensor contract checks disagree'
print(wasm, flush=True)
run(['clang', '-std=c11', '-O3', '-I.', source, '-lm', '-o', 'build/sensor-bench'])
native_bench = run(['build/sensor-bench', '--bench'])
wasm_bench = run(['node', 'build/sensor-test.js', '--bench'])
print(native_bench, flush=True)
print(wasm_bench, flush=True)
native_metrics = dict(re.findall(r'(\w+)=([^\s]+)', native_bench))
wasm_metrics = dict(re.findall(r'(\w+)=([^\s]+)', wasm_bench))
for key in ['worlds', 'steps', 'rays', 'samples', 'obs_floats']:
    assert native_metrics[key] == wasm_metrics[key], key
report = {'sensor_version': 1, 'generator_version': 11,
          'platform': platform.system() + ' ' + platform.machine(),
          'compiler': run(['clang', '--version']).splitlines()[0],
          'node': run(['node', '--version']), 'native': native, 'wasm': wasm,
          'native_benchmark': native_metrics, 'wasm_benchmark': wasm_metrics,
          'scope': 'Sensor sampling and observation packing only. 12 units, 3 generated worlds, 1800 steps at 60 Hz, including an underground scout. Generation, route/pose preparation, rendering and training are excluded. Finite-observation checks are included. Optimized native timing uses process CPU time; Node timing is Emscripten clock(). No CUDA throughput claim.'}
Path('build/sensor-check.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS: sensors in native C and WASM', flush=True)
