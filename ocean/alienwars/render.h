#ifndef ALIENWARS_RENDER_H
#define ALIENWARS_RENDER_H
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "volume.h"
#include "occlusion.h"
#include "detail.h"
#include <math.h>

#define AW_UNIT 2.0f

typedef struct {
    Vector3 *positions, *normals;
    Vector2 *uv;
    Color *colors;
    int count, capacity;
} AwBuilder;

typedef struct {
    Mesh terrain, scenery, overlay, ocean_overlay, water, tunnels, tunnel_lights;
    Material land_material, water_material;
    Shader land_shader, water_shader;
    Texture2D coast, detail, surface_mask, forest_scan, rock_scan;
    RenderTexture2D shadow, reflection;
    Shader shadow_shader;
    Material shadow_material;
    Matrix light_vp, reflection_vp;
    Camera3D reflected_camera;
    float reflected_reveal;
    int reflection_valid;
    int land_view, water_view, render_pass, light_matrix, reflection_matrix;
    int land_reveal, water_time, land_wind, water_wake_pose, water_wake_motion, cut_eye, cut_target, cut_mode, tunnel_view;
    int ao_enabled, ao_location, detail_enabled, detail_location;
    int built;
} AwScene;

#include "shaders.h"

static float aw_y(float height) {
    return 1.8f+(height-1.0f)*3.0f;
}

