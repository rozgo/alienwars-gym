#ifndef ALIENWARS_MAP_H
#define ALIENWARS_MAP_H
/* Deterministic terrain grammar, volumetric caves and navigation. No renderer dependencies. */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#define AW_SIZE 64
#define AW_VERT (AW_SIZE+1)
#define AW_CELLS (AW_SIZE*AW_SIZE)
#define AW_CAVE_NODES 512
#define AW_CAVE_EDGES 768
#define AW_BIN_SIZE 48
#define AW_LINKS 8
#define AW_SPANS 4096
#define AW_SPAN_START (AW_CELLS+AW_CAVE_NODES)
#define AW_NODES (AW_SPAN_START+AW_SPANS)
#ifndef AW_GENERATOR_VERSION
#define AW_GENERATOR_VERSION 12
#endif
#define AW_VERSION AW_GENERATOR_VERSION
#define AW_BRIDGES 4
#define AW_MOUNTAINS 2
#define AW_MOUNT_GRID 5
#define AW_MOUNT_CELLS 25
#define AW_TRAIL_NODES 640
#define AW_TRAIL_EDGES 640
#define AW_TRAIL_BIN 32
#define AW_LANDFORMS 24
#define AW_LAKES 6
#define AW_OCEAN_MARGIN 2
#define AW_OCEAN_BELT 16
#define AW_OCEAN_SIZE (AW_SIZE+2*AW_OCEAN_BELT)
#define AW_OCEAN_CELLS (AW_OCEAN_SIZE*AW_OCEAN_SIZE)
#define AW_SUBDIV 6
#define AW_SHAPES 16
#define AW_MAX_FLOOR 10
#define AW_TILES 11
#define AW_BIOMES 3
enum {AW_TEMPERATE=1,AW_DESERT,AW_FROZEN};
#define AW_ALL ((1u<<AW_TILES)-1)
enum {AW_GRASS,AW_FOREST,AW_DIRT,AW_SAND,AW_ROCK,AW_SNOW,AW_ICE,AW_MUD,AW_SHALLOW,AW_DEEP,AW_ROAD};
static const char *aw_terrain_names[AW_TILES]={"Grass","Forest","Soil","Sand","Rock","Snow","Ice","Mud","Shallow water","Deep water","Road"};
static const int aw_cost[AW_TILES]={12,19,13,18,16,20,23,28,34,0,10};
typedef struct {int symmetry,floors_a,floors_b,biome,tunnels;} AwOptions;
typedef struct {int x2,z2,rx,rz,height,rotation;} AwLandform;
typedef struct {int x,z,rx,rz;} AwLake;
/* Bank centers, cardinal bearing, deck crown and endpoint elevations, in q. */
typedef struct {int x,z,dx,dz,length,qa,qb,crown;} AwBridge;
typedef struct {uint8_t q[4],material,road,tunnel,portal;} AwCell;
typedef struct {int16_t x,z,q,portal; uint8_t profile,junction; int16_t links[6];} AwCaveNode;
typedef struct {int16_t a,b;} AwCaveEdge;
/* Regional WFC chooses a connected cycle of directional route sockets. The
 * two arcs between its approaches become an interior route and an open bypass. */
