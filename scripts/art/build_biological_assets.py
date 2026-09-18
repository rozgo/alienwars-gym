"""Original AlienWars art. Blender 5: blender -b --python this_file.py.

Game coordinates: +Y up, +Z forward. AWM1 is a tiny render-only interchange:
magic, uint32 vertex count, then triangle vertices (8 float32 + 4 uint8).
UV stores material class and appendage weight, not a texture coordinate.
The reproducible source is this script; editable .blend files go to outputs/art.
"""
import bpy, math, struct, json, hashlib
from mathutils import Vector
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'resources/alienwars/art'
PREVIEW=ROOT/'outputs/art'
OUT.mkdir(parents=True,exist_ok=True);PREVIEW.mkdir(parents=True,exist_ok=True)
report={}
COLORS={'shell':(.48,.55,.37),'rim':(.68,.69,.49),'tissue':(.20,.29,.25),
        'membrane':(.34,.44,.34),'rib':(.63,.61,.43),'eye':(.26,.92,.76),
        'root':(.25,.31,.23),'pod':(.64,.63,.39)}

def reset():
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    for m in list(bpy.data.materials):bpy.data.materials.remove(m)
    for name,color in COLORS.items():
        m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
        bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(*color,1)
        bs.inputs['Roughness'].default_value=.36 if name=='shell' else .65
        if name=='eye':bs.inputs['Emission Color'].default_value=(*color,1);bs.inputs['Emission Strength'].default_value=.45

def finish(o,material,animate=0):
    o.data.materials.append(bpy.data.materials[material]);o['animate']=animate
    for p in o.data.polygons:p.use_smooth=True
    return o

def ellipsoid(name,p,size,mat='shell',segments=20,rings=10):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,location=p)
    o=bpy.context.object;o.name=name;o.scale=size;return finish(o,mat)

def tube(name,points,radii,mat='rib',animate=0,sides=7):
    # Parallel transported rings avoid pinches along the tapered organic ribs.
    pts=[Vector(p) for p in points];verts=[];faces=[]
    for i,p in enumerate(pts):
        tangent=(pts[min(i+1,len(pts)-1)]-pts[max(0,i-1)]).normalized()
        axis=tangent.cross(Vector((0,1,0)))
        if axis.length<.01:axis=tangent.cross(Vector((1,0,0)))
        axis.normalize();other=tangent.cross(axis).normalized()
        for j in range(sides):verts.append(p+radii[i]*(axis*math.cos(j*math.tau/sides)+other*math.sin(j*math.tau/sides)))
    for i in range(len(pts)-1):
        for j in range(sides):
            a=i*sides+j;b=i*sides+(j+1)%sides;faces.append((a,b,b+sides,a+sides))
    faces.extend([tuple(reversed(range(sides))),tuple((len(pts)-1)*sides+j for j in range(sides))])
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o);return finish(o,mat,animate)

def plate(name,center,width,length,rise,mat='shell'):
    # A thick arched shield with overlapping scalloped trailing edge.
    verts=[];faces=[];N=12;M=7
    for j in range(M):
        t=j/(M-1);z=center[2]+(t-.5)*length
        taper=.42+.58*math.sin(math.pi*t)**.55
        for i in range(N):
            u=i/(N-1)*2-1
            verts.append((center[0]+u*width*taper,center[1]+rise*(1-u*u)*math.sin(math.pi*t)**.4,z))
    for j in range(M-1):
        for i in range(N-1):
            a=j*N+i;faces.append((a,a+N,a+N+1,a+1))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o);finish(o,mat)
    solid=o.modifiers.new('Living shell thickness','SOLIDIFY');solid.thickness=.022
    bevel=o.modifiers.new('Soft shell edge','BEVEL');bevel.width=.015;bevel.segments=2
    return o