static float aw_ground_y(const AwMap *m,int node,float fx,float fz){
    int c=aw_node_cell(m,node);float q=node>=AW_SPAN_START?m->spans[node-AW_SPAN_START].q:node>=AW_CELLS?m->cave[node-AW_CELLS].q:aw_surface_q(m,node,fx,fz);
    if(m->cave_bin_count[c]||m->trail_bin_count[c]||m->bridge_bins[c])q=aw_support_q(m,c%64+fx,c/64+fz,q);
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

#include "art.h"
#include "props.h"

static const Color aw_palette[AW_TILES]={
    {96,121,72,255},{64,87,45,255},{130,117,95,255},{182,167,131,255},
    {122,124,116,255},{214,222,222,255},{129,167,179,255},{99,91,74,255},
    {82,119,115,255},{32,68,82,255},{103,107,102,255}
};
/* Temperate art direction is independent of other biome palettes. */
static Color aw_material_color(const AwMap*m,int material){
    if(m->options.biome==AW_TEMPERATE){
        switch(material){
            case AW_GRASS:return (Color){106,139,79,255};
            case AW_FOREST:return (Color){71,106,59,255};
            case AW_DIRT:return (Color){145,125,96,255};
            case AW_MUD:return (Color){111,98,75,255};
        }
    }
    return aw_palette[material];
}
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
        Color color=aw_material_color(m,mat==AW_DEEP?AW_SAND:mat);r+=color.r;g+=color.g;b+=color.b;n++;
    }
    return (Color){r/n,g/n,b/n,255};
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
        Color color=aw_tile_color(colors,fx,fz);Vector3 normal=aw_surface_normal(m,x,z);
        int deck=m->bridge_bins[cz*64+cx]&&q>aw_height_q(m,x,z)+.02f;
        if(deck||((m->cave_bin_count[cz*64+cx]||m->trail_bin_count[cz*64+cx])&&q<aw_height_q(m,x,z)-0.08f)){
            const float e=0.02f;
            float nx=aw_density(m,x-e,q,z)-aw_density(m,x+e,q,z);
            float ny=aw_density(m,x,q-e,z)-aw_density(m,x,q+e,z);
            float nz=aw_density(m,x,q,z-e)-aw_density(m,x,q,z+e);
            normal=Vector3Normalize((Vector3){nx/AW_UNIT,ny/0.75f,nz/AW_UNIT});
            color=deck?(Color){105,116,116,255}:(Color){112,119,105,255};
        }
        int n=r->b->count++;r->b->positions[n]=(Vector3){x*AW_UNIT,aw_y(q/4),z*AW_UNIT};r->b->normals[n]=normal;r->b->colors[n]=color;r->b->uv[n]=(Vector2){r->rank,deck?5:10};
    }
    /* Keep actual excavated triangles for inspection. No proxy boxes or second
     * mesher: the isolated shell uses the same vertices as the world surface.
     * Include entrance floors where the terrain and passage floor coincide. */
    float x=(a.x+b.x+c.x)/3,z=(a.z+b.z+c.z)/3,q=(a.q+b.q+c.q)/3;
    int cell=aw_clamp((int)z,0,63)*64+aw_clamp((int)x,0,63),excavated=0,cut_rock=0;
    if(m->cave_bin_count[cell]||m->trail_bin_count[cell]){
        for(int k=0;k<3;k++)cut_rock|=p[k].q<aw_height_q(m,p[k].x,p[k].z)-0.08f;
        excavated=cut_rock;
        excavated|=aw_cave_field(m,x,q+0.1f,z)<0;
        excavated|=aw_mountain_field(m,x,q+0.1f,z)<0;
    }
    if(excavated){
        AwBuilder*t=r->tunnels;aw_reserve(t,3);
        for(int k=r->b->count-3;k<r->b->count;k++){
            /* A path on unchanged ground keeps its grass/soil material. Only
             * newly exposed rock gets cavity detail, including in isolation. */
            if(cut_rock)r->b->uv[k].y=12;
            int n=t->count++;t->positions[n]=r->b->positions[k];t->normals[n]=r->b->normals[k];
            t->colors[n]=r->b->colors[k];t->uv[n]=r->b->uv[k];
        }
    }
}
static void aw_destroy_scene(AwScene*s){
    if(!s->built)return;
    if(s->tunnels.vertexCount)UnloadMesh(s->tunnels);
    if(s->tunnel_lights.vertexCount)UnloadMesh(s->tunnel_lights);
    UnloadMesh(s->terrain);UnloadMesh(s->scenery);UnloadMesh(s->overlay);UnloadMesh(s->ocean_overlay);UnloadMesh(s->water);
    MemFree(s->land_material.maps);MemFree(s->water_material.maps);MemFree(s->shadow_material.maps);
    UnloadRenderTexture(s->shadow);if(s->reflection.id)UnloadRenderTexture(s->reflection);UnloadShader(s->shadow_shader);
    UnloadShader(s->land_shader);UnloadShader(s->water_shader);UnloadTexture(s->coast);UnloadTexture(s->detail);UnloadTexture(s->surface_mask);UnloadTexture(s->forest_scan);UnloadTexture(s->rock_scan);
    memset(s,0,sizeof(*s));
}
typedef struct {int first,last;Vector3 base;} AwTreeBake;
static void aw_bake_vertex(AwOcclusion*a,AwBuilder*b,int i){
    /* Kind 2 owns real transparency/emission. Opaque alpha stores AO. */
    if(b->uv[i].y>1.5f&&b->uv[i].y<2.5f)return;
    Vector3 p=b->positions[i],n=b->normals[i];
    b->colors[i].a=(unsigned char)lroundf(255*aw_ao_sample(a,p.x,p.y,p.z,n.x,n.y,n.z));
}
static void aw_bake_vertices(AwOcclusion*a,AwBuilder*b){
    for(int i=0;i<b->count;i++)aw_bake_vertex(a,b,i);
}
/* Tree foliage has many tiny triangles but low-frequency ambient lighting.
 * Interpolate six directional probes at root/mid/crown height instead of
 * repeating a horizon integral on every leaf corner. Contact remains local. */