typedef struct {
    int x,z,step,rotation,a,b,core;
    uint16_t domains[AW_MOUNT_CELLS];
    uint8_t sockets[AW_MOUNT_CELLS];
    int branch[2][AW_MOUNT_CELLS],length[2],trail[2][160],trail_length[2];
    uint32_t salt;int decisions,backtracks;
} AwMountain;
enum {AW_TRAIL_COVERED,AW_TRAIL_GALLERY,AW_TRAIL_CUT};
typedef struct {int16_t x,z,q;uint8_t profile,mode,junction;} AwTrailNode;
typedef struct {int16_t a,b;uint8_t region,branch;} AwTrailEdge;
typedef struct {int16_t cell,next,links[4],cave;float q;uint8_t fits,edge_fits[4];} AwSpan;
typedef struct {int16_t x,q,z;uint16_t valid;float value;} AwDensitySample;
#define AW_DENSITY_CACHE (1<<17)
typedef struct {
    AwDensitySample*density_cache; /* Temporary, per-map validation cache. */
    AwSpan spans[AW_SPANS];int span_count,span_ready;
    AwBridge bridges[AW_BRIDGES];int bridge_count;
    uint8_t bridge_bins[AW_CELLS]; /* One-based; crossings never overlap. */
    int16_t span_first[AW_CELLS],surface_span[AW_CELLS],cave_span[AW_CAVE_NODES],trail_span[AW_TRAIL_NODES];
    AwMountain mountains[AW_MOUNTAINS];int mountain_count;
    AwTrailNode trail[AW_TRAIL_NODES];AwTrailEdge trail_edges[AW_TRAIL_EDGES];
    int trail_count,trail_edge_count;
    uint16_t trail_bins[AW_CELLS][AW_TRAIL_BIN];uint8_t trail_bin_count[AW_CELLS];
    uint16_t ocean_depth[AW_OCEAN_CELLS]; /* Hundredths of a quarter-floor. */
    uint8_t ocean_connected[AW_OCEAN_CELLS];int ocean_count;
    AwLandform landforms[AW_LANDFORMS];AwLake lakes[AW_LAKES];
    int landform_count,lake_count,road_ends[2],landmarks[2],layout_attempts;
    uint32_t layout_seed;
    uint8_t macro_q[AW_VERT*AW_VERT],lake_mask[AW_VERT*AW_VERT],rolling[AW_VERT*AW_VERT];
    AwCaveNode cave[AW_CAVE_NODES]; AwCaveEdge cave_edges[AW_CAVE_EDGES];
    uint16_t cave_bins[AW_CELLS][AW_BIN_SIZE]; uint8_t cave_bin_count[AW_CELLS];
    int cave_count,cave_edge_count,cave_decisions,cave_expanded,cave_backtracks;
    int cave_entrances[2],cave_hubs[2],cave_rooms[4],cave_room_count;
    uint8_t cave_access[AW_CELLS];
    uint32_t seed,rng,hash,wave[AW_CELLS],compatible[AW_TILES];
    uint16_t shape_wave[AW_CELLS],shape_compatible[AW_CELLS][4][AW_SHAPES];
    uint8_t shape_lo[AW_CELLS][4],shape_hi[AW_CELLS][4],shape_preferred[AW_CELLS][4];
    uint8_t blend[AW_VERT*AW_VERT];
    int shape_decisions;
    AwOptions options;
    AwCell cells[AW_CELLS];
    uint8_t walkable[AW_NODES],reachable[AW_NODES];
    int order[AW_CELLS],order_count,decisions,reductions,attempts,structure_decisions;
    int path[AW_NODES],path_length,path_cost,walk_count,reached_count;
    int resources[4],spawns[2],center,valid,tunnel_count,terrain_count[AW_TILES];
} AwMap;
static AwOptions aw_defaults(void){return (AwOptions){1,6,6,AW_TEMPERATE,1};}
static int aw_clamp(int n,int a,int b){return n<a?a:n>b?b:n;}
static int aw_abs(int n){return n<0?-n:n;}
static uint32_t aw_hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
static uint32_t aw_random(AwMap*m){m->rng+=0x9e3779b9u;return aw_hash(m->rng);}
static int aw_noise(uint32_t seed,int x,int z,int scale){
    int gx=x/scale,gz=z/scale,fx=x%scale,fz=z%scale;
    int a=aw_hash(seed^(uint32_t)gx*9337u^(uint32_t)gz*1237u)&255;
    int b=aw_hash(seed^(uint32_t)(gx+1)*9337u^(uint32_t)gz*1237u)&255;
    int c=aw_hash(seed^(uint32_t)gx*9337u^(uint32_t)(gz+1)*1237u)&255;
    int d=aw_hash(seed^(uint32_t)(gx+1)*9337u^(uint32_t)(gz+1)*1237u)&255;
    return ((a*(scale-fx)+b*fx)*(scale-fz)+(c*(scale-fx)+d*fx)*fz)/(scale*scale);
}
static int aw_neighbor(int c,int d){int x=c%AW_SIZE,z=c/AW_SIZE; if(d==0)return z?c-AW_SIZE:-1;if(d==1)return x<AW_SIZE-1?c+1:-1;if(d==2)return z<AW_SIZE-1?c+AW_SIZE:-1;return x?c-1:-1;}
static int aw_corner_q(const AwMap*m,int node,int k){return node>=AW_SPAN_START?(int)lroundf(m->spans[node-AW_SPAN_START].q):node>=AW_CELLS?m->cave[node-AW_CELLS].q:m->cells[node].q[k];}
static void aw_flat(AwCell*c,int q){for(int k=0;k<4;k++)c->q[k]=(uint8_t)q;}
static int aw_edge_matches(const AwMap*m,int a,int b,int d){
    static const int edge[4][2]={{0,1},{1,2},{3,2},{0,3}};
    int e=(d+2)%4;
    return aw_corner_q(m,a,edge[d][0])==aw_corner_q(m,b,edge[e][0])&&aw_corner_q(m,a,edge[d][1])==aw_corner_q(m,b,edge[e][1]);
}
/* WFC on the planned corridor: each module has entry/exit elevation sockets.
 * Domain bits are quarter-floor elevations. Flat bends and 1:4 ramps propagate
 * both ways from pinned base/portal sockets; MRV chooses the remaining modules. */
static int aw_road_propagate(uint64_t *wave,const int *ramp,int n,int *reductions){
    int changed=1;
    while(changed){changed=0;for(int i=1;i<n;i++){
        uint64_t right=wave[i-1] | (ramp[i]?wave[i-1]>>1:0);
        uint64_t left=wave[i] | (ramp[i]?wave[i]<<1:0);
        uint64_t a=wave[i-1]&left,b=wave[i]&right;
        if(!a||!b)return 0;
        if(a!=wave[i-1]||b!=wave[i]){changed=1;(*reductions)++;wave[i-1]=a;wave[i]=b;}
    }}return 1;
}
static int aw_road_wfc(AwMap*m,int *height,const int *ramp,int n,int floor){
    uint64_t wave[160],mask=((1ull<<(floor*4+1))-1)&~15ull;
    for(int i=0;i<n;i++)wave[i]=mask;
    wave[0]=1ull<<(floor*4);wave[n-1]=1ull<<4;
    if(!aw_road_propagate(wave,ramp,n,&m->reductions))return 0;
    for(;;){
        int best=-1,entropy=99;
        for(int i=0;i<n;i++){int count=__builtin_popcountll(wave[i]);if(count>1&&count<entropy){best=i;entropy=count;}}
        if(best<0)break;
        int pick=aw_random(m)%entropy;uint64_t bits=wave[best];while(pick--)bits&=bits-1;
        wave[best]=bits&(~bits+1);m->structure_decisions++;
        if(!aw_road_propagate(wave,ramp,n,&m->reductions))return 0;
    }
    for(int i=0;i<n;i++)height[i]=__builtin_ctzll(wave[i]);return 1;
}
#include "layout.h"
#include "mountain_layout.h"
/* Surface tiles carry four elevation sockets. The 16 corner patterns use a
 * local low/high elevation domain; propagation matches actual elevations, not
 * paint IDs. Shared edge samples are authoritative for render and visibility. */
