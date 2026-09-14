#ifndef ALIENWARS_RENDER_H
#define ALIENWARS_RENDER_H
#include "raylib.h"
#include "raymath.h"
#include "map.h"
#include <math.h>

#define AW_UNIT 2.0f

typedef struct {
    Vector3 *positions, *normals;
    Vector2 *uv;
    Color *colors;
    int count, capacity;
} AwBuilder;

typedef struct {
    Mesh terrain, scenery, overlay, water;
    Material land_material, water_material;
    Shader land_shader, water_shader;
    Texture2D coast;
    int land_reveal, water_time, cut_eye, cut_target, cut_mode;
    int built;
} AwScene;

static const char *aw_vertex_shader =
#ifdef PLATFORM_WEB
    "#version 300 es\n"
#else
    "#version 330\n"
#endif
    "precision highp float;\n"
    "in vec3 vertexPosition; in vec3 vertexNormal; in vec2 vertexTexCoord; in vec4 vertexColor;\n"
    "uniform mat4 mvp; out vec3 position; out vec3 normal; out vec4 color; out vec2 uv;\n"
    "void main(){position=vertexPosition;normal=vertexNormal;color=vertexColor;uv=vertexTexCoord;"
    "gl_Position=mvp*vec4(vertexPosition,1.0);}\n";

static const char *aw_land_fragment =
#ifdef PLATFORM_WEB
    "#version 300 es\n"
#else
    "#version 330\n"
#endif
    "precision highp float;\n"
    "in vec3 position; in vec3 normal; in vec4 color; in vec2 uv; out vec4 finalColor; uniform float reveal; uniform vec3 cutEye; uniform vec3 cutTarget; uniform int cutMode;\n"
    "float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
    "float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);"
    "return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.0),f.x),f.y);}\n"
    "void main(){if(uv.x>reveal)discard;"
    "if(cutMode==2 && abs(uv.y-3.0)<0.1)discard;"
    "if(cutMode>0 && position.y>cutTarget.y+0.15){vec3 ray=cutTarget-cutEye;float t=dot(position-cutEye,ray)/dot(ray,ray);"
    "float r=length(position-(cutEye+clamp(t,0.0,1.0)*ray));"
    "if(t>0.0 && t<1.0 && r<3.6){float screen=fract(dot(floor(gl_FragCoord.xy),vec2(0.5,0.25)));"
    "if(r<2.7 || screen>smoothstep(2.7,3.6,r))discard;}}vec3 n=normalize(normal);"
    "float light=max(dot(n,normalize(vec3(-0.55,0.85,-0.4))),0.0);"
    "float grain=noise(position.xz*8.0)*0.12+noise(position.xz*1.8)*0.16+noise(position.xz*0.22)*0.2;"
    "vec3 base=color.rgb;"
    "if(uv.y>4.5){float heat=noise(position.xz*1.5);base=mix(vec3(0.14,0.12,0.10),vec3(1.0,0.30,0.035),smoothstep(0.47,0.68,heat));}"
    "if(uv.y<0.5){base*=0.75+grain; float mottling=smoothstep(0.45,0.74,noise(position.xz*0.15));"
    "base=mix(base,base*vec3(0.77,0.79,0.7),mottling*0.45);"
    "if(n.y<0.65){float strata=0.78+0.22*smoothstep(0.1,0.6,fract(position.y*2.7+noise(position.xz*0.7)*0.6));"
    "base*=strata*(0.65+0.35*smoothstep(-0.7,4.2,position.y));}}"
    "vec3 lit=base*(vec3(0.36,0.43,0.49)+light*vec3(0.75,0.67,0.50));"
    "if((uv.y>1.5 && uv.y<2.5)||uv.y>4.5)lit=mix(lit,base,0.76);"
    "float fog=smoothstep(75.0,160.0,length(position.xz-vec2(64.0)));"
    "finalColor=vec4(mix(lit,vec3(0.055,0.10,0.13),fog*0.7),color.a);}\n";