static void aw_bake_tree(AwOcclusion*a,AwBuilder*b,AwTreeBake tree){
    Vector3 base=tree.base;float top=base.y;
    for(int i=tree.first;i<tree.last;i++)top=fmaxf(top,b->positions[i].y);
    float height=fmaxf(.1f,top-base.y),probes[3][6];
    for(int h=0;h<3;h++)for(int d=0;d<6;d++){
        float n[3]={0};n[d/2]=d%2?-1:1;
        probes[h][d]=aw_ao_horizon(a,base.x,base.y+.1f+height*h*.5f,base.z,n[0],n[1],n[2]);
    }
    for(int i=tree.first;i<tree.last;i++){
        Vector3 p=b->positions[i],n=b->normals[i];
        float t=Clamp((p.y-base.y)/height*2,0,1.9999f);int h=(int)t;t-=h;
        float axis[3]={n.x,n.y,n.z},occlusion=0,weight=0;
        for(int d=0;d<3;d++){
            int side=d*2+(axis[d]<0);float w=fabsf(axis[d]);
            occlusion+=w*aw_lerp(probes[h][side],probes[h+1][side],t);weight+=w;
        }
        occlusion/=fmaxf(.001f,weight);
        float contact=p.y<base.y+1.5f?aw_ao_contact(a,p.x,p.y,p.z):0;
        b->colors[i].a=(unsigned char)lroundf(255*(1-occlusion)*(1-contact));
    }
}
static void aw_set_occlusion(AwScene*s,int enabled){
    if(!s->built||s->ao_enabled==enabled)return;
    s->ao_enabled=enabled;s->reflection_valid=0;
    SetShaderValue(s->land_shader,s->ao_location,&enabled,SHADER_UNIFORM_INT);
}
static void aw_set_detail(AwScene*s,int enabled){
    if(!s->built||s->detail_enabled==enabled)return;
    s->detail_enabled=enabled;s->reflection_valid=0;
    SetShaderValue(s->land_shader,s->detail_location,&enabled,SHADER_UNIFORM_INT);
}
static void aw_build_detail(AwScene*s,const AwMap*m){
    enum {RES=512};Image detail=GenImageColor(RES,RES,WHITE);Color*pixels=detail.data;
    for(int y=0;y<RES;y++)for(int x=0;x<RES;x++){
        float h[4];aw_detail_sample((x+.5f)/RES,(y+.5f)/RES,h);
        pixels[y*RES+x]=(Color){(uint8_t)lroundf(h[0]*255),(uint8_t)lroundf(h[1]*255),(uint8_t)lroundf(h[2]*255),(uint8_t)lroundf(h[3]*255)};
    }
    s->detail=LoadTextureFromImage(detail);UnloadImage(detail);GenTextureMipmaps(&s->detail);
    SetTextureFilter(s->detail,TEXTURE_FILTER_TRILINEAR);SetTextureWrap(s->detail,TEXTURE_WRAP_REPEAT);
    Image mask=GenImageColor(AW_VERT,AW_VERT,BLANK);pixels=mask.data;
    for(int z=0;z<AW_VERT;z++)for(int x=0;x<AW_VERT;x++){
        float w[4];aw_detail_weights(m,x,z,w);
        pixels[z*AW_VERT+x]=(Color){(uint8_t)lroundf(w[0]*255),(uint8_t)lroundf(w[1]*255),(uint8_t)lroundf(w[2]*255),(uint8_t)lroundf(w[3]*255)};
    }
    s->surface_mask=LoadTextureFromImage(mask);UnloadImage(mask);
    SetTextureFilter(s->surface_mask,TEXTURE_FILTER_BILINEAR);SetTextureWrap(s->surface_mask,TEXTURE_WRAP_CLAMP);
    s->land_material.maps[MATERIAL_MAP_ALBEDO].texture=s->surface_mask;
    s->land_material.maps[MATERIAL_MAP_ROUGHNESS].texture=s->detail;
    s->forest_scan=LoadTexture("resources/alienwars/art/forest.png");s->rock_scan=LoadTexture("resources/alienwars/art/rock.png");
    if(!IsTextureValid(s->forest_scan)||!IsTextureValid(s->rock_scan)){fprintf(stderr,"Terrain materials missing\n");exit(2);}
    Texture2D scans[2]={s->forest_scan,s->rock_scan};
    for(int i=0;i<2;i++){GenTextureMipmaps(&scans[i]);SetTextureFilter(scans[i],TEXTURE_FILTER_TRILINEAR);SetTextureWrap(scans[i],TEXTURE_WRAP_REPEAT);}
    s->forest_scan=scans[0];s->rock_scan=scans[1];
    s->land_material.maps[MATERIAL_MAP_NORMAL].texture=s->forest_scan;
    s->land_material.maps[MATERIAL_MAP_OCCLUSION].texture=s->rock_scan;
}
/* Bridge seams are visual detail outside the validated walking strip.
 * The deck itself is authoritative volume geometry, not this decorative mesh. */
