#!/usr/bin/env python3
"""Bounded three-seed curriculum using the native five-learner PPO trainer.
Run in a clean isolated GPU checkout; output retains all configs and weights.
"""
import argparse,hashlib,json,os,shutil,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
p=argparse.ArgumentParser();p.add_argument('--seeds',type=int,nargs='+',default=[373,474,575]);p.add_argument('--prefix',default='shared-v2');p.add_argument('--binary',default='build/puffer-shared');args=p.parse_args()
assert not subprocess.check_output(['git','status','--porcelain','--untracked-files=no']).strip(),'Use a clean source checkout'
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
root=Path('outputs/shared')/args.prefix;root.mkdir(parents=True,exist_ok=True)
manifest={'source_commit':source,'seeds':args.seeds,'stages':[]}
for seed in args.seeds:
    previous=None
    for curriculum,epochs in [(0,128),(1,256),(2,512)]:
        steps=epochs*288*64;run=f'{args.prefix}-s{seed}-c{curriculum}'
        command=[args.binary,'train',f'--base.seed={seed}',f'--base.run_id={run}',
                 '--base.checkpoint_dir=outputs/shared/checkpoints',f'--train.total_timesteps={steps}',
                 f'--env.curriculum={curriculum}','--env.maps=8','--env.map_seed=301',
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
        manifest['stages'].append({'seed':seed,'curriculum':curriculum,'agent_steps':steps,'run':run,'command':command,'process_seconds':time.monotonic()-start,'checkpoints':hashes})
        (root/'runs.json').write_text(json.dumps(manifest,indent=2)+'\n')
        previous=selected
        print(f'COMPLETE {run} {steps} steps',flush=True)