static int aw_corner_vertex(int c,int k){
    static const int offset[4]={0,1,AW_VERT+1,AW_VERT};
    return (c/AW_SIZE)*AW_VERT+c%AW_SIZE+offset[k];
}
static int aw_shape_q(const AwMap*m,int c,int pattern,int corner){
    return (pattern&(1<<corner))?m->shape_hi[c][corner]:m->shape_lo[c][corner];
}
static int aw_isqrt(int n){int r=0;while((r+1)*(r+1)<=n)r++;return r;}
static int aw_shape_domains(AwMap*m){
    int pin[AW_VERT*AW_VERT],lo[AW_VERT*AW_VERT],hi[AW_VERT*AW_VERT],preferred[AW_VERT*AW_VERT];
    for(int v=0;v<AW_VERT*AW_VERT;v++)pin[v]=-1;
    for(int c=0;c<AW_CELLS;c++)if(m->cells[c].road)for(int k=0;k<4;k++){
        int v=aw_corner_vertex(c,k),q=m->cells[c].q[k];
        if(pin[v]>=0&&pin[v]!=q)return 0;
        pin[v]=q;m->blend[v]=255;
    }
    for(int v=0;v<AW_VERT*AW_VERT;v++){
        int cv=m->options.symmetry&&v>AW_VERT*AW_VERT/2?AW_VERT*AW_VERT-1-v:v;
        int x=cv%AW_VERT,z=cv/AW_VERT,target=m->macro_q[cv];
        /* Earth shoulders meet the road socket instead of extruding a road
         * column above an unrelated terrain cell. */
        int nearest=999,roadq=0;
        for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++){
            int nx=x+dx,nz=z+dz;if(nx<0||nz<0||nx>=AW_VERT||nz>=AW_VERT)continue;
            int nv=nz*AW_VERT+nx,dist=dx*dx+dz*dz;
            if(pin[nv]>=0&&dist<nearest){nearest=dist;roadq=pin[nv];}
        }
        if(nearest<=9){int d=aw_isqrt(nearest);target=(roadq*(4-d)+target*d)/4;}
        target=aw_clamp(target,0,40);
        lo[v]=target/4*4;hi[v]=lo[v]+(target%4?4:0);
        preferred[v]=target%4>=2?hi[v]:lo[v];
        if(m->rolling[cv]){lo[v]=hi[v]=preferred[v]=target;m->blend[v]=255;}
        if(m->lake_mask[cv])lo[v]=hi[v]=preferred[v]=target;
        if(pin[v]>=0)lo[v]=hi[v]=preferred[v]=pin[v];
        /* Hard ocean sockets surround the entire mesh. Apply after shoulder
         * blending so high roads cannot pull the domain boundary above water.
         * Inland tiles form the beach or cliff through the shared-edge mesher. */
        if(x<=AW_OCEAN_MARGIN||z<=AW_OCEAN_MARGIN||x>=AW_SIZE-AW_OCEAN_MARGIN||z>=AW_SIZE-AW_OCEAN_MARGIN){
            if(pin[v]>0)return 0;
            lo[v]=hi[v]=preferred[v]=0;
        }
    }
    for(int c=0;c<AW_CELLS;c++){
        uint16_t domain=0;
        for(int k=0;k<4;k++){
            int v=aw_corner_vertex(c,k);
            m->shape_lo[c][k]=lo[v];m->shape_hi[c][k]=hi[v];m->shape_preferred[c][k]=preferred[v];

        }
        for(int t=0;t<AW_SHAPES;t++){
            int unique=1;
            for(int k=0;k<4;k++)if((t&(1<<k))&&m->shape_lo[c][k]==m->shape_hi[c][k])unique=0;
            if(unique)domain|=1u<<t;
        }
        m->shape_wave[c]=domain;
    }
    return 1;
}
static void aw_shape_catalog(AwMap*m){
    static const int edges[4][2]={{0,1},{1,2},{3,2},{0,3}};
    for(int c=0;c<AW_CELLS;c++)for(int d=0;d<4;d++){
        int n=aw_neighbor(c,d),e=(d+2)%4;
        for(int a=0;a<AW_SHAPES;a++){
            uint16_t mask=0;
            for(int b=0;b<AW_SHAPES;b++){
                int match=n<0;
                if(!match)match=aw_shape_q(m,c,a,edges[d][0])==aw_shape_q(m,n,b,edges[e][0])&&aw_shape_q(m,c,a,edges[d][1])==aw_shape_q(m,n,b,edges[e][1]);
                if(match)mask|=1u<<b;
            }
            m->shape_compatible[c][d][a]=mask;
        }
    }
}
static int aw_shape_propagate(AwMap*m,int initial){
    int queue[AW_CELLS],head=0,tail=0,count=0;uint8_t queued[AW_CELLS]={0};
    queue[tail++]=initial;queued[initial]=1;count++;
    while(count){
        int c=queue[head];head=(head+1)%AW_CELLS;count--;queued[c]=0;
        for(int d=0;d<5;d++){
            int n=d==4?(m->options.symmetry?AW_CELLS-1-c:-1):aw_neighbor(c,d);if(n<0)continue;
            uint16_t allowed=0,bits=m->shape_wave[c];
            while(bits){int t=__builtin_ctz(bits);bits&=bits-1;allowed|=d==4?(1u<<(((t<<2)|(t>>2))&15)):m->shape_compatible[c][d][t];}
            uint16_t mask=m->shape_wave[n]&allowed;if(!mask)return 0;
            if(mask!=m->shape_wave[n]){m->shape_wave[n]=mask;m->reductions++;if(!queued[n]){queue[tail]=n;tail=(tail+1)%AW_CELLS;queued[n]=1;count++;}}
        }
    }
    return 1;
}
static int aw_shape_wfc(AwMap*m){
    if(!aw_shape_domains(m))return 0;aw_shape_catalog(m);
    for(int c=0;c<AW_CELLS;c++)if(!aw_shape_propagate(m,c))return 0;
    uint8_t recorded[AW_CELLS]={0};
    for(;;){
        int best=-1,entropy=99;uint32_t tie=UINT_MAX;
        for(int c=0;c<AW_CELLS;c++){
            int n=__builtin_popcount(m->shape_wave[c]);
            if(n==1&&!recorded[c]){recorded[c]=1;m->order[m->order_count++]=c;}
            uint32_t h=aw_hash(m->seed^(uint32_t)c*31u);
            if(n>1&&(n<entropy||(n==entropy&&h<tie))){best=c;entropy=n;tie=h;}
        }
        if(best<0)break;
        int weight[AW_SHAPES]={0},total=0;
        for(int t=0;t<AW_SHAPES;t++)if(m->shape_wave[best]&(1u<<t)){
            int matches=0;for(int k=0;k<4;k++)matches+=aw_shape_q(m,best,t,k)==m->shape_preferred[best][k];
            weight[t]=1<<matches;total+=weight[t];
        }
        int pick=aw_random(m)%total,t=0;while(pick>=weight[t])pick-=weight[t++];
        m->shape_wave[best]=1u<<t;m->shape_decisions++;if(!aw_shape_propagate(m,best))return 0;
    }
    for(int c=0;c<AW_CELLS;c++)for(int k=0;k<4;k++)m->cells[c].q[k]=aw_shape_q(m,c,__builtin_ctz(m->shape_wave[c]),k);
    return 1;
}
static int aw_material_ok(int a,int b){
    if((a==AW_SAND&&(b==AW_SNOW||b==AW_ICE))||(b==AW_SAND&&(a==AW_SNOW||a==AW_ICE)))return 0;
    return 1;
}
static uint32_t aw_domain(AwMap*m,int c){
    AwCell*t=&m->cells[c];if(t->road||m->cave_access[c])return 1u<<AW_ROAD;if(!(t->q[0]|t->q[1]|t->q[2]|t->q[3]))return 1u<<AW_DEEP;
    int cc=m->options.symmetry&&c>=AW_CELLS/2?AW_CELLS-1-c:c,x=cc%AW_SIZE,z=cc/AW_SIZE;
    int wet=aw_noise(m->seed^317u,x,z,7),biome=m->options.biome;
    uint32_t mask=1u<<AW_ROCK;
    if(biome==AW_TEMPERATE)mask|=(1u<<AW_GRASS)|(1u<<AW_DIRT)|(1u<<(wet>130?AW_FOREST:AW_MUD));
    if(biome==AW_DESERT)mask|=(1u<<AW_SAND)|(1u<<AW_DIRT);
    if(biome==AW_FROZEN)mask|=(1u<<AW_SNOW)|(1u<<AW_ICE);
    if(t->q[0]==4&&wet>165)mask|=1u<<AW_SHALLOW;
    return mask;
}
static int aw_preferred_material(const AwMap*m,int c){
    int cc=m->options.symmetry&&c>=AW_CELLS/2?AW_CELLS-1-c:c,x=cc%AW_SIZE,z=cc/AW_SIZE;
    int wet=aw_noise(m->seed^317u,x,z,7),detail=aw_noise(m->seed^715u,x,z,5),biome=m->options.biome;
    if(m->cells[c].q[0]==4&&wet>180)return AW_SHALLOW;
    if(detail>202)return AW_ROCK;
    if(biome==AW_TEMPERATE)return wet>155?AW_FOREST:wet<75?AW_MUD:detail>170?AW_DIRT:AW_GRASS;
    if(biome==AW_DESERT)return wet>85?AW_SAND:AW_DIRT;
    if(biome==AW_FROZEN)return wet>100?AW_SNOW:AW_ICE;
    return AW_ROCK;
}
static int aw_propagate(AwMap*m,int start){
    int queue[AW_CELLS],head=0,tail=0,count=0;uint8_t queued[AW_CELLS]={0};
    queue[tail++]=start;queued[start]=1;count++;
    while(count){
        int c=queue[head];head=(head+1)%AW_CELLS;count--;queued[c]=0;
        if(!m->wave[c])return 0;
        uint32_t allowed=0,bits=m->wave[c];
        while(bits){int t=__builtin_ctz(bits);bits&=bits-1;allowed|=m->compatible[t];}
        for(int d=0;d<5;d++){
            int next=d==4?(m->options.symmetry?AW_CELLS-1-c:-1):aw_neighbor(c,d);if(next<0)continue;
            uint32_t mask=m->wave[next]&(d==4?m->wave[c]:allowed);
            if(mask==m->wave[next])continue;
            m->wave[next]=mask;m->reductions++;if(!mask)return 0;
            if(!queued[next]){queue[tail]=next;tail=(tail+1)%AW_CELLS;count++;queued[next]=1;}
        }
    }
    return 1;
}
static int aw_wfc(AwMap*m){
    for(int a=0;a<AW_TILES;a++){m->compatible[a]=0;for(int b=0;b<AW_TILES;b++)if(aw_material_ok(a,b))m->compatible[a]|=1u<<b;}
    for(int c=0;c<AW_CELLS;c++)m->wave[c]=aw_domain(m,c);
    for(int c=0;c<AW_CELLS;c++)if(!aw_propagate(m,c))return 0;
    for(;;){
        int best=-1,entropy=99;uint32_t tie=UINT_MAX;
        for(int c=0;c<AW_CELLS;c++){
            int n=__builtin_popcount(m->wave[c]);
            uint32_t h=aw_hash(m->seed^(uint32_t)c*7919u);
            if(n>1&&(n<entropy||(n==entropy&&h<tie))){best=c;entropy=n;tie=h;}
        }
        if(best<0)break;
        int weights[AW_TILES],total=0,preferred=aw_preferred_material(m,best);
        for(int t=0;t<AW_TILES;t++){
            weights[t]=(m->wave[best]&(1u<<t))?(t==preferred?96:1):0;total+=weights[t];
        }
        int pick=aw_random(m)%total,t=0;while(pick>=weights[t])pick-=weights[t++];
        m->wave[best]=1u<<t;m->decisions++;if(!aw_propagate(m,best))return 0;
    }
    for(int c=0;c<AW_CELLS;c++){m->cells[c].material=__builtin_ctz(m->wave[c]);m->terrain_count[m->cells[c].material]++;}
    return 1;
}
/* Visibility uses the same triangular surface and tunnel void as navigation.
 * Coordinates here are grid units horizontally and quarter-floors vertically. */