static Vector3 aw_bridge_point(const AwBridge*b,float u,float side,float rise){
    return (Vector3){(b->x+.5f+b->dx*u+b->dz*side)*AW_UNIT,
        aw_y(aw_bridge_q(b,u)/4)+rise,(b->z+.5f+b->dz*u-b->dx*side)*AW_UNIT};
}
static void aw_bridge_details(AwBuilder*mesh,const AwBridge*b){
    /* Transverse expansion seams sit on the actual deck grade. */
    for(int u=0;u<=b->length;u+=2){
        Vector3 left=aw_bridge_point(b,u,-.98f,.018f),right=aw_bridge_point(b,u,.98f,.018f);
        aw_branch(mesh,left,right,.018f,.018f,(Color){ 70, 70, 70,255},AW_CELLS-1,5,4);
    }
}
static void aw_build_scene(AwScene*s,const AwMap*m){
    aw_destroy_scene(s);AwBuilder terrain={0},scenery={0},overlay={0},ocean_overlay={0},water={0},tunnels={0},tunnel_lights={0};
    AwOcclusion*ao=aw_ao_alloc(1,sizeof(*ao));aw_ao_init(ao,m);
    AwTreeBake*trees=aw_ao_alloc(AW_CELLS,sizeof(*trees));int tree_count=0;
    for(int index=0;index<AW_CELLS;index++){
        int c=m->order[index],x=c%AW_SIZE,z=c/AW_SIZE;float rank=index;
        const AwCell*t=&m->cells[c];int mat=t->material;Color ground=aw_material_color(m,mat);
        Vector3 v[4]={{x*AW_UNIT,0,z*AW_UNIT},{(x+1)*AW_UNIT,0,z*AW_UNIT},{(x+1)*AW_UNIT,0,(z+1)*AW_UNIT},{x*AW_UNIT,0,(z+1)*AW_UNIT}};
        for(int k=0;k<4;k++)v[k].y=aw_y(t->q[k]/4.0f);
        AwVolumeRender render={&terrain,&tunnels,m,rank};aw_volume_cell(m,c,aw_render_volume_triangle,&render);

        int canonical=m->options.symmetry&&c>=AW_CELLS/2?AW_CELLS-1-c:c;
        uint32_t h=aw_hash(m->seed^(uint32_t)canonical*8191u);Vector3 p=aw_center(m,c);
        int trail_surface=m->trail_bin_count[c]&&aw_mountain_field(m,x+.5f,aw_height_q(m,x+.5f,z+.5f)+1,z+.5f)<1;
        if(!t->road&&!m->cave_access[c]&&!t->tunnel&&!trail_surface&&!m->bridge_bins[c]&&m->walkable[c]){
            float jitter=(aw_prop_random(h)-.5f)*.65f;if(m->options.symmetry&&c>=AW_CELLS/2)jitter=-jitter;
            p.x+=jitter;p.z-=jitter;
            p.y=aw_ground_y(m,c,.5f+jitter/AW_UNIT,.5f-jitter/AW_UNIT);
            int first=scenery.count;
            int tree=mat==AW_FOREST||(mat==AW_SNOW&&h%19==0)||(mat==AW_GRASS&&h%31==0);
            if(tree)aw_ao_contact_add(ao,p.x,p.y,p.z,1.15f,1.15f,1.1f,.65f);
            if(mat==AW_FOREST)aw_tree(&scenery,p,h,rank,h%4==0,0);
            else if(mat==AW_SNOW&&h%19==0)aw_tree(&scenery,p,h,rank,1,1);
            else if(mat==AW_GRASS&&h%31==0)aw_tree(&scenery,p,h,rank,0,0);
            else if((mat==AW_ROCK||mat==AW_SNOW||mat==AW_DIRT||mat==AW_SAND)&&h%5==0){
                float radius=.3f+aw_prop_random(h+1)*.38f,height=.35f+aw_prop_random(h+2)*.45f;
                aw_boulder(&scenery,p,radius,height,h,ground,rank);
                aw_ao_contact_add(ao,p.x,p.y,p.z,radius+.4f,radius+.4f,height+.3f,.78f);
                for(int i=0;i<3;i++){
                    Vector3 q=p;q.x+=cosf(i*2.1f)*.42f;q.z+=sinf(i*2.1f)*.42f;
                    q.y=aw_ground_y(m,c,(q.x/AW_UNIT-c%64),(q.z/AW_UNIT-c/64));
                    aw_boulder(&scenery,q,.09f,.1f,h+i,ground,rank);
                }
            }else if((mat==AW_GRASS||mat==AW_DIRT||mat==AW_SAND)&&h%4==0)aw_grass(&scenery,p,h,rank,mat!=AW_GRASS);
            if(m->options.biome==AW_TEMPERATE&&(mat==AW_FOREST||mat==AW_GRASS)&&h%3==0){
                for(int j=0;j<3;j++){Vector3 q=p;q.x+=cosf(h+j*2.4f)*.43f;q.z+=sinf(h+j*2.4f)*.43f;
                    q.y=aw_ground_y(m,c,q.x/AW_UNIT-x,q.z/AW_UNIT-z);aw_grass(&scenery,q,h+j*733u,rank,0);}
            }
            if(tree)trees[tree_count++]=(AwTreeBake){first,scenery.count,p};
        }
        if(m->walkable[c]){
            for(int k=0;k<4;k++){v[k].y=aw_y(t->q[k]/4.0f)+0.04f;}
            Color color=m->reachable[c]?(Color){66,236,178,105}:(Color){246,132,82,105};aw_top(&overlay,v,4,color,rank,2);
        }
    }
    for(int i=0;i<m->bridge_count;i++)aw_bridge_details(&scenery,&m->bridges[i]);
    for(int i=0;i<m->span_count;i++){
        const AwSpan*s=&m->spans[i];int c=s->cell;
        if(m->surface_span[c]==i&&m->walkable[c])continue;
        float x=c%64,z=c/64;Vector3 v[4];
        const float dx[4]={.12f,.88f,.88f,.12f},dz[4]={.12f,.12f,.88f,.88f};
        for(int k=0;k<4;k++){float q=aw_support_q(m,x+dx[k],z+dz[k],s->q);v[k]=(Vector3){(x+dx[k])*AW_UNIT,aw_y(q/4)+.06f,(z+dz[k])*AW_UNIT};}
        Color color=m->reachable[AW_SPAN_START+i]?(Color){66,236,178,105}:(Color){246,132,82,105};aw_top(&overlay,v,4,color,0,2);
    }
    for(int i=0;i<m->cave_count;i++)if(i%3==0){
        Vector3 p=aw_center(m,AW_CELLS+i);p.y+=0.06f;
        aw_branch(&scenery,p,Vector3Add(p,(Vector3){0,.11f,0}),.07f,.05f,(Color){108,177,151,255},0,2,8);
        aw_branch(&tunnel_lights,p,Vector3Add(p,(Vector3){0,.11f,0}),.07f,.05f,(Color){108,177,151,255},0,2,8);
    }
    for(int r=0;r<4;r++){
        Vector3 p=aw_center(m,m->resources[r]);
        aw_ao_contact_add(ao,p.x,p.y,p.z,1.15f,1.15f,1,.78f);
        aw_boulder(&scenery,p,.72f,.62f,m->seed+r,(Color){95,109,107,255},0);
        for(int i=0;i<7;i++){
            Vector3 q=p;q.x+=cosf(i*2.399f)*.58f;q.z+=sinf(i*2.399f)*.58f;
            aw_rock(&scenery,q,.12f,.32f+(i%3)*.12f,i,(Color){83,143,155,255},0,6);
        }
    }
    for(int side=0;side<2;side++){
        Vector3 p=aw_center(m,m->spawns[side]);aw_art_nursery(&scenery,p,side);
        aw_ao_contact_add(ao,p.x,p.y,p.z,2.7f,2.5f,1.5f,.9f);
    }
    /* Model the playable ocean shelf; its inner edge is exactly the land
     * mesher's submerged border. The water sheet continues beyond play bounds. */
    for(int c=0;c<AW_OCEAN_CELLS;c++){
        int x=c%AW_OCEAN_SIZE-AW_OCEAN_BELT,z=c/AW_OCEAN_SIZE-AW_OCEAN_BELT;
        Vector3 v[4]={{x*AW_UNIT,0,z*AW_UNIT},{(x+1)*AW_UNIT,0,z*AW_UNIT},{(x+1)*AW_UNIT,0,(z+1)*AW_UNIT},{x*AW_UNIT,0,(z+1)*AW_UNIT}};
        if(x<0||z<0||x>=AW_SIZE||z>=AW_SIZE){
            for(int k=0;k<4;k++)v[k].y=aw_y(aw_ocean_bed_q(m,v[k].x/AW_UNIT,v[k].z/AW_UNIT)/4);
            aw_top(&terrain,v,4,aw_palette[AW_SAND],0,0);
        }
        if(m->ocean_connected[c]){
            for(int k=0;k<4;k++)v[k].y=-.035f;
            aw_top(&ocean_overlay,v,4,(Color){74,167,232,115},0,2);
        }
    }
    Vector3 sea[4]={{-96,-0.12f,-96},{224,-0.12f,-96},{224,-0.12f,224},{-96,-0.12f,224}};
    aw_top(&water,sea,4,WHITE,0,0);
    double bake_start=GetTime();
    aw_bake_vertices(ao,&terrain);
    unsigned terrain_samples=ao->samples_baked;
    for(int i=0,tree=0;i<scenery.count;){
        if(tree<tree_count&&i==trees[tree].first){aw_bake_tree(ao,&scenery,trees[tree]);i=trees[tree++].last;}
        else aw_bake_vertex(ao,&scenery,i++);
    }
    aw_bake_vertices(ao,&tunnels);free(trees);
    printf("AO_BAKE ms=%.0f samples=%u cave_samples=%u contacts=%d terrain_samples=%u tree_probes=%d\n",(GetTime()-bake_start)*1000,ao->samples_baked,ao->cave_samples,ao->contact_count,terrain_samples,tree_count*18);
    aw_ao_free(ao);free(ao);
    if(tunnels.count)s->tunnels=aw_upload(&tunnels);
    if(tunnel_lights.count)s->tunnel_lights=aw_upload(&tunnel_lights);
    s->terrain=aw_upload(&terrain);s->scenery=aw_upload(&scenery);s->overlay=aw_upload(&overlay);s->ocean_overlay=aw_upload(&ocean_overlay);s->water=aw_upload(&water);
    s->land_shader=LoadShaderFromMemory(aw_vertex_shader,aw_land_fragment);
    s->water_shader=LoadShaderFromMemory(aw_vertex_shader,aw_water_fragment);
    float temperate=m->options.biome==AW_TEMPERATE?1.0f:0.0f;
    SetShaderValue(s->land_shader,GetShaderLocation(s->land_shader,"temperate"),&temperate,SHADER_UNIFORM_FLOAT);
    SetShaderValue(s->water_shader,GetShaderLocation(s->water_shader,"temperate"),&temperate,SHADER_UNIFORM_FLOAT);
    s->detail_location=GetShaderLocation(s->land_shader,"detailEnabled");s->detail_enabled=1;
    SetShaderValue(s->land_shader,s->detail_location,&s->detail_enabled,SHADER_UNIFORM_INT);
    s->ao_location=GetShaderLocation(s->land_shader,"aoEnabled");s->ao_enabled=1;
    SetShaderValue(s->land_shader,s->ao_location,&s->ao_enabled,SHADER_UNIFORM_INT);
    s->land_reveal=GetShaderLocation(s->land_shader,"reveal");s->water_time=GetShaderLocation(s->water_shader,"time");
    s->land_wind=GetShaderLocation(s->land_shader,"windTime");
    s->water_wake_pose=GetShaderLocation(s->water_shader,"wakePose");s->water_wake_motion=GetShaderLocation(s->water_shader,"wakeMotion");
    s->cut_eye=GetShaderLocation(s->land_shader,"cutEye");s->cut_target=GetShaderLocation(s->land_shader,"cutTarget");s->cut_mode=GetShaderLocation(s->land_shader,"cutMode");
    s->tunnel_view=GetShaderLocation(s->land_shader,"tunnelView");
    s->land_view=GetShaderLocation(s->land_shader,"viewDirection");s->water_view=GetShaderLocation(s->water_shader,"viewDirection");
    s->render_pass=GetShaderLocation(s->land_shader,"renderPass");s->light_matrix=GetShaderLocation(s->land_shader,"lightVP");
    s->reflection_matrix=GetShaderLocation(s->water_shader,"reflectionVP");
    s->land_shader.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(s->land_shader,"texture0");
    s->land_shader.locs[SHADER_LOC_MAP_ROUGHNESS]=GetShaderLocation(s->land_shader,"texture2");
    s->land_shader.locs[SHADER_LOC_MAP_METALNESS]=GetShaderLocation(s->land_shader,"texture1");
    s->land_shader.locs[SHADER_LOC_MAP_NORMAL]=GetShaderLocation(s->land_shader,"texture3");
    s->land_shader.locs[SHADER_LOC_MAP_OCCLUSION]=GetShaderLocation(s->land_shader,"texture4");
    s->water_shader.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(s->water_shader,"texture0");
    s->water_shader.locs[SHADER_LOC_MAP_METALNESS]=GetShaderLocation(s->water_shader,"texture1");
    s->shadow_shader=LoadShaderFromMemory(aw_vertex_shader,aw_shadow_fragment);
    if(s->detail_location<0||s->land_shader.locs[SHADER_LOC_MAP_ROUGHNESS]<0||s->ao_location<0||s->land_reveal<0||s->water_time<0||s->cut_mode<0||s->tunnel_view<0||s->light_matrix<0||s->reflection_matrix<0||s->land_view<0||s->water_view<0||s->render_pass<0||s->water_shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]<0||s->shadow_shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]<0||!IsShaderValid(s->shadow_shader)){fprintf(stderr,"Map Lab shader compilation failed\n");exit(2);}
    s->land_material=LoadMaterialDefault();s->land_material.shader=s->land_shader;
    s->water_material=LoadMaterialDefault();s->water_material.shader=s->water_shader;
    s->shadow_material=LoadMaterialDefault();s->shadow_material.shader=s->shadow_shader;
    /* Shoreline distance and water mask. AO is baked per opaque vertex. */
    enum {RES=384};float *height=malloc(RES*RES*sizeof(float)),*distance=malloc(RES*RES*sizeof(float));
    if(!height||!distance){fprintf(stderr,"Shore map allocation failed\n");exit(2);}
    for(int z=0;z<RES;z++)for(int x=0;x<RES;x++){
        int i=z*RES+x;height[i]=aw_ocean_bed_q(m,(x+.5f)*AW_OCEAN_SIZE/RES-AW_OCEAN_BELT,(z+.5f)*AW_OCEAN_SIZE/RES-AW_OCEAN_BELT);
        distance[i]=height[i]>1.44f?0:1000;
    }
    for(int pass=0;pass<2;pass++)for(int j=0;j<RES*RES;j++){
        int i=pass?RES*RES-1-j:j,x=i%RES,z=i/RES,step=pass?1:-1;
        for(int k=-1;k<=1;k++){
            int nx=x+k,nz=z+step;if(nx>=0&&nx<RES&&nz>=0&&nz<RES)distance[i]=fminf(distance[i],distance[nz*RES+nx]+(k?1.414214f:1));
        }
        if(x+step>=0&&x+step<RES)distance[i]=fminf(distance[i],distance[i+step]+1);
    }
    Image coast=GenImageColor(RES,RES,WHITE);Color*pixels=coast.data;
    for(int z=0;z<RES;z++)for(int x=0;x<RES;x++){
        int i=z*RES+x;
        pixels[i]=(Color){(unsigned char)Clamp(distance[i]*.5f/12*255,0,255),(unsigned char)Clamp((1.08f-height[i]*.75f)/12*255,0,255),255,height[i]>1.44f?0:255};
    }
    free(height);free(distance);
    s->coast=LoadTextureFromImage(coast);UnloadImage(coast);SetTextureFilter(s->coast,TEXTURE_FILTER_BILINEAR);SetTextureWrap(s->coast,TEXTURE_WRAP_CLAMP);
    aw_build_detail(s,m);
    s->water_material.maps[MATERIAL_MAP_ALBEDO].texture=s->coast;
    s->shadow=LoadRenderTexture(2048,2048);
    if(!IsRenderTextureValid(s->shadow)){fprintf(stderr,"Shadow framebuffer unavailable\n");exit(2);}
    SetTextureFilter(s->shadow.texture,TEXTURE_FILTER_POINT);SetTextureWrap(s->shadow.texture,TEXTURE_WRAP_CLAMP);
    s->land_material.maps[MATERIAL_MAP_METALNESS].texture=s->shadow.texture;
    Vector3 sun=Vector3Normalize((Vector3){-.55f,.85f,-.40f});
    Camera3D light={.position=Vector3Add((Vector3){64,7,64},Vector3Scale(sun,170)),.target={64,7,64},.up={0,1,0},.fovy=210,.projection=CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(s->shadow);ClearBackground(WHITE);BeginMode3D(light);
    s->light_vp=MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection());
    DrawMesh(s->terrain,s->shadow_material,MatrixIdentity());DrawMesh(s->scenery,s->shadow_material,MatrixIdentity());
    EndMode3D();EndTextureMode();
    SetShaderValueMatrix(s->land_shader,s->light_matrix,s->light_vp);
    s->built=1;
}
/* Planar reflections are cached until the orthographic camera or assembly
 * changes. Water normals animate independently; a still view costs one pass. */