def scout():
    ellipsoid('Abdominal muscle',(0,.37,-.13),(.27,.22,.38),'tissue')
    for k in range(4):plate('Overlapping abdominal shield',(0,.49,-.37+k*.16),.30,.26,.18)
    ellipsoid('Forward sensory crown',(0,.47,.34),(.23,.18,.23),'shell')
    plate('Crown ridge',(0,.61,.31),.16,.36,.11,'rim')
    for side in [-1,1]:
        ellipsoid('Forward photoreceptor',(side*.15,.49,.51),(.059,.052,.038),'eye',12,8)
        tube('Sensory feeler',[(side*.12,.55,.46),(side*.18,.61,.53),(side*.20,.66,.58)],[.025,.017,.006],'rim')
        for k,z in enumerate([-.32,-.02,.27]):
            tube('Walking limb',[(side*.21,.4,z),(side*.42,.33,z-.07),(side*.55,.12,z+.07),(side*.52,.025,z+.16)],[.065,.053,.026,.015],'tissue',1)
            tube('Limb armor',[(side*.23,.43,z),(side*.37,.39,z-.045),(side*.43,.32,z-.06)],[.073,.069,.032],'rim',1)
        for k in range(4):
            ellipsoid('Flank breathing slit',(side*.273,.38,-.31+k*.115),(.014,.033,.036),'rim',10,6)

def skiff():
    ellipsoid('Aquatic muscle',(0,.13,-.08),(.28,.25,.94),'tissue')
    for k in range(5):plate('Hydrodynamic carapace',(0,.28,-.65+k*.28),.29,.46,.21)
    tube('Dorsal ridge',[(0,.42,-.85),(0,.61,-.30),(0,.63,.25),(0,.39,.82)],[.015,.045,.04,.008],'rim')
    for s in [-1,1]:
        ellipsoid('Sensory organ',(s*.16,.32,.77),(.058,.057,.105),'eye',14,8)
        for k in range(6):
            z=-.62+k*.2
            tube('Swimming frill',[(s*.22,.10,z),(s*.35,.08,z-.04),(s*.38,-.06,z-.14)],[.047,.05,.005],'membrane',.65)
            tube('Respiratory fold',[(s*.265,.30,z),(s*.295,.21,z+.03),(s*.27,.11,z+.045)],[.017,.018,.006],'rim')

def wing():
    ellipsoid('Gliding body',(0,0,.13),(.19,.18,1.04),'tissue')
    for k in range(4):plate('Dorsal flight shield',(0,.11,-.40+k*.27),.22,.43,.16)
    ellipsoid('Navigation crown',(0,.05,.96),(.13,.13,.25),'shell')
    for s in [-1,1]:
        ellipsoid('Forward organ',(s*.08,.08,1.15),(.042,.043,.058),'eye',12,6)
        verts=[];faces=[];N=13;M=8
        for j in range(N):
            t=j/(N-1);x=s*(.12+1.46*t)
            leading=.57-.96*t;trailing=-.61-.21*t+.1*math.sin(t*math.pi)
            for i in range(M):
                u=i/(M-1);z=leading+(trailing-leading)*u
                y=.04+.16*math.sin(math.pi*u)*math.sin(math.pi*t)-.14*t
                verts.append((x,y,z))
        for j in range(N-1):
            for i in range(M-1):
                a=j*M+i;f=(a,a+1,a+M+1,a+M);faces.append(f if s==1 else tuple(reversed(f)))
        mesh=bpy.data.meshes.new('Flight membrane');mesh.from_pydata(verts,[],faces);mesh.update()
        o=bpy.data.objects.new('Flight membrane',mesh);bpy.context.collection.objects.link(o);finish(o,'membrane',.3)
        sol=o.modifiers.new('Membrane thickness','SOLIDIFY');sol.thickness=.015
        for j in range(7):
            t=j/6;x=s*(.15+1.40*t)
            tube('Membrane spar',[(s*.15,.08,.44-j*.12),(x*.67,.13,-.05-j*.1),(x,-.13*t,-.64-.18*t)],[.025,.018,.006],'rib',.3,6)
        tube('Leading flight edge',[(s*.14,.06,.57),(s*.62,.10,.23),(s*1.13,.02,-.13),(s*1.58,-.14,-.39)],[.053,.049,.027,.009],'shell',.3)
    plate('Tail stabilizer',(0,.035,-.90),.44,.48,.065,'rim')