static float aw_lerp(float a,float b,float t){return a+(b-a)*t;}
static float aw_bilinear(float a,float b,float c,float d,float x,float z){return aw_lerp(aw_lerp(a,b,x),aw_lerp(d,c,x),z);}
/* Beveled cliff cross-section: flat shelves with a shaped transition through
 * each height band. Road sockets retain their linear grade. */
static float aw_tile_sample_q(const AwMap*m,int c,float x,float z){
    const uint8_t*q=m->cells[c].q;
    float height=aw_bilinear(q[0],q[1],q[2],q[3],x,z);
    float support=aw_bilinear(m->blend[aw_corner_vertex(c,0)],m->blend[aw_corner_vertex(c,1)],m->blend[aw_corner_vertex(c,2)],m->blend[aw_corner_vertex(c,3)],x,z)/255.0f;
    if(m->cells[c].road)support=1;
    float band=floorf(height/4),t=height/4-band;
    t=fminf(1,fmaxf(0,(t-0.27f)/0.46f));t=t*t*(3-2*t);
    float shaped=(band+t)*4;
    return aw_lerp(shaped,height,support);
}
/* The same tessellated triangles are queried by collision, scout height and
 * camera occlusion. Shared boundaries have exactly the same sample profile. */
static float aw_surface_q(const AwMap*m,int c,float fx,float fz){
    float sx=fminf(1,fmaxf(0,fx))*AW_SUBDIV,sz=fminf(1,fmaxf(0,fz))*AW_SUBDIV;
    int ix=aw_clamp((int)sx,0,AW_SUBDIV-1),iz=aw_clamp((int)sz,0,AW_SUBDIV-1);
    float x=sx-ix,z=sz-iz,unit=1.0f/AW_SUBDIV;
    float a=aw_tile_sample_q(m,c,ix*unit,iz*unit),b=aw_tile_sample_q(m,c,(ix+1)*unit,iz*unit);
    float d=aw_tile_sample_q(m,c,ix*unit,(iz+1)*unit),e=aw_tile_sample_q(m,c,(ix+1)*unit,(iz+1)*unit);
    return x>=z?a+(b-a)*x+(e-b)*z:a+(e-d)*x+(d-a)*z;
}

