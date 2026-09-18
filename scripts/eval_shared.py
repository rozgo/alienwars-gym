#!/usr/bin/env -S uv run
"""Evaluate exact shared-world checkpoint sets against matched baselines."""
import argparse,hashlib,json,os,subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
p=argparse.ArgumentParser();p.add_argument('--models',action='append',default=[],help='label=directory with mission-N.bin');p.add_argument('--seed',type=int,required=True);p.add_argument('--maps',type=int,default=8);p.add_argument('--curriculum',type=int,default=2);p.add_argument('--out',required=True);p.add_argument('--baselines',action='store_true');p.add_argument('--reference',action='store_true');p.add_argument('--scenarios-per-map',type=int,default=8);p.add_argument('--contract',type=int,choices=[2,3],default=3);p.add_argument('--no-assist',action='store_true');p.add_argument('--jobs',type=int,default=1);args=p.parse_args()
os.environ['AW_EVAL_SCENARIOS_PER_MAP']=str(args.scenarios_per_map);os.environ['AW_EVAL_NAV_VERSION']=str(args.contract)
if args.no_assist:os.environ['AW_EVAL_NO_ASSIST']='1'
assert 1<=args.maps<=32 and 1<=args.jobs<=16
out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
Path('build').mkdir(exist_ok=True)
binary=str(out/'shared-eval')
subprocess.run(['clang','-std=c11','-O3','-I.','-Isrc','-Ivendor','-Iraylib-5.5_macos/include','-Iraylib-5.5_linux_amd64/include','ocean/alienwars_shared/shared_eval.c','ocean/alienwars/flecs_runtime.c','-lm','-o',binary],check=True)
runs=[(v,v) for v in ['reference','random']] if args.baselines else []
if args.reference and not args.baselines:runs.append(('reference','reference'))
for item in args.models:
    label,directory=item.split('=',1);assert label and '/' not in label
    metadata=Path(directory)/'contract.json'
    assert (json.loads(metadata.read_text())['contract']==args.contract if metadata.exists() else args.contract==2),'Explicit checkpoint contract required'
    runs.append((label,directory))
report={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','status','--porcelain','--untracked-files=no']).strip()),'map_seed':args.seed,'maps':args.maps,'scenarios':args.scenarios_per_map*args.maps,'curriculum':args.curriculum,'contract':args.contract,'assistance':not args.no_assist and args.contract>=3,'scenarios_per_map':args.scenarios_per_map,'frozen_pool':os.environ.get('AW_SHARED_FROZEN_DIR'),'results':{}}
if report['frozen_pool']:
    report['frozen_hashes']=[hashlib.sha256((Path(report['frozen_pool'])/f'mission-{f}.bin').read_bytes()).hexdigest() for f in range(5)]
report['deadlock_diagnostic']='10s confined within 0.35 units; 5s yielding grace; one event until escape'
for label,directory in runs:
    command=[binary,directory,str(args.seed),str(args.maps),str(args.scenarios_per_map*args.maps),str(args.curriculum),'json']
    jobs=min(args.jobs,args.scenarios_per_map*args.maps) if directory!='random' else 1
    if jobs==1:
        with (out/f'{label}.jsonl').open('w') as data,(out/f'{label}.log').open('w') as log:subprocess.run(command,stdout=data,stderr=log,check=True)
    else:
        total=args.scenarios_per_map*args.maps
        def shard(index):
            env={**os.environ,'AW_EVAL_EPISODE_START':str(total*index//jobs),'AW_EVAL_EPISODE_STOP':str(total*(index+1)//jobs)}
            with (out/f'{label}-part{index}.jsonl').open('w') as data,(out/f'{label}-part{index}.log').open('w') as log:subprocess.run(command,env=env,stdout=data,stderr=log,check=True)
        with ThreadPoolExecutor(max_workers=jobs) as pool:list(pool.map(shard,range(jobs)))
        episodes=[];totals={}
        for index in range(jobs):
            for line in (out/f'{label}-part{index}.jsonl').read_text().splitlines():
                record=json.loads(line)
                if 'scenario' in record:episodes.append(record)
                else:
                    family=record['family'];acc=totals.setdefault(family,{'family':family})
                    for key,value in record.items():
                        if key!='family':acc[key]=acc.get(key,0)+value
        (out/f'{label}.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in episodes+[totals[f] for f in range(5)]))
    records=[json.loads(line) for line in (out/f'{label}.jsonl').read_text().splitlines() if line.startswith('{')]
    result={'command':command,'jobs':jobs,'families':[v for v in records if 'attempted' in v]}
    assert len(result['families'])==5 and sum('scenario' in v for v in records)==sum(f['attempted'] for f in result['families'])
    seen=set();initial=[];repeated=[]
    for record in records:
        if 'scenario' not in record:continue
        key=(record['scenario'],record['unit'])
        (repeated if key in seen else initial).append(record);seen.add(key)
    for family in result['families']:
        outcomes=[v for v in records if 'scenario' in v and v['family']==family['family'] and v.get('available')]
        family['timeouts_after_confinement']=sum(bool(v.get('timeout')) and v.get('deadlock_events',0)>0 for v in outcomes)
        family['arrivals_after_confinement']=sum(bool(v.get('arrived')) and v.get('deadlock_events',0)>0 for v in outcomes)
        family['requested_arrival_rate']=family['arrivals']/family['attempted']
        family['collision_free_arrival_rate']=family['collision_free_arrivals']/family['attempted']
        available=family['attempted']-family['unavailable'];family['available_arrival_rate']=family['arrivals']/available if available else None
        for name,group in [('initial',initial),('repeat',repeated)]:
            attempts=[v for v in group if v['family']==family['family']]
            clean=sum(bool(v.get('arrived')) and not v.get('contact_decisions',0) for v in attempts)
            family[f'{name}_attempts']=len(attempts);family[f'{name}_clean_arrivals']=clean
            family[f'{name}_clean_rate']=clean/len(attempts) if attempts else None
    if directory not in ['reference','random']:result['hashes']=[hashlib.sha256((Path(directory)/f'mission-{f}.bin').read_bytes()).hexdigest() for f in range(5)]
    report['results'][label]=result;(out/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    print(label,[(f['family'],f['arrivals'],f['attempted'],f['unavailable']) for f in result['families']],flush=True)
