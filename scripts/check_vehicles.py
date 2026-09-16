#!/usr/bin/env python3
"""Validate A* / vehicle constraints in sanitized native C and WebAssembly."""
from pathlib import Path
import os,subprocess,json
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
Path('build').mkdir(exist_ok=True);Path('outputs').mkdir(exist_ok=True)
emcc=str(ROOT/'.local/emsdk/upstream/emscripten/emcc')
report={}
for name in ['vehicle','patrol','local_adapter']:
    source=f'tests/alienwars/{name}_test.c'
    includes=['-I.','-Isrc','-Ivendor','-Iraylib-5.5_macos/include','-Iraylib-5.5_linux_amd64/include']
    subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined',*includes,source,'-lm','-o',f'build/{name}-test'],check=True)
    native=subprocess.check_output([f'build/{name}-test'],text=True)
    subprocess.run([emcc,'-std=c11','-O3',*includes,source,'-lm','-sSTACK_SIZE=2MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node','-o',f'build/{name}-test.js'],check=True)
    wasm=subprocess.check_output(['node',f'build/{name}-test.js'],text=True)
    assert native==wasm,(name,native,wasm)
    report[name]=native.strip();print(native.strip(),flush=True)
Path('outputs/vehicle-check.json').write_text(json.dumps(report,indent=2)+'\n')
