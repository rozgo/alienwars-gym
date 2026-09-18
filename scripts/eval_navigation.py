#!/usr/bin/env -S uv run
"""Stratified checkpoint/random/greedy evaluation with matched episode lists."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--model',required=True,type=Path)
parser.add_argument('--viewer',type=Path,default=ROOT/'build/nav-viewer')
parser.add_argument('--output',required=True,type=Path)
parser.add_argument('--map-seed',type=int,default=10001)
parser.add_argument('--maps',type=int,default=8)
parser.add_argument('--episode-seed',type=int,default=9001)
parser.add_argument('--sampling-seed',type=int,default=9002)
parser.add_argument('--episodes',type=int,default=64,help='Episodes per controller and terrain category')
parser.add_argument('--hidden',type=int,default=128)
parser.add_argument('--layers',type=int,default=2)
parser.add_argument('--argmax',action='store_true')
parser.add_argument('--reference',action='store_true')
args=parser.parse_args()
if args.output.exists():
    parser.error('Use a new output directory to preserve previous evaluations.')
args.output.mkdir(parents=True)
model=args.model.resolve()
report={'checkpoint':model.name,'checkpoint_sha256':hashlib.sha256(model.read_bytes()).hexdigest(),
        'source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT)),
        'settings':{k:str(v) if isinstance(v,Path) else v for k,v in vars(args).items()},'runs':[]}
controllers=[('policy',0),('random',1),('greedy',2)]+([('reference',3)] if args.reference else [])
witnesses={}
for name,mode in controllers:
    for kind,terrain in enumerate(['surface','bridge','tunnel']):
        cmd=[str(args.viewer.resolve()),'--headless',f'--episodes={args.episodes}',
             f'--env.controller={mode}',f'--env.task_kind={kind}',
             f'--env.map_seed={args.map_seed}',f'--env.maps={args.maps}',
             f'--env.episode_seed={args.episode_seed}',f'--base.seed={args.sampling_seed}',
             f'--policy.hidden_size={args.hidden}',f'--policy.num_layers={args.layers}']
        if mode==0:
            cmd.insert(1,str(model))
            if args.argmax:cmd.append('--argmax')
        output=args.output/f'{name}-{terrain}.jsonl'
        start=time.monotonic()
        with output.open('w') as out,output.with_suffix('.stderr').open('w') as err:
            subprocess.run(cmd,cwd=ROOT,stdout=out,stderr=err,check=True,timeout=600)
        records=[json.loads(line) for line in output.read_text().splitlines()]
        episodes=[r for r in records if r['type']=='episode']
        assert len(episodes)==args.episodes
        assert all(r['kind']==kind and r['invalid_actions']==0 for r in episodes)
        tasks=[(r['map_seed'],r['map_hash'],r['start'],r['goal']) for r in episodes]
        if kind in witnesses:assert tasks==witnesses[kind], 'Controllers received different episode lists'
        else:witnesses[kind]=tasks
        result={'controller':name,'terrain':terrain,'episodes':len(episodes),
                'successes':sum(r['success'] for r in episodes),'falls':sum(r['fall'] for r in episodes),
                'timeouts':sum(r['timeout'] for r in episodes),
                'mean_contacts':sum(r['contacts'] for r in episodes)/len(episodes),
                'mean_steps':sum(r['steps'] for r in episodes)/len(episodes),
                'process_seconds':time.monotonic()-start,'command':cmd,
                'artifact_sha256':hashlib.sha256(output.read_bytes()).hexdigest()}
        report['runs'].append(result)
        (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(f"{name}/{terrain}: {result['successes']}/{len(episodes)} arrivals, {result['falls']} support failures",flush=True)