static void aw_prepare_reflection(AwScene*s,Camera3D camera,float reveal){
    int width=1024,height=(int)(1024.0f*GetScreenHeight()/GetScreenWidth());height=aw_clamp(height,256,1536);
    if(s->reflection.texture.width!=width||s->reflection.texture.height!=height){
        if(s->reflection.id)UnloadRenderTexture(s->reflection);
        s->reflection=LoadRenderTexture(width,height);s->reflection_valid=0;
        if(!IsRenderTextureValid(s->reflection)){fprintf(stderr,"Reflection framebuffer unavailable\n");exit(2);}
        SetTextureFilter(s->reflection.texture,TEXTURE_FILTER_BILINEAR);SetTextureWrap(s->reflection.texture,TEXTURE_WRAP_CLAMP);
        s->water_material.maps[MATERIAL_MAP_METALNESS].texture=s->reflection.texture;
    }
    if(s->reflection_valid&&Vector3Distance(camera.position,s->reflected_camera.position)<.0001f&&Vector3Distance(camera.target,s->reflected_camera.target)<.0001f&&fabsf(camera.fovy-s->reflected_camera.fovy)<.0001f&&fabsf(reveal-s->reflected_reveal)<.1f)return;
    s->reflected_camera=camera;s->reflected_reveal=reveal;s->reflection_valid=1;
    camera.position.y=-.24f-camera.position.y;camera.target.y=-.24f-camera.target.y;
    Vector3 view=Vector3Normalize(Vector3Subtract(camera.position,camera.target));
    int pass=1,off=0;
    SetShaderValue(s->land_shader,s->render_pass,&pass,SHADER_UNIFORM_INT);
    SetShaderValue(s->land_shader,s->cut_mode,&off,SHADER_UNIFORM_INT);SetShaderValue(s->land_shader,s->tunnel_view,&off,SHADER_UNIFORM_INT);
    SetShaderValue(s->land_shader,s->land_view,&view,SHADER_UNIFORM_VEC3);SetShaderValue(s->land_shader,s->land_reveal,&reveal,SHADER_UNIFORM_FLOAT);
    BeginTextureMode(s->reflection);ClearBackground((Color){99,122,136,255});BeginMode3D(camera);
    s->reflection_vp=MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection());
    DrawMesh(s->terrain,s->land_material,MatrixIdentity());DrawMesh(s->scenery,s->land_material,MatrixIdentity());
    EndMode3D();EndTextureMode();
    SetShaderValue(s->land_shader,s->render_pass,&off,SHADER_UNIFORM_INT);
    SetShaderValueMatrix(s->water_shader,s->reflection_matrix,s->reflection_vp);
}