static const char *aw_water_fragment =
#ifdef PLATFORM_WEB
    "#version 300 es\n"
#else
    "#version 330\n"
#endif
    "precision highp float;\n"
    "in vec3 position;in vec3 normal;in vec4 color;in vec2 uv;out vec4 finalColor;"
    "uniform float time;uniform sampler2D texture0;\n"
    "float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
    "float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);"
    "return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.0),f.x),f.y);}\n"
    "void main(){vec2 p=position.xz;vec2 t=p/128.0;float coast=texture(texture0,t).r;"
    "float n=noise(p*0.58+vec2(time*0.055,-time*0.025));"
    "float ripple=sin(p.x*2.5+p.y*1.3+n*5.0+time*0.7)*0.5+0.5;"
    "vec3 deep=vec3(0.035,0.092,0.12),shallow=vec3(0.105,0.235,0.26);"
    "vec3 base=mix(deep,shallow,clamp(coast*0.85+n*0.14,0.0,1.0));"
    "float foam=pow(ripple,18.0)*smoothstep(0.28,0.8,coast);"
    "base+=vec3(0.3,0.43,0.43)*foam*0.085;"
    "base+=pow(ripple,30.0)*0.007;"
    "float edge=smoothstep(48.0,85.0,length(p-vec2(64.0)));"
    "finalColor=vec4(mix(base,vec3(0.028,0.049,0.069),edge),1.0);}\n";

static float aw_y(float height) {
    return 1.8f+(height-1.0f)*3.0f;
}

static float aw_ground_y(const AwMap *m,int node,float fx,float fz){
    return aw_y((node>=AW_CELLS?4:aw_surface_q(m,node,fx,fz))/4.0f);
}
static Vector3 aw_center(const AwMap *m,int node){
    int c=node%AW_CELLS;
    return (Vector3){(c%AW_SIZE+0.5f)*AW_UNIT,aw_ground_y(m,node,0.5f,0.5f),(c/AW_SIZE+0.5f)*AW_UNIT};
}
static void aw_reserve(AwBuilder *b,int n) {
    if(b->count+n<=b->capacity)return;
    int capacity=b->capacity ? b->capacity*2 : 4096;
    while(capacity<b->count+n)capacity*=2;
    b->positions=realloc(b->positions,(size_t)capacity*sizeof(Vector3));
    b->normals=realloc(b->normals,(size_t)capacity*sizeof(Vector3));
    b->uv=realloc(b->uv,(size_t)capacity*sizeof(Vector2));
    b->colors=realloc(b->colors,(size_t)capacity*sizeof(Color));
    if(!b->positions||!b->normals||!b->uv||!b->colors){fprintf(stderr,"Mesh allocation failed\n");exit(2);}
    b->capacity=capacity;
}

static void aw_triangle(AwBuilder *b,Vector3 a,Vector3 c,Vector3 d,Color color,float rank,float kind) {
    aw_reserve(b,3);
    Vector3 normal=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(c,a),Vector3Subtract(d,a)));
    Vector3 v[3]={a,c,d};
    for(int i=0;i<3;i++){
        int n=b->count++;
        b->positions[n]=v[i];b->normals[n]=normal;b->colors[n]=color;b->uv[n]=(Vector2){rank,kind};
    }
}

static void aw_top(AwBuilder *b,Vector3 *v,int count,Color color,float rank,float kind) {
    for(int i=1;i<count-1;i++){
        Vector3 normal=Vector3CrossProduct(Vector3Subtract(v[i],v[0]),Vector3Subtract(v[i+1],v[0]));
        if(normal.y>=0)aw_triangle(b,v[0],v[i],v[i+1],color,rank,kind);
        else aw_triangle(b,v[0],v[i+1],v[i],color,rank,kind);
    }
}