def nursery():
    ellipsoid('Root crown',(0,.31,0),(1.28,.37,1.37),'root',28,12)
    ellipsoid('Nurturing organ',(0,1.21,0),(.94,1.14,.93),'tissue',28,14)
    for k in range(9):
        a=k*math.tau/9;co=math.cos(a);si=math.sin(a)
        points=[(co*1.85,.015,si*1.8),(co*1.3,.20,si*1.25),(co*.98,.71,si*.94),(co*.88,1.43,si*.83),(co*.60,2.17,si*.56),(co*.20,2.62,si*.18)]
        tube('Grown structural rib',points,[.065,.19,.23,.18,.105,.012],'shell',0,10)
        if k%3==0:
            ellipsoid('Peripheral seed chamber',(co*1.17,.51,si*1.12),(.44,.59,.43),'pod')
        ellipsoid('Living aperture',(co*.835,1.32,si*.805),(.12,.26,.12),'eye',12,8)
    ellipsoid('Apical seed',(0,2.44,0),(.24,.43,.24),'pod')
    for k in range(5):
        a=k*math.tau/5;tube('Apical antenna',[(math.cos(a)*.18,2.43,math.sin(a)*.18),(math.cos(a)*.31,2.9,math.sin(a)*.31),(math.cos(a)*.22,3.1,math.sin(a)*.22)],[.045,.022,.006],'rim')

def export(name):
    dep=bpy.context.evaluated_depsgraph_get();records=[];positions=[]
    for o in list(bpy.context.scene.objects):
        if o.type!='MESH':continue
        ev=o.evaluated_get(dep);mesh=ev.to_mesh();mesh.calc_loop_triangles()
        transform=o.matrix_world;normal_matrix=transform.to_3x3().inverted().transposed()
        mat=o.data.materials[0];rgb=tuple(round(c*255) for c in mat.diffuse_color[:3])
        kind=2 if mat.name=='eye' else 1 if mat.name=='membrane' else 0
        for tri in mesh.loop_triangles:
            corners=[]
            for li in tri.loops:
                loop=mesh.loops[li];v=mesh.vertices[loop.vertex_index];p=transform@v.co
                n=(normal_matrix@mesh.corner_normals[li].vector).normalized();corners.append((p,n))
            face=(corners[1][0]-corners[0][0]).cross(corners[2][0]-corners[0][0])
            if face.length_squared<1e-14:continue
            face.normalize()
            for p,n in corners:
                # Tiny bevel caps need face normals instead of a smoothed normal
                # pointing into the adjacent shell after nonuniform scaling.
                if n.dot(face)<0:n=face
                records.append(struct.pack('<8f4B',*p,*n,kind,float(o.get('animate',0)),*rgb,255));positions.append(tuple(p))
        ev.to_mesh_clear()
    data=b'AWM1'+struct.pack('<I',len(records))+b''.join(records)
    (OUT/(name+'.awm')).write_bytes(data)
    report[name]={'vertices':len(records),'triangles':len(records)//3,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'bounds':[[min(p[k] for p in positions) for k in range(3)],[max(p[k] for p in positions) for k in range(3)]]}
    bpy.ops.wm.save_as_mainfile(filepath=str(PREVIEW/(name+'.blend')))
    print(name,report[name])

for name,fn in [('scout',scout),('skiff',skiff),('wing',wing),('nursery',nursery)]:
    reset();fn();export(name)
(OUT/'models.json').write_text(json.dumps(report,indent=2)+'\n')