#include "bridge_field.h"
#include "caves.h"
#include "ocean.h"
#include "traversal.h"
static int aw_open(const AwMap*m,int node,int d){
    if(node<0||node>=AW_NODES||!m->walkable[node]||d<0||d>=AW_LINKS)return -1;
    if(node>=AW_SPAN_START){
        const AwSpan*s=&m->spans[node-AW_SPAN_START];int next=-1;
        if(d<4&&s->links[d]>=0)next=AW_SPAN_START+s->links[d];
        if(d==4&&m->surface_span[s->cell]==node-AW_SPAN_START)next=s->cell;
        if(d==5&&s->cave>=0)next=AW_CELLS+s->cave;
        return next>=0&&m->walkable[next]?next:-1;
    }
    if(node>=AW_CELLS){
        const AwCaveNode*n=&m->cave[node-AW_CELLS];
        if(d==7){int s=m->span_ready?m->cave_span[node-AW_CELLS]:-1;return s>=0&&m->walkable[AW_SPAN_START+s]?AW_SPAN_START+s:-1;}
        int next=d==6?n->portal:(n->links[d]<0?-1:AW_CELLS+n->links[d]);
        return next>=0&&m->walkable[next]?next:-1;
    }
    if(d==4){for(int i=0;i<m->cave_count;i++)if(m->cave[i].portal==node)return AW_CELLS+i;return -1;}
    if(d==5){int s=m->span_ready?m->surface_span[node]:-1;return s>=0&&m->walkable[AW_SPAN_START+s]?AW_SPAN_START+s:-1;}
    if(d>3)return -1;
    int next=aw_neighbor(node,d);if(next<0)return -1;
    return m->walkable[next]&&aw_edge_matches(m,node,next,d)?next:-1;
}
static void aw_navigation(AwMap*m){
    m->walk_count=m->reached_count=m->tunnel_count=0;
    memset(m->reachable,0,sizeof(m->reachable));
    memset(m->walkable,0,sizeof(m->walkable));
    for(int c=0;c<AW_CELLS;c++){
        int lo=40,hi=0;for(int k=0;k<4;k++){int q=m->cells[c].q[k];if(q<lo)lo=q;if(q>hi)hi=q;}
        /* Quarter-floor 2 is the first dry shore above the 1.44 water datum. */
        m->walkable[c]=aw_cost[m->cells[c].material]>0&&lo>=(AW_VERSION>=12?2:4)&&hi-lo<=1;
        if(m->walkable[c]&&(m->cave_bin_count[c]||m->trail_bin_count[c]||m->bridge_bins[c])){
            float q=aw_surface_q(m,c,0.5f,0.5f);
            if(fabsf(aw_support_q(m,c%64+0.5f,c/64+0.5f,q)-q)>0.1f)m->walkable[c]=0;
            if(m->bridge_bins[c]&&!aw_body_fits(m,c%64+.5f,q,c/64+.5f,0))m->walkable[c]=0;
        }
        m->walk_count+=m->walkable[c];
    }
    for(int i=0;i<m->cave_count;i++){m->walkable[AW_CELLS+i]=1;m->walk_count++;}
    for(int i=0;i<m->span_count;i++){m->walkable[AW_SPAN_START+i]=1;m->walk_count++;}
    m->tunnel_count=m->cave_count;
    int queue[AW_NODES],head=0,tail=0,start=m->spawns[0];
    if(!m->walkable[start])return;
    queue[tail++]=start;m->reachable[start]=1;
    while(head<tail){int c=queue[head++];for(int d=0;d<AW_LINKS;d++){int n=aw_open(m,c,d);if(n>=0&&!m->reachable[n]){m->reachable[n]=1;queue[tail++]=n;}}}
    m->reached_count=tail;
}
static int aw_move_cost(const AwMap*m,int a,int b){
    int ca=a>=AW_CELLS?10:aw_cost[m->cells[a].material],cb=b>=AW_CELLS?10:aw_cost[m->cells[b].material];
    int qa=0,qb=0;for(int k=0;k<4;k++){qa+=aw_corner_q(m,a,k);qb+=aw_corner_q(m,b,k);}
    return (a<AW_CELLS&&b>=AW_CELLS&&b<AW_SPAN_START&&m->cave[b-AW_CELLS].portal==a)||(b<AW_CELLS&&a>=AW_CELLS&&a<AW_SPAN_START&&m->cave[a-AW_CELLS].portal==b)?1:(ca+cb)/2+aw_abs(qa-qb);
}
/* Indexed binary heap: deterministic Dijkstra with terrain and grade costs. */
static int aw_find_path(AwMap*m,int from,int to){
    m->path_length=0;m->path_cost=0;
    if(from<0||to<0||from>=AW_NODES||to>=AW_NODES||!m->walkable[from]||!m->walkable[to])return 0;
    int dist[AW_NODES],prev[AW_NODES],heap[AW_NODES],pos[AW_NODES],size=0;
    for(int i=0;i<AW_NODES;i++){dist[i]=INT_MAX;prev[i]=-1;pos[i]=-1;}dist[from]=0;heap[size++]=from;pos[from]=0;
    while(size){
        int c=heap[0];pos[c]=-2;size--;
        if(size){heap[0]=heap[size];pos[heap[0]]=0;int p=0;for(;;){int a=p*2+1;if(a>=size)break;if(a+1<size&&dist[heap[a+1]]<dist[heap[a]])a++;if(dist[heap[p]]<=dist[heap[a]])break;int v=heap[p];heap[p]=heap[a];heap[a]=v;pos[heap[p]]=p;pos[v]=a;p=a;}}
        if(c==to)break;
        for(int d=0;d<AW_LINKS;d++){
            int n=aw_open(m,c,d);if(n<0||pos[n]==-2)continue;
            int cost=dist[c]+aw_move_cost(m,c,n);if(cost>=dist[n])continue;dist[n]=cost;prev[n]=c;
            int p=pos[n];if(p<0){p=size;heap[size++]=n;pos[n]=p;}
            while(p){int parent=(p-1)/2;if(dist[heap[parent]]<=dist[n])break;heap[p]=heap[parent];pos[heap[p]]=p;p=parent;}heap[p]=n;pos[n]=p;
        }
    }
    if(dist[to]==INT_MAX)return 0;
    for(int c=to;c>=0;c=prev[c])m->path[m->path_length++]=c;
    for(int i=0;i<m->path_length/2;i++){int v=m->path[i];m->path[i]=m->path[m->path_length-1-i];m->path[m->path_length-1-i]=v;}
    m->path_cost=dist[to];return 1;
}
/* Flood the visible water vertices from the map boundary. Every lake must
 * retain an interior below the waterline and no connection to the ocean.
 * Eight-neighbor flooding conservatively rejects diagonal leaks too. */
