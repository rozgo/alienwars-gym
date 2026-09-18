#!/usr/bin/env python3
"""Check native/WASM live reflection and fixed-slot behavior with viewer addons."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/explorer-check';out.mkdir(parents=True,exist_ok=True)
def run(args):
    return subprocess.run(args,cwd=ROOT,check=True,text=True,capture_output=True)
for backend in ['native','wasm']:
    compiler='clang' if backend=='native' else str(ROOT/'.local/emsdk/upstream/emscripten/emcc')
    for fixture in ['flecs_inspect_test','flecs_trace_test']:
        binary=out/(fixture+('-native' if backend=='native' else '.js'))
        args=[compiler,'-std=c11','-O3' if fixture=='flecs_trace_test' else '-O1','-g','-DAW_FLECS_EXPLORER','-I.',f'tests/alienwars/{fixture}.c','ocean/alienwars/flecs_runtime.c','-lm','-o',str(binary)]
        if backend=='native' and fixture=='flecs_inspect_test':args+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        if backend=='wasm':args+=['-sSTACK_SIZE=2MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node']
        run(args);result=run([str(binary)] if backend=='native' else ['node',str(binary)])
        if fixture=='flecs_trace_test':
            expected='4c97cac7' if backend=='native' else '1e3a8cff'
            assert expected in result.stdout,result.stdout
            print(backend,result.stdout.strip())
        else:print(backend,result.stderr.strip())
        (out/f'{fixture}-{backend}.log').write_text(result.stdout+result.stderr)
print('PASS: live reflection, read-only requests, reset/recreation, and pre-port traces on native/WASM.')