static void aw_draw_scene(AwScene*s,float reveal,float time,int overlay,int ocean_overlay,Vector3 eye,Vector3 target,int cut,int tunnel_view){
    if(tunnel_view){reveal=AW_CELLS;cut=0;}
    Vector3 view=Vector3Normalize(Vector3Subtract(eye,target));
    SetShaderValue(s->land_shader,s->land_view,&view,SHADER_UNIFORM_VEC3);SetShaderValue(s->water_shader,s->water_view,&view,SHADER_UNIFORM_VEC3);
    SetShaderValue(s->land_shader,s->tunnel_view,&tunnel_view,SHADER_UNIFORM_INT);
    SetShaderValue(s->land_shader,s->land_reveal,&reveal,SHADER_UNIFORM_FLOAT);
    SetShaderValue(s->land_shader,s->cut_eye,&eye,SHADER_UNIFORM_VEC3);
    SetShaderValue(s->land_shader,s->cut_target,&target,SHADER_UNIFORM_VEC3);
    SetShaderValue(s->land_shader,s->cut_mode,&cut,SHADER_UNIFORM_INT);
    SetShaderValue(s->water_shader,s->water_time,&time,SHADER_UNIFORM_FLOAT);SetShaderValue(s->land_shader,s->land_wind,&time,SHADER_UNIFORM_FLOAT);
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
    if(ocean_overlay)DrawMesh(s->ocean_overlay,s->land_material,identity);
}
#endif
