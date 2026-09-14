#ifndef ALIENWARS_RENDER_H
#define ALIENWARS_RENDER_H
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "volume.h"
#include <math.h>

#define AW_UNIT 2.0f

typedef struct {
    Vector3 *positions, *normals;
    Vector2 *uv;
    Color *colors;
    int count, capacity;
} AwBuilder;

typedef struct {
    Mesh terrain, scenery, overlay, water, tunnels, tunnel_lights;
    Material land_material, water_material;
    Shader land_shader, water_shader;
    Texture2D coast;
    int land_reveal, water_time, cut_eye, cut_target, cut_mode, tunnel_view;
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
    "in vec3 position; in vec3 normal; in vec4 color; in vec2 uv; out vec4 finalColor; uniform float reveal; uniform vec3 cutEye; uniform vec3 cutTarget; uniform int cutMode; uniform int tunnelView;\n"
    "float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
    "float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);"
    "return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.0),f.x),f.y);}\n"
    "void main(){if(uv.x>reveal)discard;"
    "if(tunnelView==1 && normal.y< -0.1)discard;"
    "if(cutMode==2 && position.y>cutTarget.y+1.5)discard;"
    "if(cutMode>0 && position.y>cutTarget.y+0.15){vec3 ray=cutTarget-cutEye;float t=dot(position-cutEye,ray)/dot(ray,ray);"
    "float r=length(position-(cutEye+clamp(t,0.0,1.0)*ray));"
    "if(t>0.0 && t<1.0 && r<3.6){float screen=fract(dot(floor(gl_FragCoord.xy),vec2(0.5,0.25)));"
    "if(r<2.7 || screen>smoothstep(2.7,3.6,r))discard;}}vec3 n=normalize(normal);if(tunnelView>0 && !gl_FrontFacing)n=-n;"
    "float light=max(dot(n,normalize(vec3(-0.55,0.85,-0.4))),0.0);"
    "float grain=noise(position.xz*8.0)*0.12+noise(position.xz*1.8)*0.16+noise(position.xz*0.22)*0.2;"
    "vec3 base=color.rgb;"
    "float lava=clamp(uv.y-10.0,0.0,1.0);if(lava>0.0){float heat=noise(position.xz*1.5);vec3 magma=mix(vec3(0.14,0.12,0.10),vec3(1.0,0.30,0.035),smoothstep(0.47,0.68,heat));base=mix(base,magma,lava);}"
    "if(uv.y<0.5||uv.y>9.5){base*=0.75+grain; float mottling=smoothstep(0.45,0.74,noise(position.xz*0.15));"
    "base=mix(base,base*vec3(0.77,0.79,0.7),mottling*0.45);"
    "if(n.y<0.65){base=mix(base,vec3(0.34,0.33,0.29),0.68);float strata=0.88+0.12*smoothstep(0.1,0.6,fract(position.y*0.8+noise(position.xz*0.65)*2.0));"
    "base*=strata*(0.65+0.35*smoothstep(-0.7,4.2,position.y));}}"
    "vec3 lit=base*(vec3(0.36,0.43,0.49)+light*vec3(0.75,0.67,0.50));"
    "if(uv.y>1.5 && uv.y<2.5)lit=mix(lit,base,0.76);lit=mix(lit,base,lava*0.76);"
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
    "void main(){vec2 p=position.xz;vec2 t=p/128.0;vec4 shore=texture(texture0,t);if(shore.a<0.5)discard;float coast=shore.r;"
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
    int c=aw_node_cell(m,node);float q=node>=AW_CELLS?m->cave[node-AW_CELLS].q:aw_surface_q(m,node,fx,fz);
    if(m->cave_bin_count[c])q=aw_support_q(m,c%64+fx,c/64+fz,q);
    return aw_y(q/4.0f);
}
static Vector3 aw_center(const AwMap *m,int node){
    int c=aw_node_cell(m,node);
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
static float aw_world_q(const AwMap*m,float x,float z){
    x=fminf(AW_SIZE-0.0001f,fmaxf(0,x));z=fminf(AW_SIZE-0.0001f,fmaxf(0,z));
    int c=(int)z*AW_SIZE+(int)x;return aw_surface_q(m,c,x-(int)x,z-(int)z);
}
static Vector3 aw_surface_normal(const AwMap*m,float x,float z){
    const float e=0.035f;
    float dx=(aw_world_q(m,x+e,z)-aw_world_q(m,x-e,z))*0.75f;
    float dz=(aw_world_q(m,x,z+e)-aw_world_q(m,x,z-e))*0.75f;
    return Vector3Normalize((Vector3){-dx,2*e*AW_UNIT,-dz});
}
static Color aw_vertex_color(const AwMap*m,int vx,int vz){
    int r=0,g=0,b=0,n=0;
    for(int dz=-1;dz<=0;dz++)for(int dx=-1;dx<=0;dx++){
        int x=vx+dx,z=vz+dz;if(x<0||z<0||x>=AW_SIZE||z>=AW_SIZE)continue;
        int c=z*AW_SIZE+x,mat=m->cells[c].material;
        /* The seabed shares the adjacent soil tint; the water is separate. */
        Color color=aw_palette[mat==AW_DEEP?AW_SAND:mat];r+=color.r;g+=color.g;b+=color.b;n++;
    }
    return (Color){r/n,g/n,b/n,255};
}
static float aw_vertex_lava(const AwMap*m,int vx,int vz){
    int lava=0,n=0;
    for(int dz=-1;dz<=0;dz++)for(int dx=-1;dx<=0;dx++){
        int x=vx+dx,z=vz+dz;if(x<0||z<0||x>=AW_SIZE||z>=AW_SIZE)continue;
        lava+=m->cells[z*AW_SIZE+x].material==AW_LAVA;n++;
    }
    return (float)lava/n;
}
static Color aw_tile_color(const Color colors[4],float x,float z){
    return (Color){
        (uint8_t)aw_bilinear(colors[0].r,colors[1].r,colors[2].r,colors[3].r,x,z),
        (uint8_t)aw_bilinear(colors[0].g,colors[1].g,colors[2].g,colors[3].g,x,z),
        (uint8_t)aw_bilinear(colors[0].b,colors[1].b,colors[2].b,colors[3].b,x,z),255};
}
typedef struct {AwBuilder*b,*tunnels;const AwMap*m;float rank;} AwVolumeRender;
static void aw_render_volume_triangle(void*opaque,AwVolumePoint a,AwVolumePoint b,AwVolumePoint c){
    AwVolumeRender*r=opaque;const AwMap*m=r->m;AwVolumePoint p[3]={a,b,c};aw_reserve(r->b,3);
    for(int k=0;k<3;k++){
        float x=p[k].x,z=p[k].z,q=p[k].q;
        int cx=aw_clamp((int)x,0,63),cz=aw_clamp((int)z,0,63);float fx=x-cx,fz=z-cz;
        Color colors[4]={aw_vertex_color(m,cx,cz),aw_vertex_color(m,cx+1,cz),aw_vertex_color(m,cx+1,cz+1),aw_vertex_color(m,cx,cz+1)};
        float lava=aw_bilinear(aw_vertex_lava(m,cx,cz),aw_vertex_lava(m,cx+1,cz),aw_vertex_lava(m,cx+1,cz+1),aw_vertex_lava(m,cx,cz+1),fx,fz);
        Color color=aw_tile_color(colors,fx,fz);Vector3 normal=aw_surface_normal(m,x,z);
        if(m->cave_bin_count[cz*64+cx]&&q<aw_height_q(m,x,z)-0.08f){
            const float e=0.02f;
            float nx=aw_density(m,x-e,q,z)-aw_density(m,x+e,q,z);
            float ny=aw_density(m,x,q-e,z)-aw_density(m,x,q+e,z);
            float nz=aw_density(m,x,q,z-e)-aw_density(m,x,q,z+e);
            normal=Vector3Normalize((Vector3){nx/AW_UNIT,ny/0.75f,nz/AW_UNIT});
            color=(Color){112,119,105,255};lava=0;
        }
        int n=r->b->count++;r->b->positions[n]=(Vector3){x*AW_UNIT,aw_y(q/4),z*AW_UNIT};r->b->normals[n]=normal;r->b->colors[n]=color;r->b->uv[n]=(Vector2){r->rank,10+lava};
    }
    /* Keep actual excavated triangles for inspection. No proxy boxes or second
     * mesher: the isolated shell uses the same vertices as the world surface.
     * Include entrance floors where the terrain and passage floor coincide. */
    float x=(a.x+b.x+c.x)/3,z=(a.z+b.z+c.z)/3,q=(a.q+b.q+c.q)/3;
    int cell=aw_clamp((int)z,0,63)*64+aw_clamp((int)x,0,63),excavated=0;
    if(m->cave_bin_count[cell]){
        for(int k=0;k<3;k++)excavated|=p[k].q<aw_height_q(m,p[k].x,p[k].z)-0.001f;
        excavated|=aw_cave_field(m,x,q+0.1f,z)<0;
    }
    if(excavated){
        AwBuilder*t=r->tunnels;aw_reserve(t,3);
        for(int k=r->b->count-3;k<r->b->count;k++){
            int n=t->count++;t->positions[n]=r->b->positions[k];t->normals[n]=r->b->normals[k];
            t->colors[n]=r->b->colors[k];t->uv[n]=r->b->uv[k];
        }
    }
}
static void aw_destroy_scene(AwScene*s){
    if(!s->built)return;
    if(s->tunnels.vertexCount)UnloadMesh(s->tunnels);
    if(s->tunnel_lights.vertexCount)UnloadMesh(s->tunnel_lights);
    UnloadMesh(s->terrain);UnloadMesh(s->scenery);UnloadMesh(s->overlay);UnloadMesh(s->water);
    MemFree(s->land_material.maps);MemFree(s->water_material.maps);
    UnloadShader(s->land_shader);UnloadShader(s->water_shader);UnloadTexture(s->coast);
    memset(s,0,sizeof(*s));
}
static void aw_build_scene(AwScene*s,const AwMap*m){
    aw_destroy_scene(s);AwBuilder terrain={0},scenery={0},overlay={0},water={0},tunnels={0},tunnel_lights={0};
    for(int index=0;index<AW_CELLS;index++){
        int c=m->order[index],x=c%AW_SIZE,z=c/AW_SIZE;float rank=index;
        const AwCell*t=&m->cells[c];int mat=t->material;Color ground=aw_palette[mat];
        Vector3 v[4]={{x*AW_UNIT,0,z*AW_UNIT},{(x+1)*AW_UNIT,0,z*AW_UNIT},{(x+1)*AW_UNIT,0,(z+1)*AW_UNIT},{x*AW_UNIT,0,(z+1)*AW_UNIT}};
        for(int k=0;k<4;k++)v[k].y=aw_y(t->q[k]/4.0f);
        AwVolumeRender render={&terrain,&tunnels,m,rank};aw_volume_cell(m,c,aw_render_volume_triangle,&render);

        if(mat==AW_SHALLOW||mat==AW_ICE){
            Vector3 p=aw_center(m,c);p.y+=0.025f;
            for(int k=0;k<3;k++){
                Vector3 a={p.x-0.6f,p.y,p.z-0.6f+k*0.5f},b={p.x+0.5f,p.y,p.z-0.63f+k*0.5f},d=b,e=a;d.z+=0.04f;e.z+=0.04f;
                Vector3 strip[4]={a,b,d,e};aw_top(&scenery,strip,4,(Color){144,202,210,255},rank,1);
            }
        }
        int canonical=m->options.symmetry&&c>=AW_CELLS/2?AW_CELLS-1-c:c;
        uint32_t h=aw_hash(m->seed^(uint32_t)canonical*8191u);Vector3 p=aw_center(m,c);
        if(!t->road&&!t->tunnel&&m->walkable[c]){
            float jitter=((h&255)/255.0f-0.5f)*0.75f;if(m->options.symmetry&&c>=AW_CELLS/2)jitter=-jitter;p.x+=jitter;p.z-=jitter;
            if(mat==AW_FOREST){
                aw_rock(&scenery,p,0.15f,0.9f,h,(Color){79,67,51,255},rank,1);p.y+=0.5f;
                aw_rock(&scenery,p,0.77f,1.8f+(h%7)*0.12f,h,(Color){49,94,67,255},rank,1);p.y+=0.6f;
                aw_rock(&scenery,p,0.54f,1.5f,h,(Color){67,121,78,255},rank,1);
            }else if((mat==AW_ROCK||mat==AW_SNOW||mat==AW_DIRT)&&h%4==0){
                aw_rock(&scenery,p,0.28f+(h%13)*0.035f,0.3f+(h%9)*0.1f,h,ground,rank,0);
            }else if(mat==AW_GRASS&&h%3==0){aw_rock(&scenery,p,0.34f,0.26f,h,(Color){97,151,70,255},rank,0);}
        }
        if(m->walkable[c]){
            for(int k=0;k<4;k++){v[k].y=aw_y(t->q[k]/4.0f)+0.04f;}
            Color color=m->reachable[c]?(Color){66,236,178,105}:(Color){246,132,82,105};aw_top(&overlay,v,4,color,rank,2);
        }
    }
    for(int i=0;i<m->cave_count;i++)if(i%3==0){
        Vector3 p=aw_center(m,AW_CELLS+i);p.y+=0.06f;
        aw_rock(&scenery,p,0.10f,0.14f,i,(Color){94,244,216,255},0,2);
        aw_rock(&tunnel_lights,p,0.10f,0.14f,i,(Color){94,244,216,255},0,2);
    }
    for(int r=0;r<4;r++){
        Vector3 p=aw_center(m,m->resources[r]);
        for(int i=0;i<5;i++){Vector3 q=p;q.x+=cosf(i*1.4f)*0.65f;q.z+=sinf(i*1.4f)*0.65f;aw_rock(&scenery,q,0.22f,0.55f+(i%3)*0.2f,i,(Color){64,205,236,255},0,2);}
    }
    Vector3 sea[4]={{-60,-0.12f,-60},{188,-0.12f,-60},{188,-0.12f,188},{-60,-0.12f,188}};
    aw_top(&water,sea,4,WHITE,0,0);
    if(tunnels.count)s->tunnels=aw_upload(&tunnels);
    if(tunnel_lights.count)s->tunnel_lights=aw_upload(&tunnel_lights);
    s->terrain=aw_upload(&terrain);s->scenery=aw_upload(&scenery);s->overlay=aw_upload(&overlay);s->water=aw_upload(&water);
    s->land_shader=LoadShaderFromMemory(aw_vertex_shader,aw_land_fragment);
    s->water_shader=LoadShaderFromMemory(aw_vertex_shader,aw_water_fragment);
    s->land_reveal=GetShaderLocation(s->land_shader,"reveal");s->water_time=GetShaderLocation(s->water_shader,"time");
    s->cut_eye=GetShaderLocation(s->land_shader,"cutEye");s->cut_target=GetShaderLocation(s->land_shader,"cutTarget");s->cut_mode=GetShaderLocation(s->land_shader,"cutMode");
    s->tunnel_view=GetShaderLocation(s->land_shader,"tunnelView");
    if(s->land_reveal<0||s->water_time<0||s->cut_mode<0||s->tunnel_view<0){fprintf(stderr,"Map Lab shader compilation failed\n");exit(2);}
    s->land_material=LoadMaterialDefault();s->land_material.shader=s->land_shader;
    s->water_material=LoadMaterialDefault();s->water_material.shader=s->water_shader;
    Image coast=GenImageColor(AW_SIZE,AW_SIZE,BLACK);Color*pixels=coast.data;
    for(int z=0;z<AW_SIZE;z++)for(int x=0;x<AW_SIZE;x++){
        int near=0;for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++){
            int nx=x+dx,nz=z+dz;if(nx<0||nz<0||nx>=AW_SIZE||nz>=AW_SIZE)continue;
            if(aw_world_q(m,nx+0.5f,nz+0.5f)>1.5f){int value=255-45*(abs(dx)+abs(dz));if(value>near)near=value;}
        }
        pixels[z*AW_SIZE+x]=(Color){near,near,near,aw_world_q(m,x+0.5f,z+0.5f)>2?0:255};
    }
    s->coast=LoadTextureFromImage(coast);UnloadImage(coast);SetTextureFilter(s->coast,TEXTURE_FILTER_BILINEAR);SetTextureWrap(s->coast,TEXTURE_WRAP_CLAMP);
    s->water_material.maps[MATERIAL_MAP_DIFFUSE].texture=s->coast;s->built=1;
}
static void aw_draw_scene(AwScene*s,float reveal,float time,int overlay,Vector3 eye,Vector3 target,int cut,int tunnel_view){
    if(tunnel_view){reveal=AW_CELLS;cut=0;}
    SetShaderValue(s->land_shader,s->tunnel_view,&tunnel_view,SHADER_UNIFORM_INT);
    SetShaderValue(s->land_shader,s->land_reveal,&reveal,SHADER_UNIFORM_FLOAT);
    SetShaderValue(s->land_shader,s->cut_eye,&eye,SHADER_UNIFORM_VEC3);
    SetShaderValue(s->land_shader,s->cut_target,&target,SHADER_UNIFORM_VEC3);
    SetShaderValue(s->land_shader,s->cut_mode,&cut,SHADER_UNIFORM_INT);
    SetShaderValue(s->water_shader,s->water_time,&time,SHADER_UNIFORM_FLOAT);
    Matrix identity=MatrixIdentity();
    if(tunnel_view){
        /* Cave faces point into their void. Two-sided inspection also exposes
         * the outside of the arch when the viewer orbits around the shell. */
        rlDrawRenderBatchActive();rlDisableBackfaceCulling();
        if(s->tunnels.vertexCount)DrawMesh(s->tunnels,s->land_material,identity);
        if(s->tunnel_lights.vertexCount)DrawMesh(s->tunnel_lights,s->land_material,identity);
        rlEnableBackfaceCulling();return;
    }
    DrawMesh(s->water,s->water_material,identity);DrawMesh(s->terrain,s->land_material,identity);DrawMesh(s->scenery,s->land_material,identity);
    if(overlay)DrawMesh(s->overlay,s->land_material,identity);
}
#endif
