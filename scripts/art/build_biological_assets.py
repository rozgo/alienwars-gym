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

def sail(name,points,mat='membrane',animate=.5):
    """Ribbed, thickened membrane, curved through a central control point."""
    center=sum((Vector(p) for p in points),Vector())/len(points)
    center.y+=.065
    verts=[center]+[Vector(p) for p in points]
    faces=[(0,i+1,(i+1)%len(points)+1) for i in range(len(points))]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o);finish(o,mat,animate)
    sol=o.modifiers.new('Living membrane','SOLIDIFY');sol.thickness=.012
    for i in range(1,len(points)-1):
        p=Vector(points[0]);q=Vector(points[i]);mid=p.lerp(q,.55);mid.y+=.055
        tube(name+' vein',[p,mid,q],[.026,.018,.004],'rib',animate,6)

def crown(y,z,width):
    ellipsoid('Forward sensory crown',(0,y,z),(width,.16,.20),'shell',18,8)
    for s in [-1,1]:
        ellipsoid('Forward photoreceptor',(s*width*.60,y+.045,z+.16),(.055,.046,.054),'eye',12,6)
    tube('Median crown crest',[(0,y+.11,z-.10),(0,y+.23,z+.03),(0,y+.16,z+.20)],[.04,.028,.008],'rim')

def rover():
    # Low armored grazer: eight gripping limbs and a broad overlapping shell.
    ellipsoid('Locomotor body',(0,.40,-.12),(.43,.30,.74),'tissue',24,10)
    for k in range(5):plate('Broad traction shield',(0,.59,-.66+k*.28),.46,.43,.23)
    crown(.49,.72,.29)
    for s in [-1,1]:
        for k,z in enumerate([-.68,-.27,.15,.57]):
            tube('Gripping limb',[(s*.34,.39,z),(s*.62,.35,z-.09),(s*.70,.10,z+.05),(s*.67,.035,z+.18)],[.11,.09,.045,.025],'tissue',.65)
            plate('Flank armor',(s*.39,.40,z),.20,.30,.15,'rim')
        tube('Heavy shoulder ridge',[(s*.30,.62,-.68),(s*.49,.69,-.10),(s*.31,.68,.57)],[.07,.09,.035],'shell')

def hauler():
    # Long six-pair crawler carrying integrated metabolic storage lobes.
    ellipsoid('Segmented core',(0,.37,-.12),(.39,.30,1.06),'tissue',24,10)
    for k in range(6):
        z=-.90+k*.33
        plate('Load bearing saddle',(0,.48,z),.45,.44,.17)
        ellipsoid('Storage organ',(0,.74,z),(.31,.28,.23),'pod',16,8)
        for s in [-1,1]:
            tube('Saddle rib',[(s*.39,.41,z),(s*.35,.76,z),(s*.16,.99,z)],[.055,.041,.015],'rim')
            tube('Load bearing limb',[(s*.31,.35,z),(s*.59,.30,z-.08),(s*.70,.06,z+.10)],[.09,.073,.025],'tissue',.65)
    crown(.48,1.02,.25)

def patrol_boat():
    ellipsoid('Buoyant body',(0,.17,-.12),(.38,.32,1.33),'tissue',24,12)
    for k in range(6):plate('Keel shield',(0,.37,-1.09+k*.38),.40,.54,.26)
    crown(.46,1.26,.25)
    for s in [-1,1]:
        for k in range(4):
            z=-.93+k*.48
            sail('Swimming lobe',[(s*.29,.17,z+.22),(s*.53,.10,z+.08),(s*.50,-.08,z-.24),(s*.28,.09,z-.30)])
            ellipsoid('Gill organ',(s*.34,.34,z),(.08,.16,.10),'rim',12,6)
    sail('Dorsal sensory sail',[(0,.46,-.88),(0,.92,-.25),(0,.98,.25),(0,.48,.70)],'shell',.12)

def cutter():
    # Broad paired flotation muscles, tall carapace and long stern flukes.
    ellipsoid('Deep core',(0,.26,-.12),(.45,.42,1.70),'tissue',24,12)
    for s in [-1,1]:
        ellipsoid('Flotation organ',(s*.39,.15,-.22),(.24,.31,1.27),'shell',20,10)
        for k in range(5):
            z=-1.03+k*.45
            tube('Exposed flank rib',[(s*.25,.55,z),(s*.55,.46,z-.06),(s*.64,.17,z-.12)],[.055,.064,.014],'rim')
        sail('Stern fluke',[(s*.16,.20,-1.1),(s*.68,.08,-1.66),(s*.48,.02,-1.93),(s*.12,.11,-1.64)])
    for k in range(5):plate('High dorsal shield',(0,.52,-.98+k*.49),.40,.63,.38)
    crown(.56,1.65,.31)
    tube('Sensory ridge',[(0,.71,-.83),(0,1.12,-.14),(0,1.25,.50),(0,.72,1.18)],[.06,.075,.04,.012],'rim')

