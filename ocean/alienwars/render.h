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
    int land_reveal, water_time;
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
    "in vec3 position; in vec3 normal; in vec4 color; in vec2 uv; out vec4 finalColor; uniform float reveal;\n"
    "float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
    "float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);"
    "return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.0),f.x),f.y);}\n"
    "void main(){if(uv.x>reveal)discard; vec3 n=normalize(normal);"
    "float light=max(dot(n,normalize(vec3(-0.55,0.85,-0.4))),0.0);"
    "float grain=noise(position.xz*8.0)*0.12+noise(position.xz*1.8)*0.16+noise(position.xz*0.22)*0.2;"
    "vec3 base=color.rgb;"
    "if(uv.y<0.5){base*=0.75+grain; float mottling=smoothstep(0.45,0.74,noise(position.xz*0.15));"
    "base=mix(base,base*vec3(0.77,0.79,0.7),mottling*0.45);"
    "if(n.y<0.65){float strata=0.78+0.22*smoothstep(0.1,0.6,fract(position.y*2.7+noise(position.xz*0.7)*0.6));"
    "base*=strata*(0.65+0.35*smoothstep(-0.7,4.2,position.y));}}"
    "vec3 lit=base*(vec3(0.36,0.43,0.49)+light*vec3(0.75,0.67,0.50));"
    "if(uv.y>1.5)lit=mix(lit,base,0.76);"
    "float fog=smoothstep(75.0,160.0,length(position.xz-vec2(48.0)));"
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
    "void main(){vec2 p=position.xz;vec2 t=p/96.0;float coast=texture(texture0,t).r;"
    "float n=noise(p*0.58+vec2(time*0.055,-time*0.025));"
    "float ripple=sin(p.x*2.5+p.y*1.3+n*5.0+time*0.7)*0.5+0.5;"
    "vec3 deep=vec3(0.035,0.092,0.12),shallow=vec3(0.105,0.235,0.26);"
    "vec3 base=mix(deep,shallow,clamp(coast*0.85+n*0.14,0.0,1.0));"
    "float foam=pow(ripple,18.0)*smoothstep(0.28,0.8,coast);"
    "base+=vec3(0.3,0.43,0.43)*foam*0.085;"
    "base+=pow(ripple,30.0)*0.007;"
    "float edge=smoothstep(48.0,85.0,length(p-vec2(48.0)));"
    "finalColor=vec4(mix(base,vec3(0.028,0.049,0.069),edge),1.0);}\n";

static float aw_y(float height) {
    return height < 1.0f ? -0.8f : 1.8f+(height-1.0f)*3.0f;
}

static float aw_ground_y(const AwMap *m, int cell, float fx, float fz) {
    float a=aw_corner_q(m,cell,0), b=aw_corner_q(m,cell,1);
    float c=aw_corner_q(m,cell,2), d=aw_corner_q(m,cell,3);
    return aw_y(((a+(b-a)*fx)*(1-fz)+(d+(c-d)*fx)*fz)/4.0f);
}

