#!/usr/bin/env -S uv run
"""Compare Flecs to the saved pre-port runtime and audit storage/lifecycle.

The baseline is archived into ignored build storage, never a user worktree.
Native/WASM traces are compared to their respective pre-port build because
the platform math libraries already differ in their final floating-point bits.
"""
from pathlib import Path
import hashlib, io, json, os, re, statistics, subprocess, tarfile

ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
OUT=ROOT/'build/flecs-check';OUT.mkdir(parents=True,exist_ok=True)
BASE='179cbf02'
base=OUT/'baseline';base.mkdir(exist_ok=True)
archive=subprocess.check_output(['git','archive',BASE,'ocean/alienwars','ocean/alienwars_shared'])
with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
    tar.extractall(base,filter='data')
emcc=str(ROOT/'.local/emsdk/upstream/emscripten/emcc')
trace=str(ROOT/'tests/alienwars/flecs_trace_test.c')
runtime=str(ROOT/'ocean/alienwars/flecs_runtime.c')
report={'baseline_commit':subprocess.check_output(['git','rev-parse',BASE],text=True).strip(),
        'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        'source_dirty':bool(subprocess.check_output(['git','status','--porcelain'],text=True).strip()),
        'flecs':json.loads((ROOT/'vendor/flecs/version.json').read_text()),'trace':{},'lifecycle':{}}

def run(command):
    result=subprocess.run(command,text=True,capture_output=True)
    if result.returncode:
        print(result.stdout,result.stderr,flush=True)
        result.check_returncode()
    return result

for backend in ['native','wasm']:
    outputs={};times={}
    for version,include,extra in [('baseline',base,[]),('flecs',ROOT,[runtime])]:
        binary=OUT/f'{version}-{backend}'
        if backend=='wasm':binary=binary.with_suffix('.js')
        command=['clang' if backend=='native' else emcc,'-std=c11','-O3','-DAW_GENERATOR_VERSION=11',f'-I{include}',trace,*extra,'-lm','-o',str(binary)]
        if backend=='wasm':command+=['-sSTACK_SIZE=2MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node']
        run(command)
        timings=[]
        for repeat in range(3):
            result=run([str(binary)] if backend=='native' else ['node',str(binary)])
            outputs[version]=result.stdout
            timings.append(float(re.search(r'TRACE_TIME seconds=([0-9.]+)',result.stderr)[1]))
            (OUT/f'{version}-{backend}-{repeat}.log').write_text(result.stderr)
        times[version]=timings
        print(backend,version,result.stdout.strip(),flush=True)
    assert outputs['baseline']==outputs['flecs'],(backend,outputs)
    report['trace'][backend]={'result':outputs['flecs'].strip(),'baseline_seconds':times['baseline'],
        'flecs_seconds':times['flecs'],'median_time_ratio':statistics.median(times['flecs'])/statistics.median(times['baseline'])}
    (OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')

for backend in ['native','wasm']:
    binary=OUT/f'lifecycle-{backend}'
    if backend=='wasm':binary=binary.with_suffix('.js')
    command=['clang' if backend=='native' else emcc,'-std=c11','-O1','-g','-I.',
             'tests/alienwars/flecs_world_test.c',runtime,'-lm','-o',str(binary)]
    if backend=='native':command+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    else:command+=['-sSTACK_SIZE=2MB','-sALLOW_MEMORY_GROWTH=1','-sASSERTIONS=1','-sENVIRONMENT=node']
    run(command);result=run([str(binary)] if backend=='native' else ['node',str(binary)])
    report['lifecycle'][backend]={'result':result.stdout.strip(),'metrics':result.stderr.strip()}
    print(result.stdout.strip(),flush=True)
assert report['lifecycle']['native']['result']==report['lifecycle']['wasm']['result']
for name,digest in report['flecs']['sha256'].items():
    assert hashlib.sha256((ROOT/'vendor/flecs'/name).read_bytes()).hexdigest()==digest
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: Flecs preserves both pre-port traces, owns component state, and resets/steps without allocations.')