static Mesh aw_upload(AwBuilder *b) {
    Mesh mesh={0};
    mesh.vertexCount=b->count;mesh.triangleCount=b->count/3;
    mesh.vertices=(float*)b->positions;mesh.normals=(float*)b->normals;
    mesh.texcoords=(float*)b->uv;mesh.colors=(unsigned char*)b->colors;
    UploadMesh(&mesh,false);
    memset(b,0,sizeof(*b));
    return mesh;
}

static void aw_rock(AwBuilder *b,Vector3 p,float radius,float height,uint32_t seed,Color color,float rank,float kind) {
    Vector3 base[6],ring[6];
    float angle=(seed%100)*0.1f;
    for(int i=0;i<6;i++){
        float theta=angle+(float)i*2*PI/6;
        float r=radius*(0.78f+(aw_hash(seed+i)%100)*0.004f);
        base[i]=(Vector3){p.x+cosf(theta)*r,p.y,p.z+sinf(theta)*r};
        ring[i]=(Vector3){p.x+cosf(theta)*r*0.68f,p.y+height*0.55f,p.z+sinf(theta)*r*0.68f};
    }
    Vector3 peak={p.x+radius*0.16f,p.y+height,p.z-radius*0.2f};
    for(int i=0;i<6;i++){
        int j=(i+1)%6;
        Vector3 surface[4]={base[i],ring[i],ring[j],base[j]};
        aw_triangle(b,surface[0],surface[1],surface[2],color,rank,kind);
        aw_triangle(b,surface[0],surface[2],surface[3],color,rank,kind);
        aw_triangle(b,ring[i],peak,ring[j],color,rank,kind);
    }
}

