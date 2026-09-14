#ifndef ALIENWARS_MAP_H
#define ALIENWARS_MAP_H
/* Deterministic terrain grammar, material WFC and two-span navigation. No renderer dependencies. */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#define AW_SIZE 64
#define AW_VERT (AW_SIZE+1)
#define AW_CELLS (AW_SIZE*AW_SIZE)
#define AW_NODES (2*AW_CELLS)
#define AW_VERSION 2
#define AW_MAX_FLOOR 10
#define AW_TILES 12
#define AW_ALL ((1u<<AW_TILES)-1)
enum {AW_GRASS,AW_FOREST,AW_DIRT,AW_SAND,AW_ROCK,AW_SNOW,AW_ICE,AW_MUD,AW_SHALLOW,AW_DEEP,AW_LAVA,AW_ROAD};
static const char *aw_terrain_names[AW_TILES]={"Grass","Forest","Soil","Sand","Rock","Snow","Ice","Mud","Shallow water","Deep water","Lava","Road"};
static const int aw_cost[AW_TILES]={12,19,13,18,16,20,23,28,34,0,0,10};
typedef struct {int symmetry,floors_a,floors_b,biome,tunnels;} AwOptions;
typedef struct {uint8_t q[4],material,road,tunnel,portal;} AwCell;
typedef struct {
    uint32_t seed,rng,hash,wave[AW_CELLS],compatible[AW_TILES];
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
static int aw_corner_q(const AwMap*m,int node,int k){return node>=AW_CELLS?4:m->cells[node].q[k];}
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
/* Tunnel modules expose two- or four-cell-wide passage sockets. WFC chooses
 * straight, expanding, chamber, and contracting pieces; endpoints stay narrow.
 * Mirrored modules reverse their entry/exit sockets for rotational layouts. */
static int aw_tunnel_wfc(AwMap*m){
    uint32_t wave[4]={3,15,15,5}; /* bit0:2->2, bit1:2->4, bit2:4->2, bit3:4->4 */
    int reverse[4]={0,2,1,3};
    for(;;){
        int changed=1;
        while(changed){
            changed=0;
            for(int i=0;i<4;i++){
                uint32_t allowed=0;
                for(int t=0;t<4;t++)if(wave[i]&(1u<<t))allowed|=1u<<reverse[t];
                if(m->options.symmetry){uint32_t mask=wave[3-i]&allowed;if(!mask)return 0;if(mask!=wave[3-i]){wave[3-i]=mask;changed=1;m->reductions++;}}
                for(int d=-1;d<=1;d+=2){
                    int n=i+d;if(n<0||n>3)continue;uint32_t mask=0;
                    for(int a=0;a<4;a++)for(int b=0;b<4;b++)if((wave[i]&(1u<<a))&&(wave[n]&(1u<<b))&&((d==1?(a&1):(a>>1))==(d==1?(b>>1):(b&1))))mask|=1u<<b;
                    if(!mask)return 0;if(mask!=wave[n]){wave[n]=mask;changed=1;m->reductions++;}
                }
            }
        }
        int best=-1,entropy=99;
        for(int i=0;i<4;i++){int n=__builtin_popcount(wave[i]);if(n>1&&n<entropy){best=i;entropy=n;}}
        if(best<0)break;int pick=aw_random(m)%entropy;uint32_t bits=wave[best];while(pick--)bits&=bits-1;wave[best]=bits&(~bits+1);m->structure_decisions++;
    }
    for(int x=29;x<=34;x++){
        int wide=x>29&&x<34&&__builtin_ctz(wave[x-30])!=0;
        for(int z=wide?30:31;z<=(wide?33:32);z++){
            AwCell*c=&m->cells[z*AW_SIZE+x];c->tunnel=1;c->portal=x==29||x==34;
            if(c->portal){c->road=1;aw_flat(c,4);}
        }
    }
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
    /* A central ridge with a separate east/west void below its north/south road. */
    for(int z=25;z<=38;z++)for(int x=30;x<=33;x++){
        AwCell*c=&m->cells[z*AW_SIZE+x];aw_flat(c,16);c->road=0;
    }
    for(int z=18;z<=45;z++)for(int x=31;x<=32;x++){
        AwCell*c=&m->cells[z*AW_SIZE+x];c->road=1;
        for(int k=0;k<4;k++){int vz=z+(k>=2);c->q[k]=4+aw_clamp(vz-18<46-vz?vz-18:46-vz,0,12);}
    }
    for(int side=0;side<2;side++)for(int z=17;z<=18;z++)for(int x=28;x<=32;x++){
        int i=z*AW_SIZE+x;if(side)i=AW_CELLS-1-i;
        /* Do not overwrite the sloping cell at the foot of the upper road. */
        if(z==18&&x>=31)continue;
        m->cells[i].road=1;aw_flat(&m->cells[i],4);
    }
    if(m->options.tunnels&&!aw_tunnel_wfc(m))return 0;
    m->center=31*AW_SIZE+31;
    return 1;
}
static int aw_material_ok(int a,int b){
    if((a==AW_LAVA&&(b==AW_FOREST||b==AW_SNOW||b==AW_ICE||b==AW_SHALLOW||b==AW_DEEP))||(b==AW_LAVA&&(a==AW_FOREST||a==AW_SNOW||a==AW_ICE||a==AW_SHALLOW||a==AW_DEEP)))return 0;
    if((a==AW_SAND&&(b==AW_SNOW||b==AW_ICE))||(b==AW_SAND&&(a==AW_SNOW||a==AW_ICE)))return 0;
    return 1;
}
static uint32_t aw_domain(AwMap*m,int c){
    AwCell*t=&m->cells[c];if(t->road)return 1u<<AW_ROAD;if(!t->q[0])return 1u<<AW_DEEP;
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
    uint8_t recorded[AW_CELLS]={0};
    for(;;){
        int best=-1,entropy=99;uint32_t tie=UINT_MAX;
        for(int c=0;c<AW_CELLS;c++){
            int n=__builtin_popcount(m->wave[c]);
            if(n==1&&!recorded[c]){recorded[c]=1;m->order[m->order_count++]=c;}
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
static int aw_open(const AwMap*m,int node,int d){
    if(node<0||node>=AW_NODES||!m->walkable[node])return -1;
    int c=node%AW_CELLS,layer=node/AW_CELLS;
    if(d==4){int next=layer?c:c+AW_CELLS;return m->cells[c].portal&&m->walkable[next]?next:-1;}
    if(d<0||d>3)return -1;
    int next=aw_neighbor(c,d);if(next<0)return -1;next+=layer*AW_CELLS;
    return m->walkable[next]&&aw_edge_matches(m,node,next,d)?next:-1;
}
static void aw_navigation(AwMap*m){
    m->walk_count=m->reached_count=m->tunnel_count=0;
    memset(m->reachable,0,sizeof(m->reachable));
    for(int c=0;c<AW_CELLS;c++){
        int lo=40,hi=0;for(int k=0;k<4;k++){int q=m->cells[c].q[k];if(q<lo)lo=q;if(q>hi)hi=q;}
        m->walkable[c]=aw_cost[m->cells[c].material]>0&&lo>=4&&hi-lo<=1;
        m->walkable[c+AW_CELLS]=m->cells[c].tunnel;
        m->walk_count+=m->walkable[c]+m->walkable[c+AW_CELLS];m->tunnel_count+=m->cells[c].tunnel;
    }
    int queue[AW_NODES],head=0,tail=0,start=m->spawns[0];
    if(!m->walkable[start])return;
    queue[tail++]=start;m->reachable[start]=1;
    while(head<tail){int c=queue[head++];for(int d=0;d<5;d++){int n=aw_open(m,c,d);if(n>=0&&!m->reachable[n]){m->reachable[n]=1;queue[tail++]=n;}}}
    m->reached_count=tail;
}
static int aw_move_cost(const AwMap*m,int a,int b){
    int ca=a>=AW_CELLS?10:aw_cost[m->cells[a].material],cb=b>=AW_CELLS?10:aw_cost[m->cells[b].material];
    int qa=0,qb=0;for(int k=0;k<4;k++){qa+=aw_corner_q(m,a,k);qb+=aw_corner_q(m,b,k);}
    return a%AW_CELLS==b%AW_CELLS?1:(ca+cb)/2+aw_abs(qa-qb);
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
        for(int d=0;d<5;d++){
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
        if(t->tunnel&&!t->portal)for(int k=0;k<4;k++)if(t->q[k]<12)return 0;
        if(t->portal)for(int k=0;k<4;k++)if(t->q[k]!=4)return 0;
    }
    for(int c=0;c<AW_CELLS;c++)for(int d=0;d<4;d++){int n=aw_neighbor(c,d);if(n>=0&&!aw_material_ok(m->cells[c].material,m->cells[n].material))return 0;}
    aw_navigation(m);
    for(int s=0;s<2;s++){int floor=s?m->options.floors_b:m->options.floors_a;for(int k=0;k<4;k++)if(m->cells[m->spawns[s]].q[k]!=floor*4)return 0;}
    if(!m->reachable[m->spawns[1]]||!m->reachable[m->center])return 0;
    for(int r=0;r<4;r++)if(!m->reachable[m->resources[r]])return 0;
    for(int c=0;c<AW_CELLS;c++)if((m->cells[c].road&&!m->reachable[c])||(m->cells[c].tunnel&&!m->reachable[c+AW_CELLS]))return 0;
    return aw_find_path(m,m->spawns[0],m->spawns[1]);
}
static uint32_t aw_fingerprint(const AwMap*m){
    uint32_t h=2166136261u;
    for(int c=0;c<AW_CELLS;c++){const AwCell*t=&m->cells[c];for(int k=0;k<4;k++)h=(h^t->q[k])*16777619u;h=(h^t->material)*16777619u;h=(h^t->road)*16777619u;h=(h^t->tunnel)*16777619u;h=(h^t->portal)*16777619u;}
    return h;
}
static int aw_generate_options(AwMap*m,uint32_t seed,AwOptions options){
    options.symmetry=!!options.symmetry;options.tunnels=!!options.tunnels;
    options.floors_a=aw_clamp(options.floors_a,1,10);options.floors_b=options.symmetry?options.floors_a:aw_clamp(options.floors_b,1,10);options.biome=aw_clamp(options.biome,0,4);
    memset(m,0,sizeof(*m));m->seed=seed;m->rng=seed;m->options=options;m->attempts=1;
    if(!aw_layout(m)||!aw_wfc(m))return 0;
    m->valid=aw_validate(m);m->hash=aw_fingerprint(m);return m->valid;
}
static int aw_generate(AwMap*m,uint32_t seed){return aw_generate_options(m,seed,aw_defaults());}
/* Visibility uses the same triangular surface and tunnel void as navigation.
 * Coordinates here are grid units horizontally and quarter-floors vertically. */
static float aw_surface_q(const AwMap*m,int c,float fx,float fz){
    const uint8_t*q=m->cells[c].q;
    return fx>=fz?q[0]+(q[1]-q[0])*fx+(q[2]-q[1])*fz:q[0]+(q[2]-q[3])*fx+(q[3]-q[0])*fz;
}
static int aw_solid(const AwMap*m,float x,float yq,float z){
    if(x<0||z<0||x>=AW_SIZE||z>=AW_SIZE||yq<0)return 0;
    int c=(int)z*AW_SIZE+(int)x;const AwCell*t=&m->cells[c];
    if(yq>=aw_surface_q(m,c,x-(int)x,z-(int)z))return 0;
    if(t->tunnel&&yq>4&&yq<10)return 0;
    return 1;
}
static int aw_occluded(const AwMap*m,float ex,float eq,float ez,float tx,float tq,float tz){
    float dx=tx-ex,dy=tq-eq,dz=tz-ez;int steps=(int)(sqrtf(dx*dx+dy*dy+dz*dz)*8)+1;
    for(int i=1;i<steps-1;i++){float t=(float)i/steps;if(aw_solid(m,ex+dx*t,eq+dy*t,ez+dz*t))return 1;}return 0;
}
#endif
