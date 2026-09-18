#!/usr/bin/env python3
"""Evaluate exact shared-world checkpoint sets against matched baselines."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
p=argparse.ArgumentParser();p.add_argument('--models',action='append',default=[],help='label=directory with mission-N.bin');p.add_argument('--seed',type=int,required=True);p.add_argument('--maps',type=int,default=8);p.add_argument('--curriculum',type=int,default=2);p.add_argument('--out',required=True);p.add_argument('--baselines',action='store_true');args=p.parse_args()
assert 1<=args.maps<=32
out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
binary='build/shared-eval'
subprocess.run(['clang','-std=c11','-O3','-I.','-Isrc','-Ivendor','-Iraylib-5.5_macos/include','-Iraylib-5.5_linux_amd64/include','ocean/alienwars_shared/shared_eval.c','ocean/alienwars/flecs_runtime.c','-lm','-o',binary],check=True)
runs=[(v,v) for v in ['reference','random']] if args.baselines else []
for item in args.models:
    label,directory=item.split('=',1);assert label and '/' not in label;runs.append((label,directory))
report={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'map_seed':args.seed,'maps':args.maps,'scenarios':3*args.maps,'curriculum':args.curriculum,'results':{}}
for label,directory in runs:
    command=[binary,directory,str(args.seed),str(args.maps),str(3*args.maps),str(args.curriculum),'json']
    with (out/f'{label}.jsonl').open('w') as data,(out/f'{label}.log').open('w') as log:subprocess.run(command,stdout=data,stderr=log,check=True)
    records=[json.loads(line) for line in (out/f'{label}.jsonl').read_text().splitlines() if line.startswith('{')]
    result={'command':command,'families':[v for v in records if 'attempted' in v]}
    assert len(result['families'])==5 and sum('scenario' in v for v in records)==36*args.maps
    for family in result['families']:
        family['requested_arrival_rate']=family['arrivals']/family['attempted']
        available=family['attempted']-family['unavailable'];family['available_arrival_rate']=family['arrivals']/available if available else None
    if directory not in ['reference','random']:result['hashes']=[hashlib.sha256((Path(directory)/f'mission-{f}.bin').read_bytes()).hexdigest() for f in range(5)]
    report['results'][label]=result;(out/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    print(label,[(f['family'],f['arrivals'],f['attempted'],f['unavailable']) for f in result['families']],flush=True)