static int aw_lakes_valid(const AwMap*m){
    if(m->lake_count<1||m->lake_count>AW_LAKES)return 0;
    uint8_t wet[AW_VERT*AW_VERT],sea[AW_VERT*AW_VERT]={0};int queue[AW_VERT*AW_VERT],head=0,tail=0;
    for(int z=0;z<65;z++)for(int x=0;x<65;x++){
        int v=z*65+x;wet[v]=aw_height_q(m,(float)x,(float)z)<1.44f;
        if(wet[v]&&(!x||!z||x==64||z==64)){sea[v]=1;queue[tail++]=v;}
    }
    while(head<tail){int c=queue[head++],x=c%65,z=c/65;
        for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
            int nx=x+dx,nz=z+dz;if(nx<0||nz<0||nx>64||nz>64)continue;
            int n=nz*65+nx;if(wet[n]&&!sea[n]){sea[n]=1;queue[tail++]=n;}
        }
    }
    for(int i=0;i<m->lake_count;i++){
        const AwLake*l=&m->lakes[i];if(l->x<4||l->z<4||l->x>60||l->z>60||l->rx<3||l->rz<3||l->rx>7||l->rz>7)return 0;
        int v=l->z*65+l->x;if(!wet[v]||sea[v])return 0;
    }
    return 1;
}
static int aw_bridge_validate(const AwMap*m,const AwBridge*b);
static int aw_validate(AwMap*m){
    if(!aw_lakes_valid(m))return 0;
    for(int c=0;c<AW_CELLS;c++){
        AwCell*t=&m->cells[c];if(t->material>=AW_TILES)return 0;
        for(int k=0;k<4;k++){
            if(t->q[k]>40)return 0;
            int v=aw_corner_vertex(c,k),x=v%AW_VERT,z=v/AW_VERT;
            if((x<=AW_OCEAN_MARGIN||z<=AW_OCEAN_MARGIN||x>=AW_SIZE-AW_OCEAN_MARGIN||z>=AW_SIZE-AW_OCEAN_MARGIN)&&t->q[k]!=0)return 0;
        }
        if(t->tunnel&&!m->options.tunnels)return 0;

    }
    for(int c=0;c<AW_CELLS;c++)for(int d=0;d<4;d++){
        int n=aw_neighbor(c,d);if(n<0)continue;
        if(!aw_material_ok(m->cells[c].material,m->cells[n].material))return 0;
        if(!aw_edge_matches(m,c,n,d))return 0;
    }
    if(!aw_cave_validate(m)||!aw_traversal_build(m))return 0;
    aw_navigation(m);
    for(int s=0;s<2;s++){int floor=s?m->options.floors_b:m->options.floors_a;for(int k=0;k<4;k++)if(m->cells[m->spawns[s]].q[k]!=floor*4)return 0;}
    if(!m->reachable[m->spawns[1]]||!m->reachable[m->center])return 0;
    for(int r=0;r<4;r++)if(!m->reachable[m->resources[r]])return 0;
    for(int c=0;c<AW_CELLS;c++)if((m->cells[c].road||m->cave_access[c])&&!m->reachable[c])return 0;
    for(int i=0;i<m->cave_count;i++)if(!m->reachable[AW_CELLS+i])return 0;
    if(m->options.tunnels){
        /* Each entrance must have its own surface approach. Reachability via
         * the other cave mouth must not conceal an isolated landing. */
        uint8_t seen[AW_CELLS]={0};int queue[AW_CELLS],head=0,tail=0;
        queue[tail++]=m->spawns[0];seen[m->spawns[0]]=1;
        while(head<tail){int c=queue[head++];for(int d=0;d<4;d++){
            int n=aw_open(m,c,d);if(n>=0&&!seen[n]){seen[n]=1;queue[tail++]=n;}
        }}
        for(int side=0;side<2;side++)if(!seen[m->cave[m->cave_entrances[side]].portal])return 0;
    }
    for(int i=0;i<m->bridge_count;i++)if(!aw_bridge_validate(m,&m->bridges[i]))return 0;
    return aw_mountain_validate(m)&&aw_find_path(m,m->spawns[0],m->spawns[1]);
}
static uint32_t aw_fingerprint(const AwMap*m){
    uint32_t h=2166136261u;
    for(int i=0;i<AW_OCEAN_CELLS;i++){h=(h^m->ocean_depth[i])*16777619u;h=(h^m->ocean_connected[i])*16777619u;}
    for(int c=0;c<AW_CELLS;c++){const AwCell*t=&m->cells[c];for(int k=0;k<4;k++)h=(h^t->q[k])*16777619u;h=(h^t->material)*16777619u;h=(h^t->road)*16777619u;h=(h^t->tunnel)*16777619u;h=(h^t->portal)*16777619u;}
    for(int i=0;i<m->cave_count;i++){const AwCaveNode*n=&m->cave[i];h=(h^(uint16_t)n->x)*16777619u;h=(h^(uint16_t)n->z)*16777619u;h=(h^(uint16_t)n->q)*16777619u;h=(h^n->profile)*16777619u;}
    for(int i=0;i<m->cave_edge_count;i++){h=(h^m->cave_edges[i].a)*16777619u;h=(h^m->cave_edges[i].b)*16777619u;}
    for(int i=0;i<2;i++)h=(h^(uint32_t)m->spawns[i])*16777619u;
    for(int i=0;i<4;i++)h=(h^(uint32_t)m->resources[i])*16777619u;
    for(int i=0;i<m->lake_count;i++){
        const AwLake*l=&m->lakes[i];h=(h^(uint32_t)l->x)*16777619u;h=(h^(uint32_t)l->z)*16777619u;h=(h^(uint32_t)l->rx)*16777619u;h=(h^(uint32_t)l->rz)*16777619u;
    }
    for(int i=0;i<m->cave_room_count;i++)h=(h^(uint32_t)m->cave_rooms[i])*16777619u;
    for(int i=0;i<m->trail_count;i++){
        const AwTrailNode*n=&m->trail[i];h=(h^n->x)*16777619u;h=(h^n->z)*16777619u;h=(h^n->q)*16777619u;h=(h^n->profile)*16777619u;h=(h^n->mode)*16777619u;
    }
    for(int i=0;i<m->mountain_count;i++)for(int c=0;c<25;c++)h=(h^m->mountains[i].sockets[c])*16777619u;
    for(int v=0;v<AW_VERT*AW_VERT;v++)h=(h^m->blend[v])*16777619u;
    for(int i=0;i<m->bridge_count;i++){const AwBridge*b=&m->bridges[i];
        int data[]={b->x,b->z,b->dx,b->dz,b->length,b->qa,b->qb,b->crown};
        for(int k=0;k<8;k++)h=(h^(uint32_t)data[k])*16777619u;
    }
    return h;
}
/* Reserve low-cost, walkable approaches before material WFC.
 * Multi-source BFS starts at existing roads and follows only matching surface
 * sockets. The selected path changes material, never elevations or cliff shape. */
