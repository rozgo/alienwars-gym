#!/usr/bin/env python3
"""Build Map Lab with all five hash-verified, explicitly selected controllers."""
import hashlib,json,os,subprocess,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
manifest=json.loads((ROOT/'web/maplab/policies.json').read_text())
assert len(manifest['policies'])==5
assert manifest['observations']==96 and manifest['actions']==[3,3,3,3]
assert manifest['hidden']==128 and manifest['layers']==2
models=ROOT/'outputs/local/selected';models.mkdir(parents=True,exist_ok=True)
for family,policy in enumerate(manifest['policies']):
    assert policy['family']==family
    path=models/f'local-{family}.bin'
    if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest()!=policy['sha256']:
        data=urllib.request.urlopen(policy['url'],timeout=60).read()
        assert len(data)==449024 and hashlib.sha256(data).hexdigest()==policy['sha256'],'Checkpoint verification failed'
        path.write_bytes(data)
    assert path.stat().st_size==449024
subprocess.run(['bash','scripts/build_maplab.sh'],env={**os.environ,'AW_LOCAL_MODELS':str(models)},check=True)
report=json.loads((ROOT/'docs/maplab/build.json').read_text())
report['controllers']=manifest
(ROOT/'docs/maplab/build.json').write_text(json.dumps(report,indent=2)+'\n')

(ROOT/"docs/training/fleet.html").write_text((ROOT/"web/training/fleet.html").read_text())
(ROOT/"docs/training/fleet.json").write_text((ROOT/"docs/runs/local-navigation-2026-09-15.json").read_text())
