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
#define AW_LINKS 7
#define AW_NODES (AW_CELLS+AW_CAVE_NODES)
#define AW_VERSION 5
#define AW_SUBDIV 6
#define AW_SHAPES 16
#define AW_MAX_FLOOR 10
#define AW_TILES 12
#define AW_ALL ((1u<<AW_TILES)-1)
enum {AW_GRASS,AW_FOREST,AW_DIRT,AW_SAND,AW_ROCK,AW_SNOW,AW_ICE,AW_MUD,AW_SHALLOW,AW_DEEP,AW_LAVA,AW_ROAD};
static const char *aw_terrain_names[AW_TILES]={"Grass","Forest","Soil","Sand","Rock","Snow","Ice","Mud","Shallow water","Deep water","Lava","Road"};
static const int aw_cost[AW_TILES]={12,19,13,18,16,20,23,28,34,0,0,10};
typedef struct {int symmetry,floors_a,floors_b,biome,tunnels;} AwOptions;
typedef struct {uint8_t q[4],material,road,tunnel,portal;} AwCell;
typedef struct {int16_t x,z,q,portal; uint8_t profile,junction; int16_t links[6];} AwCaveNode;
typedef struct {int16_t a,b;} AwCaveEdge;
typedef struct {
    AwCaveNode cave[AW_CAVE_NODES]; AwCaveEdge cave_edges[AW_CAVE_EDGES];
    uint16_t cave_bins[AW_CELLS][AW_BIN_SIZE]; uint8_t cave_bin_count[AW_CELLS];
    int cave_count,cave_edge_count,cave_decisions,cave_expanded,cave_backtracks;
    int cave_entrances[2],cave_hubs[2];
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
static AwOptions aw_defaults(void){return (AwOptions){1,6,6,0,1};}
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
static int aw_corner_q(const AwMap*m,int node,int k){return node>=AW_CELLS?m->cave[node-AW_CELLS].q:m->cells[node].q[k];}
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
/* A road is a three-cell-wide sweep with shared vertex elevations. Flatten bends
 * before distributing quarter-floor rises, so the outer lanes are continuous too. */
static int aw_road(AwMap*m,int side){
    int pts[8][2]={{13,13},{13,7},{6,7},{6,25},{22,25},{22,7},{28,7},{28,31}};
    uint32_t route=aw_hash(m->seed ^ (side&&!m->options.symmetry?9187u:0u));
    int west=5+(route%2),south=25+((route>>4)%2);
    pts[2][0]=pts[3][0]=west;pts[3][1]=pts[4][1]=south;
    if(side&&!m->options.symmetry){pts[0][0]=12;pts[0][1]=15;pts[1][0]=12;pts[2][0]=7;pts[3][0]=7;pts[3][1]=26;pts[4][1]=26;}
    int px[160],pz[160],hq[160],turn[8],n=0;
    px[n]=pts[0][0];pz[n++]=pts[0][1];turn[0]=0;
    for(int p=1;p<8;p++){
        int x=px[n-1],z=pz[n-1];
        while(x!=pts[p][0]||z!=pts[p][1]){x+=(pts[p][0]>x)-(pts[p][0]<x);z+=(pts[p][1]>z)-(pts[p][1]<z);px[n]=x;pz[n++]=z;}
        turn[p]=n-1;
    }
    int eligible[160]={0},total=0;
    for(int i=1;i<n;i++){
        int ok=i>3&&i<n-24;
        for(int p=1;p<7;p++)if(aw_abs(i-turn[p])<=2)ok=0;
        total+=eligible[i]=ok;
    }
    int floor=side?m->options.floors_b:m->options.floors_a,drop=4*(floor-1);
    if(total<drop)return 0;
    /* Share the same structural choices across a rotational pair. */
    uint32_t saved_rng=m->rng;m->rng=aw_hash(m->seed^(side&&!m->options.symmetry?12357u:0u));
    if(!aw_road_wfc(m,hq,eligible,n,floor))return 0;
    m->rng=saved_rng;
    uint8_t road[AW_CELLS]={0};
    int vq[AW_VERT*AW_VERT];
    for(int i=0;i<n;i++)for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++)road[(pz[i]+dz)*AW_SIZE+px[i]+dx]=1;
    for(int z=pts[0][1]-2;z<=pts[0][1]+2;z++)for(int x=pts[0][0]-2;x<=pts[0][0]+2;x++)road[z*AW_SIZE+x]=1;
    for(int z=0;z<AW_VERT;z++)for(int x=0;x<AW_VERT;x++){
        int best=INT_MAX,q=4;
        for(int i=1;i<n;i++){
            int ax=px[i-1]*2+1,az=pz[i-1]*2+1,dx=px[i]-px[i-1],dz=pz[i]-pz[i-1];
            int t=aw_clamp((x*2-ax)*dx+(z*2-az)*dz,0,2);
            int ex=x*2-ax-dx*t,ez=z*2-az-dz*t,dist=ex*ex+ez*ez;
            if(dist<best){best=dist;q=(hq[i-1]*(2-t)+hq[i]*t)/2;}
        }
        if(aw_abs(x*2-(pts[0][0]*2+1))<=5&&aw_abs(z*2-(pts[0][1]*2+1))<=5)q=floor*4;
        vq[z*AW_VERT+x]=q;
    }
    for(int c=0;c<AW_CELLS;c++)if(road[c]){
        int x=c%AW_SIZE,z=c/AW_SIZE,dst=side?AW_CELLS-1-c:c;
        int q[4]={vq[z*AW_VERT+x],vq[z*AW_VERT+x+1],vq[(z+1)*AW_VERT+x+1],vq[(z+1)*AW_VERT+x]};
        m->cells[dst].road=1;
        for(int k=0;k<4;k++)m->cells[dst].q[(k+side*2)%4]=q[k];
    }
    int spawn=pts[0][1]*AW_SIZE+pts[0][0];m->spawns[side]=side?AW_CELLS-1-spawn:spawn;
    int resource=pts[0][1]*AW_SIZE+pts[0][0]+2;m->resources[side*2]=side?AW_CELLS-1-resource:resource;
    resource=pts[3][1]*AW_SIZE+15;m->resources[side*2+1]=side?AW_CELLS-1-resource:resource;
    /* The asymmetric lane still has its strategic resource on its own road. */
    if(side&&!m->options.symmetry)m->resources[3]=AW_CELLS-1-(26*AW_SIZE+15);
    return 1;
}
static int aw_layout(AwMap*m){
    for(int c=0;c<AW_CELLS;c++){
        int cc=m->options.symmetry&&c>=AW_CELLS/2?AW_CELLS-1-c:c,x=cc%AW_SIZE,z=cc/AW_SIZE;
        int noise=aw_noise(m->seed,x,z,9),detail=aw_noise(m->seed^913u,x,z,4);
        int edge=x<z?x:z;edge=edge<63-x?edge:63-x;edge=edge<63-z?edge:63-z;
        int floor=edge<3+(noise/60)?0:1+(noise>110)+(noise>155)+(detail>185);
        int da=aw_abs(x-13)+aw_abs(z-13),db=aw_abs(x-50)+aw_abs(z-50);
        int hill=m->options.floors_a-da/3;if(m->options.floors_b-db/3>hill)hill=m->options.floors_b-db/3;
        if(edge>5&&hill>floor)floor=hill;
        aw_flat(&m->cells[c],aw_clamp(floor,0,10)*4);
    }
    if(!aw_road(m,0)||!aw_road(m,1))return 0;
    for(int z=18;z<=45;z++)for(int x=31;x<=32;x++){
        AwCell*c=&m->cells[z*AW_SIZE+x];c->road=1;
        for(int k=0;k<4;k++){int vz=z+(k>=2);c->q[k]=4+aw_clamp(vz-18<46-vz?vz-18:46-vz,0,12);}
    }
    for(int side=0;side<2;side++)for(int z=17;z<=17;z++)for(int x=28;x<=32;x++){
        int i=z*AW_SIZE+x;if(side)i=AW_CELLS-1-i;
        /* Do not overwrite the sloping cell at the foot of the upper road. */
        if(z==18&&x>=31)continue;
        m->cells[i].road=1;aw_flat(&m->cells[i],4);
    }

    m->center=31*AW_SIZE+31;
    return 1;
}
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
    const int islands[7][3]={{13,13,15},{51,51,15},{32,32,15},{14,44,12},{50,20,12},{28,13,10},{36,51,10}};
    for(int v=0;v<AW_VERT*AW_VERT;v++){
        int cv=m->options.symmetry&&v>AW_VERT*AW_VERT/2?AW_VERT*AW_VERT-1-v:v;
        int x=cv%AW_VERT,z=cv/AW_VERT,land=-10000;
        for(int i=0;i<7;i++){
            int dx=x-islands[i][0],dz=z-islands[i][1],r=islands[i][2];
            int value=r*r-dx*dx-dz*dz;if(value>land)land=value;
        }
        land+=(aw_noise(m->seed,x,z,5)-128)/2+(aw_noise(m->seed^913u,x,z,2)-128)/5;
        int target=land<-24?0:land<24?2:4;
        if(land>24){
            for(int i=0;i<2;i++){
                int c=m->spawns[i],dx=x-(c%AW_SIZE),dz=z-(c/AW_SIZE),d=aw_isqrt(dx*dx+dz*dz);
                int floor=i?m->options.floors_b:m->options.floors_a;
                int q=floor*4-(d>6?(d-6)*2:0);if(q>target)target=q;
            }
            for(int i=0;i<2;i++){
                int dx=x-(i?50:14),dz=z-(i?20:44),d=aw_isqrt(dx*dx+dz*dz);
                int q=12-(d>4?(d-4)*2:0);if(q>target)target=q;
            }
        }
        if(x<2||z<2||x>62||z>62)target=0;
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
        if(pin[v]>=0)lo[v]=hi[v]=preferred[v]=pin[v];
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
    if((a==AW_LAVA&&(b==AW_FOREST||b==AW_SNOW||b==AW_ICE||b==AW_SHALLOW||b==AW_DEEP))||(b==AW_LAVA&&(a==AW_FOREST||a==AW_SNOW||a==AW_ICE||a==AW_SHALLOW||a==AW_DEEP)))return 0;
    if((a==AW_SAND&&(b==AW_SNOW||b==AW_ICE))||(b==AW_SAND&&(a==AW_SNOW||a==AW_ICE)))return 0;
    return 1;
}
static uint32_t aw_domain(AwMap*m,int c){
    AwCell*t=&m->cells[c];if(t->road||m->cave_access[c])return 1u<<AW_ROAD;if(!(t->q[0]|t->q[1]|t->q[2]|t->q[3]))return 1u<<AW_DEEP;
    int cc=m->options.symmetry&&c>=AW_CELLS/2?AW_CELLS-1-c:c,x=cc%AW_SIZE,z=cc/AW_SIZE;
    int climate=aw_noise(m->seed^71391u,x,z,12),wet=aw_noise(m->seed^317u,x,z,7),biome=m->options.biome;
    if(!biome)biome=climate<145?1:climate<190?2:climate<220?3:4;
    uint32_t mask=1u<<AW_ROCK;
    if(biome==1)mask|=(1u<<AW_GRASS)|(1u<<AW_DIRT)|(1u<<(wet>130?AW_FOREST:AW_MUD));
    if(biome==2)mask|=(1u<<AW_SAND)|(1u<<AW_DIRT);
    if(biome==3)mask|=(1u<<AW_SNOW)|(1u<<AW_ICE);
    if(biome==4)mask|=(1u<<AW_DIRT)|(1u<<AW_LAVA);
    if(t->q[0]==4&&wet>165&&biome!=4)mask|=1u<<AW_SHALLOW;
    return mask;
}
static int aw_preferred_material(const AwMap*m,int c){
    int cc=m->options.symmetry&&c>=AW_CELLS/2?AW_CELLS-1-c:c,x=cc%AW_SIZE,z=cc/AW_SIZE;
    int climate=aw_noise(m->seed^71391u,x,z,12),wet=aw_noise(m->seed^317u,x,z,7),detail=aw_noise(m->seed^715u,x,z,5),biome=m->options.biome;
    if(!biome)biome=climate<145?1:climate<190?2:climate<220?3:4;
    if(m->cells[c].q[0]==4&&wet>180&&biome!=4)return AW_SHALLOW;
    if(detail>202)return AW_ROCK;
    if(biome==1)return wet>155?AW_FOREST:wet<75?AW_MUD:detail>170?AW_DIRT:AW_GRASS;
    if(biome==2)return wet>85?AW_SAND:AW_DIRT;
    if(biome==3)return wet>100?AW_SNOW:AW_ICE;
    return wet>175||wet<75?AW_LAVA:AW_DIRT;
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

#include "caves.h"
static int aw_open(const AwMap*m,int node,int d){
    if(node<0||node>=AW_NODES||!m->walkable[node]||d<0||d>=AW_LINKS)return -1;
    if(node>=AW_CELLS){
        const AwCaveNode*n=&m->cave[node-AW_CELLS];
        int next=d==6?n->portal:(n->links[d]<0?-1:AW_CELLS+n->links[d]);
        return next>=0&&m->walkable[next]?next:-1;
    }
    if(d==4){for(int i=0;i<m->cave_count;i++)if(m->cave[i].portal==node)return AW_CELLS+i;return -1;}
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
        m->walkable[c]=aw_cost[m->cells[c].material]>0&&lo>=4&&hi-lo<=1;
        if(m->walkable[c]&&m->cave_bin_count[c]){
            float q=aw_surface_q(m,c,0.5f,0.5f);
            if(fabsf(aw_support_q(m,c%64+0.5f,c/64+0.5f,q)-q)>0.1f)m->walkable[c]=0;
        }
        m->walk_count+=m->walkable[c];
    }
    for(int i=0;i<m->cave_count;i++){m->walkable[AW_CELLS+i]=1;m->walk_count++;}
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
    return (a<AW_CELLS&&b>=AW_CELLS&&m->cave[b-AW_CELLS].portal==a)||(b<AW_CELLS&&a>=AW_CELLS&&m->cave[a-AW_CELLS].portal==b)?1:(ca+cb)/2+aw_abs(qa-qb);
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
static int aw_validate(AwMap*m){
    for(int c=0;c<AW_CELLS;c++){
        AwCell*t=&m->cells[c];if(t->material>=AW_TILES)return 0;
        for(int k=0;k<4;k++)if(t->q[k]>40)return 0;
        if(t->tunnel&&!m->options.tunnels)return 0;

    }
    for(int c=0;c<AW_CELLS;c++)for(int d=0;d<4;d++){
        int n=aw_neighbor(c,d);if(n<0)continue;
        if(!aw_material_ok(m->cells[c].material,m->cells[n].material))return 0;
        if(!aw_edge_matches(m,c,n,d))return 0;
    }
    if(!aw_cave_validate(m))return 0;
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
    return aw_find_path(m,m->spawns[0],m->spawns[1]);
}
static uint32_t aw_fingerprint(const AwMap*m){
    uint32_t h=2166136261u;
    for(int c=0;c<AW_CELLS;c++){const AwCell*t=&m->cells[c];for(int k=0;k<4;k++)h=(h^t->q[k])*16777619u;h=(h^t->material)*16777619u;h=(h^t->road)*16777619u;h=(h^t->tunnel)*16777619u;h=(h^t->portal)*16777619u;}
    for(int i=0;i<m->cave_count;i++){const AwCaveNode*n=&m->cave[i];h=(h^(uint16_t)n->x)*16777619u;h=(h^(uint16_t)n->z)*16777619u;h=(h^(uint16_t)n->q)*16777619u;h=(h^n->profile)*16777619u;}
    for(int i=0;i<m->cave_edge_count;i++){h=(h^m->cave_edges[i].a)*16777619u;h=(h^m->cave_edges[i].b)*16777619u;}
    return h;
}
/* Reserve walkable approaches before material WFC can put lava on them.
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
static int aw_generate_options(AwMap*m,uint32_t seed,AwOptions options){
    options.symmetry=!!options.symmetry;options.tunnels=!!options.tunnels;
    options.floors_a=aw_clamp(options.floors_a,1,10);options.floors_b=options.symmetry?options.floors_a:aw_clamp(options.floors_b,1,10);options.biome=aw_clamp(options.biome,0,4);
    memset(m,0,sizeof(*m));m->seed=seed;m->rng=seed;m->options=options;m->attempts=1;
    if(!aw_layout(m)||!aw_shape_wfc(m)||!aw_cave_approaches(m)||!aw_wfc(m))return 0;
    uint32_t surface_rng=m->rng;
    for(int attempt=0;attempt<(options.tunnels?16:1);attempt++){
        aw_cave_clear(m);aw_navigation(m);m->rng=surface_rng;m->attempts=attempt+1;
        if(!aw_caves(m,attempt))continue;
        m->valid=aw_validate(m);
        if(m->valid){m->hash=aw_fingerprint(m);return 1;}
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
