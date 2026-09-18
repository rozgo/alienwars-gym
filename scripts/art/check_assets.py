#!/usr/bin/env python3
"""Check the custom mesh boundary: size, winding, normals and physical bounds."""
from pathlib import Path
import hashlib,json,math,struct
ROOT=Path(__file__).resolve().parents[2];ART=ROOT/'resources/alienwars/art'
manifest=json.loads((ART/'models.json').read_text())
limits={'scout':(.58,.85,.60),'skiff':(.40,.90,1.05),'wing':(1.65,.60,1.25),'nursery':(2.0,3.2,2.0),
        'rover':(.76,1.0,1.02),'hauler':(.76,1.15,1.30),'patrol_boat':(.56,1.18,1.53),
        'cutter':(.72,1.46,2.01),'quad':(.94,.325,.94),'transport':(2.45,.60,1.65),
        'recon_sub':(.54,.675,1.20),'patrol_sub':(.70,.875,1.60),'heavy_sub':(.86,1.075,2.0)}
assert set(manifest)==set(limits), 'Every current unit and nursery must have an authored mesh'
for name,info in manifest.items():
    data=(ART/(name+'.awm')).read_bytes();assert data[:4]==b'AWM1'
    count=struct.unpack_from('<I',data,4)[0];assert count==info['vertices'] and count%3==0
    assert len(data)==8+36*count==info['bytes'];assert hashlib.sha256(data).hexdigest()==info['sha256']
    vertices=list(struct.iter_unpack('<8f4B',data[8:]));opposing=0;valid=0
    for v in vertices:
        assert all(math.isfinite(n) for n in v[:8]);assert abs(sum(n*n for n in v[3:6])-1)<.002
        assert v[6] in (0,1,2) and 0<=v[7]<=1
        assert all(abs(v[i])<=limits[name][i] for i in range(3)),(name,v[:3])
        # Conservative extrema of the shader articulation, not just rest poses.
        motion=[0,0,0]
        if name in ('scout','rover','hauler'):motion=[0,.023*v[7],.018*v[7]]
        elif name in ('skiff','patrol_boat','cutter','recon_sub','patrol_sub','heavy_sub'):motion[1]=.032*v[7]
        elif name in ('quad','wing','transport'):motion[1]=.018*v[7]
        assert all(abs(v[i])+motion[i]<=limits[name][i] for i in range(3)),(name,'articulation',v[:3])
    for i in range(0,count,3):
        a,b,c=vertices[i:i+3];u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
        cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
        if sum(x*x for x in cross)<1e-14:continue
        valid+=1;opposing+=sum(cross[k]*(a[3+k]+b[3+k]+c[3+k]) for k in range(3))<0
    assert opposing==0,(name,opposing,valid)
    print(f'{name}: {count//3} triangles, finite unit normals, winding and hull bounds PASS')
for name,info in json.loads((ART/'materials.json').read_text()).items():
    assert hashlib.sha256((ART/(name+'.png')).read_bytes()).hexdigest()==info['sha256']
print('Material hashes PASS')