def quad():
    # Four radial lift organs allow hover and strafe; the crown still marks +Z.
    ellipsoid('Hover core',(0,0,0),(.31,.19,.39),'tissue',24,10)
    plate('Dorsal hover shield',(0,.11,-.03),.32,.67,.16)
    crown(.015,.39,.20)
    for x in [-1,1]:
        for z in [-1,1]:
            p=(x*.61,-.035,z*.58)
            tube('Lift organ tendon',[(x*.20,.02,z*.22),(x*.42,.045,z*.41),p],[.09,.065,.10],'shell')
            ellipsoid('Lift bladder',p,(.27,.15,.27),'membrane',20,10)['animate']=.25
            plate('Lift valve',(p[0],.055,p[2]),.26,.48,.09,'rim')['animate']=.4
            for k in [-1,0,1]:
                tube('Ventral lift gill',[(p[0]+k*.07,-.09,p[2]-.14),(p[0]+k*.07,-.21,p[2]),(p[0]+k*.07,-.12,p[2]+.15)],[.017,.023,.010],'rib',.5)

def transport():
    # A broad manta-like glider, with paired metabolic lobes under its wings.
    ellipsoid('Transport body',(0,.02,.03),(.41,.29,1.43),'tissue',24,12)
    for k in range(5):plate('Transport shield',(0,.21,-.98+k*.48),.41,.64,.24)
    crown(.11,1.39,.21)
    for s in [-1,1]:
        ellipsoid('Flight reserve organ',(s*.51,-.035,-.20),(.24,.25,.77),'pod',18,10)
        sail('Broad flight membrane',[(s*.29,.13,.93),(s*1.17,.12,.45),(s*2.39,-.12,-.57),(s*1.91,-.06,-1.34),(s*.43,.04,-1.14)],animate=.30)
        tube('Leading flight edge',[(s*.25,.16,.91),(s*1.10,.18,.48),(s*1.90,.03,-.10),(s*2.38,-.11,-.56)],[.075,.069,.040,.009],'shell',.25)
        sail('Caudal stabilizer',[(s*.12,.05,-1.1),(s*.82,.08,-1.47),(s*.45,.02,-1.61),(s*.12,.01,-1.42)],'rim',.2)

def submarine(variant):
    # Recon is slender and finned; patrol has side lobes; heavy has pressure rings.
    length=[1.12,1.48,1.88][variant];width=[.31,.42,.56][variant]
    radius=[.29,.39,.50][variant]
    ellipsoid('Pressure body',(0,0,0),(width,radius,length),'tissue',28,14)
    for k in range(5+variant):
        z=-length*.68+k*length*1.30/(4+variant)
        plate('Pressure carapace',(0,radius*.63,z),width*.97,length*.45,radius*.53)
    for s in [-1,1]:
        ellipsoid('Forward sonar organ',(s*width*.53,.06,length*.90),(.075,.085,.12),'eye',14,8)
        if variant==0:
            sail('Recon steering fin',[(s*.22,0,.52),(s*.49,-.04,.04),(s*.46,-.05,-.35),(s*.22,0,-.20)])
        elif variant==1:
            ellipsoid('Long range respiratory lobe',(s*.39,-.04,-.10),(.20,.24,.86),'shell',20,10)
            sail('Patrol pectoral fin',[(s*.39,.02,.65),(s*.67,-.04,.10),(s*.60,-.03,-.39),(s*.35,0,-.37)])
        else:
            ellipsoid('Reserve organ',(s*.48,-.06,-.22),(.25,.34,1.20),'pod',20,10)
            for k in range(5):
                z=-1.12+k*.50
                tube('External pressure arch',[(s*.24,.44,z),(s*.57,.48,z-.05),(s*.75,.11,z-.09),(s*.57,-.37,z-.03)],[.045,.057,.04,.015],'rim')
            sail('Heavy pectoral fin',[(s*.44,0,.70),(s*.82,-.02,.07),(s*.76,-.02,-.58),(s*.39,0,-.60)])
        sail('Caudal fluke',[(s*.10,.0,-length*.66),(s*(width+.16),.04,-length*.96),(s*.28,0,-length*1.04),(s*.09,-.02,-length*.89)],'membrane',.7)
    sail('Dorsal fin',[(0,radius*.7,.48),(0,radius*1.64,.08),(0,radius*1.32,-.51),(0,radius*.6,-.68)],'rim',.2)

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

for name,fn in [('scout',scout),('skiff',skiff),('wing',wing),('nursery',nursery),
                ('rover',rover),('hauler',hauler),('patrol_boat',patrol_boat),('cutter',cutter),
                ('quad',quad),('transport',transport),('recon_sub',lambda:submarine(0)),
                ('patrol_sub',lambda:submarine(1)),('heavy_sub',lambda:submarine(2))]:
    reset();fn();export(name)
(OUT/'models.json').write_text(json.dumps(report,indent=2)+'\n')
