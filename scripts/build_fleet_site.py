#!/usr/bin/env python3
"""Build Map Lab with all five hash-verified shared-world controllers."""
import hashlib,json,os,subprocess,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
manifest=json.loads((ROOT/'web/maplab/policies.json').read_text())
assert len(manifest['policies'])==5
assert manifest['contract']==2 and manifest['observations']==645 and manifest['actions']==[4,3,3,3]
assert manifest['hidden']==128 and manifest['layers']==2
models=ROOT/'outputs/shared/selected';models.mkdir(parents=True,exist_ok=True)
for family,policy in enumerate(manifest['policies']):
    assert policy['family']==family
    path=models/f'mission-{family}.bin'
    if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest()!=policy['sha256']:
        data=urllib.request.urlopen(policy['url'],timeout=60).read()
        assert len(data)==730624 and hashlib.sha256(data).hexdigest()==policy['sha256'],'Checkpoint verification failed'
        path.write_bytes(data)
    assert path.stat().st_size==730624
subprocess.run(['bash','scripts/build_maplab.sh'],env={**os.environ,'AW_MISSION_MODELS':str(models)},check=True)
report=json.loads((ROOT/'docs/maplab/build.json').read_text())
report['controllers']=manifest
(ROOT/'docs/maplab/build.json').write_text(json.dumps(report,indent=2)+'\n')

results=(ROOT/"docs/runs/shared-navigation-2026-09-16.json").read_bytes()
page=(ROOT/"web/training/fleet.html").read_text()
assert '__FLEET_RESULT_VERSION__' in page
page=page.replace('__FLEET_RESULT_VERSION__',hashlib.sha256(results).hexdigest()[:16])
(ROOT/"docs/training/fleet.html").write_text(page)
(ROOT/"docs/training/fleet.json").write_bytes(results)
training_manifest=ROOT/'docs/training/manifest.json'
if training_manifest.exists():
    training=json.loads(training_manifest.read_text())
    for name in ['fleet.html','fleet.json']:
        training['files'][name]=hashlib.sha256((ROOT/'docs/training'/name).read_bytes()).hexdigest()
    training['fleet_source']=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
    training_manifest.write_text(json.dumps(training,separators=(',',':'))+'\n')
