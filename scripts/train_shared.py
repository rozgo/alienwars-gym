#!/usr/bin/env -S uv run
"""Bounded three-seed curriculum using the native five-learner PPO trainer.
Run in a clean isolated GPU checkout; output retains all configs and weights.
"""
import argparse,hashlib,json,os,shutil,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
p=argparse.ArgumentParser();p.add_argument('--seeds',type=int,nargs='+',default=[373,474,575]);p.add_argument('--prefix',default='reliability-v3');p.add_argument('--binary',default='build/puffer-shared');p.add_argument('--epochs',type=int,nargs=3,default=[128,256,512]);p.add_argument('--maps',type=int,default=32);p.add_argument('--frozen',required=True);p.add_argument('--resume',type=Path);p.add_argument('--start-stage',type=int,choices=[0,1,2],default=0);args=p.parse_args()
assert 1<=args.maps<=32 and min(args.epochs)>0
manifest_path=ROOT/'config/alienwars_shared_frozen.json'
historical=json.loads(manifest_path.read_text())
assert historical['contract']==2
for f,policy in enumerate(historical['policies']):
    assert hashlib.sha256((Path(args.frozen)/f'mission-{f}.bin').read_bytes()).hexdigest()==policy['sha256']
os.environ['AW_SHARED_FROZEN_DIR']=str(Path(args.frozen).resolve())
assert not subprocess.check_output(['git','status','--porcelain','--untracked-files=no']).strip(),'Use a clean source checkout'
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
root=Path('outputs/shared')/args.prefix;root.mkdir(parents=True,exist_ok=True)
manifest={'source_commit':source,'seeds':args.seeds,'generator':13,'contract':3,'frozen_hashes':[p['sha256'] for p in historical['policies']],'stages':[]}
if args.start_stage:
    assert args.resume and len(args.seeds)==1
    parent=json.loads(args.resume.read_text());assert parent['seeds']==args.seeds and parent['contract']==3
    manifest['stages']=[{**s,'source_commit':s.get('source_commit',parent['source_commit'])} for s in parent['stages'] if s['curriculum']<args.start_stage]
    assert [s['curriculum'] for s in manifest['stages']]==list(range(args.start_stage))
    manifest['parent_manifest']=str(args.resume)
for seed in args.seeds:
    previous=args.resume.parent/f's{seed}-c{args.start_stage-1}' if args.start_stage else None
    if previous:
        metadata=json.loads((previous/'contract.json').read_text());assert metadata['contract']==3
        assert [hashlib.sha256((previous/f'mission-{f}.bin').read_bytes()).hexdigest() for f in range(5)]==manifest['stages'][-1]['checkpoints']
    for curriculum,epochs in enumerate(args.epochs):
        if curriculum<args.start_stage:continue
        steps=epochs*288*64;run=f'{args.prefix}-s{seed}-c{curriculum}'
        command=[args.binary,'train',f'--base.seed={seed}',f'--base.run_id={run}',
                 '--base.checkpoint_dir=outputs/shared/checkpoints',f'--train.total_timesteps={steps}',
                 f'--env.curriculum={curriculum}',f'--env.maps={args.maps}','--env.map_seed=301',
                 f'--train.learning_rate={.012 if curriculum==0 else .004}',
                 f'--train.ent_coef={.003 if curriculum==0 else .008}']
        if previous:command.append(f'--base.load_model_dir={previous}')
        start=time.monotonic()
        with (root/f'{run}.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        checkpoint=Path('outputs/shared/checkpoints/alienwars_shared')/run/f'{steps:016d}.bin'
        selected=root/f's{seed}-c{curriculum}';selected.mkdir(exist_ok=True)
        hashes=[]
        for family in range(5):
            src=checkpoint if family==0 else Path(f'{checkpoint}.policy-{family}.bin')
            assert src.stat().st_size==730624
            dst=selected/f'mission-{family}.bin';shutil.copyfile(src,dst);hashes.append(hashlib.sha256(dst.read_bytes()).hexdigest())
        (selected/'contract.json').write_text(json.dumps({'contract':3,'generator':13,'source_commit':source,'sha256':hashes},indent=2)+'\n')
        manifest['stages'].append({'source_commit':source,'seed':seed,'curriculum':curriculum,'agent_steps':steps,'run':run,'command':command,'process_seconds':time.monotonic()-start,'checkpoints':hashes})
        (root/'runs.json').write_text(json.dumps(manifest,indent=2)+'\n')
        previous=selected
        print(f'COMPLETE {run} {steps} steps',flush=True)
