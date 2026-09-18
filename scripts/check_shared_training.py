#!/usr/bin/env -S uv run
"""Audit a completed native shared curriculum against its exact checkpoints."""
import argparse,hashlib,json,math,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('runs');p.add_argument('--out',required=True);args=p.parse_args()
runs=json.loads(Path(args.runs).read_text());audits=[];previous={}
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def finite(value):
 if isinstance(value,float):assert math.isfinite(value)
 elif isinstance(value,dict):
  for v in value.values():finite(v)
 elif isinstance(value,list):
  for v in value:finite(v)
for stage in runs['stages']:
 run=stage['run'];directory=Path('outputs/shared/checkpoints/alienwars_shared')/run
 records=[json.loads(s) for s in (Path('logs/alienwars_shared')/f'{run}.jsonl').read_text().splitlines()];finite(records)
 text=(Path('logs/alienwars_shared')/f'{run}.ini').read_text()
 report={'run':run,'source_commit':stage.get('source_commit',runs['source_commit']),'families':[],'metric_records':len(records)}
 for family in range(5):
  suffix=f'.policy-{family}.bin' if family else ''
  initial=directory/f'initial.bin{suffix}';final=directory/f"{stage['agent_steps']:016d}.bin{suffix}"
  a=initial.read_bytes();b=final.read_bytes();assert len(a)==len(b)==730624
  weights=struct.unpack('<182656f',b);assert all(math.isfinite(v) for v in weights)
  before=digest(initial);after=digest(final);assert before!=after and after==stage['checkpoints'][family]
  if stage['curriculum']:assert before==previous[(stage['seed'],family)],'Warm initialization differs from previous exact checkpoint'
  previous[(stage['seed'],family)]=after
  report['families'].append({'family':family,'initial_sha256':before,'final_sha256':after,'finite_weights':True,'weights_changed':True})
  assert any(f'policy_{family}/loss/' in json.dumps(r) for r in records),'Missing learner metrics'
 audits.append(report)
assert len(audits)==len(runs['seeds'])*3
Path(args.out).write_text(json.dumps({'stages':audits,'all_finite':True,'all_five_learners_updated':True,'warm_initialization_exact':True},indent=2)+'\n')
print(f'TRAINING_AUDIT stages={len(audits)} learners={len(audits)*5} finite=PASS independent_updates=PASS warm_weights=PASS')