static Vector3 aw_center(const AwMap *m,int cell) {
    return (Vector3){(cell%AW_SIZE+0.5f)*AW_UNIT,aw_ground_y(m,cell,0.5f,0.5f),(cell/AW_SIZE+0.5f)*AW_UNIT};
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

static Vector3 aw_grid_point(const AwMap *m,int x,int z) {
    uint32_t h=aw_hash(m->seed ^ (uint32_t)x*9337u ^ (uint32_t)z*1237u);
    /* Shared vertex displacement makes neighboring tile edges coincide. Keep
     * ramp vertices regular so walkable surfaces agree with navigation. */
    int near_ramp=(x>=13&&x<=19&&z>=10&&z<=15)||(x>=29&&x<=35&&z>=34&&z<=39);
    float dx=near_ramp?0.0f:((float)(h&255)/255.0f-0.5f)*0.52f;
    float dz=near_ramp?0.0f:((float)((h>>8)&255)/255.0f-0.5f)*0.52f;
    return (Vector3){x*AW_UNIT+dx,0,z*AW_UNIT+dz};
}

static void aw_wall(AwBuilder *b,Vector3 a,Vector3 c,float bottom,float top,Color color,float rank) {
    Vector3 direction=Vector3Normalize(Vector3Subtract(c,a));
    Vector3 offset={direction.z*0.11f,0,-direction.x*0.11f};
    for(int band=0;band<3;band++){
        float lo=bottom+(top-bottom)*band/3.0f,hi=bottom+(top-bottom)*(band+1)/3.0f;
        Vector3 v[4]={a,c,c,a};
        v[0].y=v[1].y=lo;v[2].y=v[3].y=hi;
        if(band>0){v[0]=Vector3Add(v[0],offset);v[1]=Vector3Add(v[1],offset);}
        if(band<2){v[2]=Vector3Add(v[2],offset);v[3]=Vector3Add(v[3],offset);}
        aw_triangle(b,v[0],v[2],v[1],color,rank,0);
        aw_triangle(b,v[0],v[3],v[2],color,rank,0);
        /* Both orientations cover either contour winding without culling holes. */
        aw_triangle(b,v[0],v[1],v[2],color,rank,0);
        aw_triangle(b,v[0],v[2],v[3],color,rank,0);
    }
}

static void aw_terrace_triangle(AwBuilder *b,Vector3 *points,float *height,int low,Color color,float rank) {
    Vector3 crossings[2];int crossing_count=0;
    float threshold=low+0.5f;
    for(int side=0;side<2;side++){
        Vector3 polygon[6];int count=0;
        for(int i=0;i<3;i++){
            int j=(i+1)%3;
            int inside=side ? height[i]>=threshold : height[i]<threshold;
            int next=side ? height[j]>=threshold : height[j]<threshold;
            if(inside){polygon[count]=points[i];polygon[count++].y=aw_y((float)(low+side));}
            if(inside!=next){
                float t=(threshold-height[i])/(height[j]-height[i]);
                Vector3 point=Vector3Lerp(points[i],points[j],t);
                if(side==0&&crossing_count<2)crossings[crossing_count++]=point;
                point.y=aw_y((float)(low+side));polygon[count++]=point;
            }
        }
        if(low+side>0)aw_top(b,polygon,count,color,rank,0);
    }
    if(crossing_count==2)aw_wall(b,crossings[0],crossings[1],aw_y((float)low),aw_y((float)(low+1)),(Color){108,104,90,255},rank);
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

static void aw_destroy_scene(AwScene *s) {
    if(!s->built)return;
    UnloadMesh(s->terrain);UnloadMesh(s->scenery);UnloadMesh(s->overlay);UnloadMesh(s->water);
    /* Material maps are owned separately; shader/texture handles are shared. */
    MemFree(s->land_material.maps);MemFree(s->water_material.maps);
    UnloadShader(s->land_shader);UnloadShader(s->water_shader);UnloadTexture(s->coast);
    memset(s,0,sizeof(*s));
}

static void aw_build_scene(AwScene *s,const AwMap *m) {
    aw_destroy_scene(s);
    AwBuilder terrain={0},scenery={0},overlay={0},water={0};
    for(int index=0;index<AW_CELLS;index++){
        int cell=m->order[index],x=cell%AW_SIZE,z=cell/AW_SIZE;
        float rank=(float)index;
        AwTile tile=m->tiles[(int)m->tile[cell]];
        Vector3 points[4]={aw_grid_point(m,x,z),aw_grid_point(m,x+1,z),aw_grid_point(m,x+1,z+1),aw_grid_point(m,x,z+1)};
        Color ground={148,126,91,255};
        if(tile.corner[0]==2)ground=(Color){153,139,106,255};
        if(m->ramp[cell]>=0){
            for(int k=0;k<4;k++)points[k].y=aw_y(aw_corner_q(m,cell,k)/4.0f);
            aw_top(&terrain,points,4,(Color){157,137,105,255},rank,0);
            /* Close exposed sides of the ramp volume. */
            for(int edge=0;edge<2;edge++){
                int a=edge?3:0,c=edge?2:1;
                Vector3 v[4]={points[a],points[c],points[c],points[a]};
                v[2].y=v[3].y=1.8f;
                aw_triangle(&terrain,v[0],v[1],v[2],(Color){110,104,89,255},rank,0);
                aw_triangle(&terrain,v[0],v[2],v[3],(Color){110,104,89,255},rank,0);
                aw_triangle(&terrain,v[0],v[2],v[1],(Color){110,104,89,255},rank,0);
                aw_triangle(&terrain,v[0],v[3],v[2],(Color){110,104,89,255},rank,0);
            }
        }else{
            int lo=2,hi=0;
            for(int k=0;k<4;k++){if(tile.corner[k]<lo)lo=tile.corner[k];if(tile.corner[k]>hi)hi=tile.corner[k];}
            if(lo==hi){
                if(lo>0){for(int k=0;k<4;k++)points[k].y=aw_y((float)lo);aw_top(&terrain,points,4,ground,rank,0);}
            }else{
                Vector3 center=Vector3Scale(Vector3Add(Vector3Add(points[0],points[1]),Vector3Add(points[2],points[3])),0.25f);
                float mid=(tile.corner[0]+tile.corner[1]+tile.corner[2]+tile.corner[3])*0.25f;
                /* Resolve exact saddle ties consistently inside the cell. */
                if(mid==lo+0.5f)mid+=((aw_hash(m->seed+cell)&1)?0.12f:-0.12f);
                for(int k=0;k<4;k++){
                    Vector3 tri[3]={points[k],points[(k+1)%4],center};
                    float h[3]={(float)tile.corner[k],(float)tile.corner[(k+1)%4],mid};
                    aw_terrace_triangle(&terrain,tri,h,lo,ground,rank);
                }
            }
        }
        if(m->walkable[cell]){
            Vector3 v[4];
            for(int k=0;k<4;k++){
                v[k]=(Vector3){(x+(k==1||k==2?0.94f:0.06f))*AW_UNIT,
                    aw_ground_y(m,cell,k==1||k==2?0.94f:0.06f,k>=2?0.94f:0.06f)+0.055f,
                    (z+(k>=2?0.94f:0.06f))*AW_UNIT};
            }
            Color tint=m->reachable[cell]?(Color){68,205,179,105}:(Color){239,99,80,150};
            if(m->ramp[cell]>=0)tint=(Color){255,190,80,160};
            aw_top(&overlay,v,4,tint,rank,2);
            uint32_t h=aw_hash(m->seed ^ (uint32_t)cell*7193u);
            int reserved=m->ramp[cell]>=0;
            for(int p=0;p<m->path_length;p++)if(m->path[p]==cell)reserved=1;
            for(int r=0;r<4;r++)if(abs(x-m->resources[r]%AW_SIZE)<2&&abs(z-m->resources[r]/AW_SIZE)<2)reserved=1;
            for(int r=0;r<2;r++)if(abs(x-m->spawns[r]%AW_SIZE)<3&&abs(z-m->spawns[r]/AW_SIZE)<3)reserved=1;
            if(!reserved && h%7==0){
                Vector3 p=aw_center(m,cell);
                p.x+=(float)((h>>8)%100)/160.0f-0.3f;p.z+=(float)((h>>16)%100)/160.0f-0.3f;
                aw_rock(&scenery,p,0.2f+(h%11)*0.035f,0.18f+(h%13)*0.065f,h,(Color){111,112,98,255},rank,1);
                p.x+=0.55f;p.z-=0.4f;
                aw_rock(&scenery,p,0.15f,0.18f,h+1,(Color){128,116,92,255},rank,1);
            }
        }
    }
    /* Resource seams and architectural markers are cosmetic; they never alter
     * the navigation grid. All selected resource cells are validated reachable. */
    for(int r=0;r<4;r++){
        int cell=m->resources[r];Vector3 center=aw_center(m,cell);
        int rank=0;while(m->order[rank]!=cell)rank++;
        for(int i=0;i<9;i++){
            float a=PI*0.15f+i*0.29f;
            Vector3 p={center.x+cosf(a)*2.5f,center.y+0.04f,center.z+sinf(a)*2.0f};
            aw_rock(&scenery,p,0.18f+(i%3)*0.06f,0.65f+(i%4)*0.21f,(uint32_t)(r*20+i),(Color){48,205,231,255},(float)rank,2);
        }
    }
    /* Broad water plane fades into the page's dark horizon. */
    Vector3 plane[4]={{-60,0,-60},{156,0,-60},{156,0,156},{-60,0,156}};
    aw_top(&water,plane,4,WHITE,0,0);
    s->terrain=aw_upload(&terrain);s->scenery=aw_upload(&scenery);
    s->overlay=aw_upload(&overlay);s->water=aw_upload(&water);
    s->land_shader=LoadShaderFromMemory(aw_vertex_shader,aw_land_fragment);
    s->water_shader=LoadShaderFromMemory(aw_vertex_shader,aw_water_fragment);
    s->land_material=LoadMaterialDefault();s->land_material.shader=s->land_shader;
    s->water_material=LoadMaterialDefault();s->water_material.shader=s->water_shader;
    s->land_reveal=GetShaderLocation(s->land_shader,"reveal");
    s->water_time=GetShaderLocation(s->water_shader,"time");
    if(!s->land_shader.id || !s->water_shader.id || s->land_reveal<0 || s->water_time<0){
        fprintf(stderr,"Map Lab terrain shaders failed to initialize\n");
        exit(2);
    }
    Image mask=GenImageColor(64,64,BLACK);
    Color *pixels=mask.data;
    for(int c=0;c<64*64;c++){
        int near=0;
        for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
            int x=(c%64)*AW_SIZE/64+dx,z=(c/64)*AW_SIZE/64+dz;
            if(x<0||z<0||x>=AW_SIZE||z>=AW_SIZE)continue;
            AwTile t=m->tiles[(int)m->tile[z*AW_SIZE+x]];
            if(t.corner[0]||t.corner[1]||t.corner[2]||t.corner[3]){
                int v=255-55*(abs(dx)+abs(dz));if(v>near)near=v;
            }
        }
        pixels[c]=(Color){(unsigned char)near,(unsigned char)near,(unsigned char)near,255};
    }
    s->coast=LoadTextureFromImage(mask);UnloadImage(mask);
    SetTextureFilter(s->coast,TEXTURE_FILTER_BILINEAR);SetTextureWrap(s->coast,TEXTURE_WRAP_CLAMP);
    s->water_material.maps[MATERIAL_MAP_DIFFUSE].texture=s->coast;
    s->built=1;
}

static void aw_draw_scene(AwScene *s,float reveal,float time,int overlay) {
    SetShaderValue(s->land_shader,s->land_reveal,&reveal,SHADER_UNIFORM_FLOAT);
    SetShaderValue(s->water_shader,s->water_time,&time,SHADER_UNIFORM_FLOAT);
    DrawMesh(s->water,s->water_material,MatrixIdentity());
    DrawMesh(s->terrain,s->land_material,MatrixIdentity());
    DrawMesh(s->scenery,s->land_material,MatrixIdentity());
    if(overlay)DrawMesh(s->overlay,s->land_material,MatrixIdentity());
}

#endif
