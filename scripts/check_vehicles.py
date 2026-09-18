#!/usr/bin/env -S uv run
"""Validate A* / vehicle constraints in sanitized native C and WebAssembly."""
from pathlib import Path
import os,subprocess,json
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
Path('build').mkdir(exist_ok=True);Path('outputs').mkdir(exist_ok=True)
emcc=str(ROOT/'.local/emsdk/upstream/emscripten/emcc')
report={}
for name in ['vehicle','patrol','local_adapter','fleet']:
    source=f'tests/alienwars/{name}_test.c'
    includes=['-I.','-Isrc','-Ivendor','-Iraylib-5.5_macos/include','-Iraylib-5.5_linux_amd64/include']
    subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined',*includes,source,'-lm','-o',f'build/{name}-test'],check=True)
    native=subprocess.check_output([f'build/{name}-test'],text=True)
    subprocess.run([emcc,'-std=c11','-O3',*includes,source,'-lm','-sSTACK_SIZE=2MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node','-o',f'build/{name}-test.js'],check=True)
    wasm=subprocess.check_output(['node',f'build/{name}-test.js'],text=True)
    assert native==wasm,(name,native,wasm)
    report[name]=native.strip();print(native.strip(),flush=True)
models=Path(os.environ.get('AW_LOCAL_MODELS','outputs/local/selected')).resolve()
if all((models/f'local-{family}.bin').exists() for family in range(5)):
    source='tests/alienwars/local_policy_test.c'
    subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',source,'-lm','-o','build/local-policy-test'],check=True)
    native=subprocess.check_output(['build/local-policy-test',str(models)],text=True)
    subprocess.run([emcc,'-std=c11','-O3','-I.',source,'--preload-file',f'{models}@models','-sSTACK_SIZE=2MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node','-o','build/local-policy-test.js'],check=True)
    wasm=subprocess.check_output(['node','local-policy-test.js','models'],cwd='build',text=True)
    assert native==wasm,('policy inference',native,wasm)
    report['local_policy']=native.strip();print(native.strip(),flush=True)
else:
    report['local_policy']='Skipped: fetch all five selected checkpoints to validate inference parity.'
Path('outputs/vehicle-check.json').write_text(json.dumps(report,indent=2)+'\n')