static const Color aw_palette[AW_TILES]={
    {103,142,83,255},{58,109,75,255},{140,112,80,255},{203,171,107,255},
    {119,127,131,255},{209,225,230,255},{120,190,209,255},{95,82,65,255},
    {67,133,143,255},{32,68,82,255},{242,91,28,255},{91,103,102,255}
};
static void aw_quad(AwBuilder*b,Vector3 a,Vector3 c,Vector3 d,Vector3 e,Color color,float rank,float kind){
    aw_triangle(b,a,c,d,color,rank,kind);aw_triangle(b,a,d,e,color,rank,kind);
    aw_triangle(b,a,d,c,color,rank,kind);aw_triangle(b,a,e,d,color,rank,kind);
}
static void aw_face(AwBuilder*b,Vector3 a,Vector3 c,float lowa,float lowc,Color color,float rank,float kind){
    if(a.y<=lowa+0.001f&&c.y<=lowc+0.001f)return;
    Vector3 d=c,e=a;d.y=fminf(c.y,lowc);e.y=fminf(a.y,lowa);
    aw_quad(b,a,c,d,e,color,rank,kind);
}
static void aw_destroy_scene(AwScene*s){
    if(!s->built)return;
    UnloadMesh(s->terrain);UnloadMesh(s->scenery);UnloadMesh(s->overlay);UnloadMesh(s->water);
    MemFree(s->land_material.maps);MemFree(s->water_material.maps);
    UnloadShader(s->land_shader);UnloadShader(s->water_shader);UnloadTexture(s->coast);
    memset(s,0,sizeof(*s));
}
static void aw_build_scene(AwScene*s,const AwMap*m){
    aw_destroy_scene(s);AwBuilder terrain={0},scenery={0},overlay={0},water={0};
    static const int edges[4][2]={{0,1},{1,2},{3,2},{0,3}};
    for(int index=0;index<AW_CELLS;index++){
        int c=m->order[index],x=c%AW_SIZE,z=c/AW_SIZE;float rank=index;
        const AwCell*t=&m->cells[c];int mat=t->material;Color ground=aw_palette[mat];
        Vector3 v[4]={{x*AW_UNIT,0,z*AW_UNIT},{(x+1)*AW_UNIT,0,z*AW_UNIT},{(x+1)*AW_UNIT,0,(z+1)*AW_UNIT},{x*AW_UNIT,0,(z+1)*AW_UNIT}};
        for(int k=0;k<4;k++)v[k].y=aw_y(t->q[k]/4.0f);
        float roof=t->tunnel&&!t->portal?3:0;
        aw_top(&terrain,v,4,ground,rank,roof?roof:mat==AW_LAVA?5:0);
        for(int d=0;d<4;d++){
            int n=aw_neighbor(c,d),a=edges[d][0],b=edges[d][1],e=(d+2)%4;
            float la=n<0?-4:aw_y(m->cells[n].q[edges[e][0]]/4.0f);
            float lb=n<0?-4:aw_y(m->cells[n].q[edges[e][1]]/4.0f);
            Color cliff=mat==AW_SNOW||mat==AW_ICE?(Color){111,135,146,255}:(Color){99,97,89,255};
            /* Subtract the actual tunnel aperture from exposed column faces. */
            if(t->tunnel&&!t->portal&&n>=0&&m->cells[n].tunnel){
                aw_face(&terrain,v[a],v[b],fmaxf(la,aw_y(2.5f)),fmaxf(lb,aw_y(2.5f)),cliff,rank,3);
                Vector3 lowa=v[a],lowb=v[b];lowa.y=lowb.y=aw_y(1);aw_face(&terrain,lowa,lowb,la,lb,cliff,rank,0);
            }else aw_face(&terrain,v[a],v[b],la,lb,cliff,rank,roof);
        }
        if(t->tunnel&&!t->portal){
            Vector3 floor[4];for(int k=0;k<4;k++){floor[k]=v[k];floor[k].y=aw_y(1);}
            aw_top(&terrain,floor,4,(Color){102,115,113,255},rank,0);
            for(int k=0;k<4;k++)floor[k].y=aw_y(2.5f);
            aw_quad(&terrain,floor[0],floor[1],floor[2],floor[3],(Color){72,82,87,255},rank,3);
            for(int d=0;d<4;d++){
                int n=aw_neighbor(c,d);if(n>=0&&m->cells[n].tunnel)continue;
                int a=edges[d][0],b=edges[d][1];aw_face(&terrain,floor[a],floor[b],aw_y(1),aw_y(1),(Color){82,92,94,255},rank,0);
            }
            Vector3 light=aw_center(m,c+AW_CELLS);light.y+=0.09f;
            aw_rock(&scenery,light,0.11f,0.16f,c,(Color){94,244,216,255},rank,2);
        }
        if(mat==AW_DEEP){
            for(int k=0;k<4;k++)v[k].y=-0.12f;aw_top(&water,v,4,WHITE,rank,0);
        }
        if(mat==AW_SHALLOW||mat==AW_ICE){
            Vector3 p=aw_center(m,c);p.y+=0.025f;
            for(int k=0;k<3;k++){
                Vector3 a={p.x-0.6f,p.y,p.z-0.6f+k*0.5f},b={p.x+0.5f,p.y,p.z-0.63f+k*0.5f},d=b,e=a;d.z+=0.04f;e.z+=0.04f;
                Vector3 strip[4]={a,b,d,e};aw_top(&scenery,strip,4,(Color){144,202,210,255},rank,1);
            }
        }
        int canonical=m->options.symmetry&&c>=AW_CELLS/2?AW_CELLS-1-c:c;
        uint32_t h=aw_hash(m->seed^(uint32_t)canonical*8191u);Vector3 p=aw_center(m,c);
        if(!t->road&&!t->tunnel){
            float jitter=((h&255)/255.0f-0.5f)*0.75f;if(m->options.symmetry&&c>=AW_CELLS/2)jitter=-jitter;p.x+=jitter;p.z-=jitter;
            if(mat==AW_FOREST){
                aw_rock(&scenery,p,0.15f,0.9f,h,(Color){79,67,51,255},rank,0);p.y+=0.5f;
                aw_rock(&scenery,p,0.77f,1.8f+(h%7)*0.12f,h,(Color){49,94,67,255},rank,0);p.y+=0.6f;
                aw_rock(&scenery,p,0.54f,1.5f,h,(Color){67,121,78,255},rank,0);
            }else if((mat==AW_ROCK||mat==AW_SNOW||mat==AW_DIRT)&&h%4==0){
                aw_rock(&scenery,p,0.28f+(h%13)*0.035f,0.3f+(h%9)*0.1f,h,ground,rank,0);
            }else if(mat==AW_GRASS&&h%3==0){aw_rock(&scenery,p,0.34f,0.26f,h,(Color){97,151,70,255},rank,0);}
        }
        if(m->walkable[c]){
            for(int k=0;k<4;k++){v[k].y=aw_y(t->q[k]/4.0f)+0.04f;}
            Color color=m->reachable[c]?(Color){66,236,178,105}:(Color){246,132,82,105};aw_top(&overlay,v,4,color,rank,2);
        }
    }
    for(int r=0;r<4;r++){
        Vector3 p=aw_center(m,m->resources[r]);
        for(int i=0;i<5;i++){Vector3 q=p;q.x+=cosf(i*1.4f)*0.65f;q.z+=sinf(i*1.4f)*0.65f;aw_rock(&scenery,q,0.22f,0.55f+(i%3)*0.2f,i,(Color){64,205,236,255},0,2);}
    }
    s->terrain=aw_upload(&terrain);s->scenery=aw_upload(&scenery);s->overlay=aw_upload(&overlay);s->water=aw_upload(&water);
    s->land_shader=LoadShaderFromMemory(aw_vertex_shader,aw_land_fragment);
    s->water_shader=LoadShaderFromMemory(aw_vertex_shader,aw_water_fragment);
    s->land_reveal=GetShaderLocation(s->land_shader,"reveal");s->water_time=GetShaderLocation(s->water_shader,"time");
    s->cut_eye=GetShaderLocation(s->land_shader,"cutEye");s->cut_target=GetShaderLocation(s->land_shader,"cutTarget");s->cut_mode=GetShaderLocation(s->land_shader,"cutMode");
    if(s->land_reveal<0||s->water_time<0||s->cut_mode<0){fprintf(stderr,"Map Lab shader compilation failed\n");exit(2);}
    s->land_material=LoadMaterialDefault();s->land_material.shader=s->land_shader;
    s->water_material=LoadMaterialDefault();s->water_material.shader=s->water_shader;
    Image coast=GenImageColor(64,64,(Color){90,90,90,255});s->coast=LoadTextureFromImage(coast);UnloadImage(coast);
    s->water_material.maps[MATERIAL_MAP_DIFFUSE].texture=s->coast;s->built=1;
}
static void aw_draw_scene(AwScene*s,float reveal,float time,int overlay,Vector3 eye,Vector3 target,int cut){
    SetShaderValue(s->land_shader,s->land_reveal,&reveal,SHADER_UNIFORM_FLOAT);
    SetShaderValue(s->land_shader,s->cut_eye,&eye,SHADER_UNIFORM_VEC3);
    SetShaderValue(s->land_shader,s->cut_target,&target,SHADER_UNIFORM_VEC3);
    SetShaderValue(s->land_shader,s->cut_mode,&cut,SHADER_UNIFORM_INT);
    SetShaderValue(s->water_shader,s->water_time,&time,SHADER_UNIFORM_FLOAT);
    Matrix identity=MatrixIdentity();DrawMesh(s->water,s->water_material,identity);DrawMesh(s->terrain,s->land_material,identity);DrawMesh(s->scenery,s->land_material,identity);
    if(overlay)DrawMesh(s->overlay,s->land_material,identity);
}
#endif