static int aw_cave_approaches(AwMap*m){
    if(!m->options.tunnels)return 1;
    aw_navigation(m);
    int prev[AW_CELLS],queue[AW_CELLS],head=0,tail=0;
    for(int c=0;c<AW_CELLS;c++){prev[c]=-1;if(m->cells[c].road){prev[c]=-2;queue[tail++]=c;}}
    while(head<tail){int c=queue[head++];for(int d=0;d<4;d++){
        int n=aw_open(m,c,d);if(n<0||prev[n]!=-1)continue;prev[n]=c;queue[tail++]=n;
    }}
    for(int side=0;side<(m->options.symmetry?1:2);side++){
        int portal=aw_cave_site(m,side,0);if(portal<0)return 0;
        for(int c=portal;c>=0&&prev[c]!=-2;c=prev[c]){
            m->cave_access[c]=1;
            if(m->options.symmetry)m->cave_access[AW_CELLS-1-c]=1;
        }
        for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
            int c=portal+dz*64+dx,flat=1;
            for(int k=0;k<4;k++)flat&=m->cells[c].q[k]==4;
            if(!flat||!m->reachable[c])continue;
            m->cave_access[c]=1;if(m->options.symmetry)m->cave_access[AW_CELLS-1-c]=1;
        }
    }
    return 1;
}
#include "natural_routes.h"
#include "bridges.h"
static int aw_generate_options(AwMap*m,uint32_t seed,AwOptions options){
    options.symmetry=!!options.symmetry;options.tunnels=!!options.tunnels;
    options.floors_a=aw_clamp(options.floors_a,1,10);options.floors_b=options.symmetry?options.floors_a:aw_clamp(options.floors_b,1,10);
    /* Keep palette IDs stable; retired mixed/volcanic IDs use Temperate. */
    if(options.biome<AW_TEMPERATE||options.biome>AW_FROZEN)options.biome=AW_TEMPERATE;
    /* Establish a valid world before fitting optional passages to its rock. */
    for(int layout=0;layout<24;layout++){
        memset(m,0,sizeof(*m));m->seed=seed;m->layout_seed=aw_hash(seed^(uint32_t)layout*0x9e3779b9u);m->rng=m->layout_seed;m->options=options;m->attempts=1;m->layout_attempts=layout+1;
        if(!aw_layout(m)||!aw_shape_wfc(m)||!aw_cave_approaches(m)||!aw_wfc(m))continue;
        uint32_t surface_rng=m->rng;
        for(int attempt=0;attempt<(options.tunnels?16:1);attempt++){
            aw_cave_clear(m);aw_navigation(m);m->rng=surface_rng;m->attempts=attempt+1;
            if(!aw_caves(m,attempt))continue;
            m->valid=aw_validate(m);
            if(m->valid){aw_natural_passages(m);aw_bridges(m);aw_ocean_build(m);m->hash=aw_fingerprint(m);return 1;}
        }
    }
    return 0;
}

static int aw_generate(AwMap*m,uint32_t seed){return aw_generate_options(m,seed,aw_defaults());}
static int aw_solid(const AwMap*m,float x,float yq,float z){
    if(x<0||z<0||x>=AW_SIZE||z>=AW_SIZE)return 0;
    return aw_density(m,x,yq,z)>0;
}
static int aw_occluded(const AwMap*m,float ex,float eq,float ez,float tx,float tq,float tz){
    float dx=tx-ex,dy=tq-eq,dz=tz-ez;int steps=(int)(sqrtf(dx*dx+dy*dy+dz*dz)*8)+1;
    for(int i=1;i<steps-1;i++){float t=(float)i/steps;if(aw_solid(m,ex+dx*t,eq+dy*t,ez+dz*t))return 1;}return 0;
}
#endif
