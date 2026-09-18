#!/usr/bin/env -S uv run
"""Build the trained Navigation Lab and a static, recorded Training Observatory."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from nav_dashboard import read_records

ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
release=json.loads((ROOT/'web/navigation/release.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def write_json(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(data,allow_nan=False,separators=(',',':'))+'\n')

model=Path(os.environ.get('AW_NAV_MODEL',str(ROOT/'outputs/navigation/checkpoints'/release['checkpoint']))).resolve()
assert sha(model)==release['checkpoint_sha256'],'Use the evaluated release checkpoint'
env=os.environ.copy()
env['PATH']=str(ROOT/'.local/emsdk/upstream/emscripten')+os.pathsep+env['PATH']
env.update(AW_NAV_MODEL=str(model),AW_NAV_PUBLIC='1',AW_NAV_HIDDEN=str(release['hidden']),AW_NAV_LAYERS=str(release['layers']))
subprocess.run(['./build.sh','alienwars','--web','--rl'],env=env,check=True)
viewer=ROOT/'docs/navigation';viewer.mkdir(parents=True,exist_ok=True)
for name in ['index.html','index.js','index.wasm','index.data','theme.css','manifest.json']:
    shutil.copyfile(ROOT/'build/web/alienwars-nav'/name,viewer/name)
    (viewer/name).chmod(0o644)

site=ROOT/'docs/training';site.mkdir(parents=True,exist_ok=True)
html=(ROOT/'web/navigation/dashboard.html').read_text()
html=html.replace('name="alienwars-data" content="live"','name="alienwars-data" content="static"')
html=html.replace('href="viewer/"','href="../navigation/?kind=1"')
html=html.replace('<a href="../navigation/?kind=1">Navigation Lab ↗</a>',
    '<nav style="display:flex;gap:16px"><a href="../maplab/">Map Lab</a><a href="../navigation/?kind=1">Navigation Lab</a><a href="../demo/">Demo</a></nav>')
html=html.replace(' · refresh every 2 seconds',' · recorded training runs')
(site/'index.html').write_text(html)
shutil.copyfile(ROOT/'web/navigation/theme.css',site/'theme.css')

sources={};runs=[]
for run in release['runs']:
    path=ROOT/'logs/alienwars'/f'{run}.jsonl'
    rows,errors=read_records(path);assert not errors,path
    published=[]
    for row in rows:
        kind=row.get('type')
        if kind=='metrics':
            assert all(k=='type' or isinstance(v,(int,float)) or v is None for k,v in row.items())
            published.append(row)
        elif kind=='start':published.append({k:row[k] for k in ['type','run_id','env']})
        elif kind=='complete':published.append({k:row[k] for k in ['type','agent_steps']})
        elif kind=='aborted':published.append({k:row[k] for k in ['type','reason']})
        elif kind=='initialization':published.append({'type':kind,'checkpoint':Path(row['checkpoint']).name,'optimizer_reset':row['optimizer_reset']})
        else:raise ValueError(f'Unreviewed record type: {kind}')
    samples=sum(r['type']=='metrics' for r in rows)
    write_json(site/'data/runs'/f'{run}.jsonl.json',{'records':published,'modified':path.stat().st_mtime,'samples':samples,'errors':0})
    runs.append({'name':path.name,'modified':path.stat().st_mtime})
    sources[path.name]=sha(path)
write_json(site/'data/runs.json',runs)

evaluations=[]
for path in sorted((ROOT/'outputs/navigation/evaluations'/release['evaluations']).glob('*.jsonl')):
    rows,errors=read_records(path);assert not errors,path
    metadata=next(r for r in rows if r.get('type')=='evaluation')
    summary=next(r for r in reversed(rows) if r.get('type')=='summary')
    metadata={k:v for k,v in metadata.items() if k in ['type','controller','map_seed','maps','episode_seed','sampling_seed','task_kind','episodes','argmax']}
    metadata['controller']=Path(str(metadata['controller'])).name
    evaluations.append({'name':release['evaluations']+'/'+path.name,'metadata':metadata,'summary':summary,'errors':0})
    sources[path.name]=sha(path)
assert len(evaluations)==12,'Publish all four evaluated controllers and three cohorts'
write_json(site/'data/evaluations.json',evaluations)
write_json(site/'manifest.json',{'source':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
    'mode':'recorded training data','source_artifact_sha256':sources,
    'normalization':'Numeric metrics and evaluation summaries retained; checkpoint paths reduced to filenames.',
    'files':{str(p.relative_to(site)):sha(p) for p in sorted(site.rglob('*')) if p.is_file() and p.name!='manifest.json'}})
print('Built docs/navigation and docs/training with checkpoint',release['checkpoint_sha256'])
