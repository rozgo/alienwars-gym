#!/usr/bin/env -S uv run
"""Shared mission, physical planner and adapter contracts in native C / WASM."""
from pathlib import Path
import json,os,subprocess
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
Path('build').mkdir(exist_ok=True);Path('outputs').mkdir(exist_ok=True)
emcc=str(ROOT/'.local/emsdk/upstream/emscripten/emcc')
report={}
for name in ['mission_route','shared_adapter','command_fleet']:
    source=f'tests/alienwars/{name}_test.c'
    includes=['-I.','-Isrc','-Ivendor','-Iraylib-5.5_macos/include','-Iraylib-5.5_linux_amd64/include']
    subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined',*includes,source,'ocean/alienwars/flecs_runtime.c','-lm','-o',f'build/{name}-test'],check=True)
    native=subprocess.check_output([f'build/{name}-test'],text=True)
    subprocess.run([emcc,'-std=c11','-O3',*includes,source,'ocean/alienwars/flecs_runtime.c','-lm','-sSTACK_SIZE=2MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node','-o',f'build/{name}-test.js'],check=True)
    wasm=subprocess.check_output(['node',f'build/{name}-test.js'],text=True)
    assert native==wasm,(name,native,wasm)
    report[name]=native.strip();print(native.strip(),flush=True)
Path('outputs/shared-check.json').write_text(json.dumps(report,indent=2)+'\n')

models=Path(os.environ.get('AW_MISSION_MODELS','outputs/shared/selected')).resolve()
if all((models/f'mission-{f}.bin').exists() for f in range(5)):
    source='tests/alienwars/shared_policy_test.c'
    subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-I.',source,'-lm','-o','build/shared-policy-test'],check=True)
    native=subprocess.check_output(['build/shared-policy-test',str(models)],text=True)
    subprocess.run([emcc,'-std=c11','-O3','-I.',source,'--preload-file',f'{models}@models','-sSTACK_SIZE=2MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node','-o','build/shared-policy-test.js'],check=True)
    wasm=subprocess.check_output(['node','shared-policy-test.js','models'],cwd='build',text=True)
    assert native==wasm,('Shared policy inference',native,wasm)
    report['policy']=native.strip();print(native.strip(),flush=True)
else:
    report['policy']='No five-model selection: inference parity not run.'
Path('outputs/shared-check.json').write_text(json.dumps(report,indent=2)+'\n')
