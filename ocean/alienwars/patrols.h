#ifndef ALIENWARS_PATROLS_H
#define ALIENWARS_PATROLS_H
#include "map.h"
#include "volume_routes.h"
/* Eight ambient patrols plus the existing inspection scout = three of each
 * ground/naval/air class. Scripted traffic, independent of the map fingerprint
 * and the inspection route; no combat, learning or unit-to-unit avoidance. */
#define AW_PATROLS 11
#define AW_UNITS (AW_PATROLS+1)
#define AW_AIR_POINTS 512
enum {AW_PATROL_GROUND,AW_PATROL_NAVAL,AW_PATROL_AIR,AW_PATROL_SUB};
typedef struct {
    int layer,variant,count,route[AW_NODES];float progress,speed;
    float altitude[AW_NODES];
} AwPatrol;
typedef struct {AwPatrol units[AW_PATROLS];int count;} AwPatrols;
typedef struct {float x,q,z;} AwPatrolPoint;
static float aw_patrol_floor(const AwMap*m,int node){
    return node>=AW_SPAN_START?m->spans[node-AW_SPAN_START].q:node>=AW_CELLS?m->cave[node-AW_CELLS].q:aw_surface_q(m,node,.5f,.5f);
}
static int aw_patrol_ground_edge(const AwMap*m,int a,int b){
    if(a>=AW_SPAN_START&&b>=AW_SPAN_START){
        for(int d=0;d<4;d++)if(m->spans[a-AW_SPAN_START].links[d]==b-AW_SPAN_START)return !!(m->spans[a-AW_SPAN_START].edge_fits[d]&2);
        return 0;
    }
    int ca=aw_node_cell(m,a),cb=aw_node_cell(m,b);float qa=aw_patrol_floor(m,a),qb=aw_patrol_floor(m,b);
    if(fabsf(qa-qb)>1.1f)return 0;
    for(int j=1;j<4;j++){
        float t=j*.25f,x=aw_lerp(ca%64,cb%64,t)+.5f,z=aw_lerp(ca/64,cb/64,t)+.5f,q=aw_lerp(qa,qb,t);
        float support=aw_support_q(m,x,z,q);
        if(fabsf(support-q)>.3f||!aw_body_fits(m,x,support,z,1))return 0;
    }return 1;
}
typedef struct {const AwMap*m;int layer,variant;const uint8_t*allowed;} AwPatrolGraph;
static AwRoutePoint aw_patrol_node(const AwPatrolGraph*g,int n){
    if(g->layer==AW_PATROL_NAVAL)return (AwRoutePoint){n%96-16+.5f,1.44f,n/96-16+.5f};
    int c=aw_node_cell(g->m,n);return (AwRoutePoint){c%64+.5f,aw_patrol_floor(g->m,n),c/64+.5f};
}
static float aw_patrol_estimate(void*ctx,int a,int b){AwPatrolGraph*g=ctx;return aw_route_distance(aw_patrol_node(g,a),aw_patrol_node(g,b));}
static int aw_patrol_edges(void*ctx,int a,AwAStarEdge*out){
    AwPatrolGraph*g=ctx;int count=0;
    for(int d=0;d<(g->layer==AW_PATROL_NAVAL?4:AW_LINKS);d++){
        int b=g->layer==AW_PATROL_NAVAL?aw_ocean_neighbor(a,d):aw_open(g->m,a,d);
        if(b<0||!g->allowed[b]||(g->layer==AW_PATROL_GROUND&&!aw_patrol_ground_edge(g->m,a,b)))continue;
        float cost=aw_patrol_estimate(g,a,b);if(g->layer==AW_PATROL_GROUND)cost*=fmaxf(1,aw_move_cost(g->m,a,b)/10.0f);
        out[count++]=(AwAStarEdge){b,cost};
    }return count;
}
static int aw_patrol_search(AwPatrolGraph*g,int start,int goal,int*out){
    AwAStar scratch={0};int nodes=g->layer==AW_PATROL_NAVAL?AW_OCEAN_CELLS:AW_NODES;
    if(!aw_astar_init(&scratch,nodes)){aw_astar_close(&scratch);return 0;}
    int n=aw_astar_path(&scratch,nodes,start,goal,g,aw_patrol_edges,aw_patrol_estimate,out,AW_NODES);aw_astar_close(&scratch);return n;
}
static int aw_patrol_ground(AwMap*m,AwPatrol*p,uint32_t salt){
    uint8_t allowed[AW_NODES]={0};int prev[AW_NODES],queue[AW_NODES],depth[AW_NODES]={0};
    for(int c=0;c<AW_CELLS;c++)allowed[c]=m->walkable[c]&&aw_body_fits(m,c%64+.5f,aw_surface_q(m,c,.5f,.5f),c/64+.5f,1);
    for(int s=0;s<m->span_count;s++)allowed[AW_SPAN_START+s]=!!(m->spans[s].fits&2);
    int start=m->spawns[p->variant==2],goal=-1;
    if(p->variant==1&&m->bridge_count){
        const AwBridge*b=&m->bridges[0];int a=b->z*64+b->x,c=(b->z+b->dz*b->length)*64+b->x+b->dx*b->length;
        start=AW_SPAN_START+aw_span_find(m,a,b->qa);goal=AW_SPAN_START+aw_span_find(m,c,b->qb);
    }
    if(!allowed[start])return 0;
    for(int n=0;n<AW_NODES;n++)prev[n]=-1;
    int head=0,tail=0,best=start,best_score=0;queue[tail++]=start;prev[start]=start;
    while(head<tail){int c=queue[head++];
        int score=depth[c]*8+(aw_hash(salt^(uint32_t)c)%80);if(score>best_score){best_score=score;best=c;}
        if(c==goal){best=c;break;}
        for(int d=0;d<AW_LINKS;d++){
            int n=aw_open(m,c,d);if(n<0||!allowed[n]||prev[n]>=0||!aw_patrol_ground_edge(m,c,n))continue;
            prev[n]=c;depth[n]=depth[c]+1;queue[tail++]=n;
        }
    }
    if(goal>=0&&prev[goal]>=0)best=goal;
    AwPatrolGraph graph={m,AW_PATROL_GROUND,p->variant,allowed};
    p->count=aw_patrol_search(&graph,start,best,p->route);
    return p->count>1;
}
static int aw_patrol_water_cell(const AwMap*m,int c,int variant){
    static const int draft[3]={100,250,500};int radius=variant?1:0;
    int x=c%AW_OCEAN_SIZE,z=c/AW_OCEAN_SIZE;
    for(int dz=-radius;dz<=radius;dz++)for(int dx=-radius;dx<=radius;dx++){
        int nx=x+dx,nz=z+dz;if(nx<0||nz<0||nx>=AW_OCEAN_SIZE||nz>=AW_OCEAN_SIZE)return 0;
        int n=nz*AW_OCEAN_SIZE+nx;if(!m->ocean_connected[n]||m->ocean_depth[n]<draft[variant])return 0;
        float gx=nx-AW_OCEAN_BELT+.5f,gz=nz-AW_OCEAN_BELT+.5f;
        if(gx>=0&&gz>=0&&gx<64&&gz<64){
            /* Mast clearance is separate from depth below the waterline. */
            float roof=1.44f+(variant==0?.8f:variant==1?1.7f:2.4f);
            for(float q=1.6f;q<=roof;q+=.2f)if(aw_density(m,gx,q,gz)>0)return 0;
        }
    }return 1;
}
static int aw_patrol_naval(const AwMap*m,AwPatrol*p,uint32_t salt){
    uint8_t allowed[AW_OCEAN_CELLS];int prev[AW_OCEAN_CELLS],queue[AW_OCEAN_CELLS],depth[AW_OCEAN_CELLS]={0};
    int start=-1;uint32_t score=0;
    for(int c=0;c<AW_OCEAN_CELLS;c++){
        allowed[c]=aw_patrol_water_cell(m,c,p->variant);prev[c]=-1;
        int x=c%96-16,z=c/96-16;
        if(!allowed[c]||x< -7||z< -7||x>70||z>70)continue;
        uint32_t key=aw_hash(salt^(uint32_t)c);if(start<0||key>score){start=c;score=key;}
    }
    if(start<0)return 0;int head=0,tail=0,best=start,best_score=0;queue[tail++]=start;prev[start]=start;
    while(head<tail){int c=queue[head++],value=depth[c]*3+aw_hash(salt^(uint32_t)c*71u)%90;
        if(value>best_score){best_score=value;best=c;}
        for(int d=0;d<4;d++){int n=aw_ocean_neighbor(c,d);if(n<0||!allowed[n]||prev[n]>=0)continue;prev[n]=c;depth[n]=depth[c]+1;queue[tail++]=n;}
    }
    AwPatrolGraph graph={m,AW_PATROL_NAVAL,p->variant,allowed};
    p->count=aw_patrol_search(&graph,start,best,p->route);
    return p->count>1;
}
static float aw_patrol_roof(const AwMap*m,float x,float z){
    float top=fmaxf(1.44f,aw_height_q(m,x,z));int c=aw_clamp((int)z,0,63)*64+aw_clamp((int)x,0,63);
    if(m->bridge_bins[c])top=fmaxf(top,m->bridges[m->bridge_bins[c]-1].crown+2);
    return top;
}
static int aw_patrol_volume(const AwMap*m,AwPatrol*p,uint32_t salt){
    AwVolumeGraph g={0};AwAStar scratch={0};int submarine=p->layer==AW_PATROL_SUB;
    if(!aw_volume_init(&g,m,submarine,p->variant)||!aw_astar_init(&scratch,g.nodes)){free(g.allowed);aw_astar_close(&scratch);return 0;}
    int path[AW_NODES],first=-1,previous=-1;p->count=0;
    /* Seeded perimeter goals make a circuit. Search can change depth/altitude
     * and route around terrain; endpoints never dictate a straight segment. */
    for(int i=0;i<=4;i++){
        float angle=((salt%6283)*.001f)+i*1.570796327f;
        float radius=submarine?43:21;AwRoutePoint target={32+cosf(angle)*radius,submarine?-3.0f-2*p->variant:24+8*p->variant,32+sinf(angle)*radius};
        int next=i==4?first:aw_volume_nearest(&g,target);if(next<0)break;
        if(previous<0){first=previous=next;continue;}
        int n=aw_astar_path(&scratch,g.nodes,previous,next,&g,aw_volume_edges,aw_volume_estimate,path,AW_NODES-p->count);
        if(n<2)break;
        for(int j=i==1?0:1;j<n;j++){
            AwRoutePoint v=aw_volume_position(&g,path[j]);int k=p->count++;
            p->route[k]=submarine?((int)floorf(v.z)+16)*96+(int)floorf(v.x)+16:(int)v.z*64+(int)v.x;
            p->altitude[k]=v.q;
        }previous=next;
    }
    free(g.allowed);aw_astar_close(&scratch);if(previous!=first)p->count=0;return p->count>2;
}
static int aw_patrol_build(AwMap*m,AwPatrols*f){
    memset(f,0,sizeof(*f));m->density_cache=calloc(AW_DENSITY_CACHE,sizeof(AwDensitySample));
    for(int i=0;i<AW_PATROLS;i++){
        AwPatrol*p=&f->units[i];p->layer=i<2?AW_PATROL_GROUND:i<5?AW_PATROL_NAVAL:i<8?AW_PATROL_AIR:AW_PATROL_SUB;
        p->variant=i<2?i+1:i<5?i-2:i<8?i-5:i-8;
        /* Route segments/second: the quadrotor deliberately cruises at a
         * third of the transport's pace, about a quarter of the gunship's. */
        static const float air_speed[3]={.65f,2.55f,1.9f};
        p->speed=i<2?(i?.65f:1.1f):i<5?(1.6f-p->variant*.35f):i<8?air_speed[p->variant]:.8f-p->variant*.15f;
        uint32_t salt=aw_hash(m->seed^(uint32_t)(i+1)*7193u);
        int ok=p->layer==AW_PATROL_GROUND?aw_patrol_ground(m,p,salt):p->layer==AW_PATROL_NAVAL?aw_patrol_naval(m,p,salt):aw_patrol_volume(m,p,salt);
        if(ok){f->count++;p->progress=(salt%1000)/1000.0f*(p->count-1);}
    }
    free(m->density_cache);m->density_cache=NULL;return f->count;
}
static AwPatrolPoint aw_patrol_position(const AwMap*m,const AwPatrol*p,float progress){
    if(p->count<2)return (AwPatrolPoint){0,0,0};
    float cycle=fmodf(progress,(p->count-1)*2.0f),step=cycle>p->count-1?(p->count-1)*2-cycle:cycle;
    if(p->layer==AW_PATROL_AIR||p->layer==AW_PATROL_SUB)step=fmodf(progress,(float)(p->count-1));
    int i=aw_clamp((int)step,0,p->count-2),a=p->route[i],b=p->route[i+1];float t=step-i,x,z,q;
    if(p->layer==AW_PATROL_NAVAL||p->layer==AW_PATROL_SUB){x=aw_lerp(a%96,b%96,t)-16+.5f;z=aw_lerp(a/96,b/96,t)-16+.5f;q=p->layer==AW_PATROL_SUB?aw_lerp(p->altitude[i],p->altitude[i+1],t):1.44f;}
    else if(p->layer==AW_PATROL_AIR){x=aw_lerp(a%64,b%64,t)+.5f;z=aw_lerp(a/64,b/64,t)+.5f;q=aw_lerp(p->altitude[i],p->altitude[i+1],t);}
    else {int ca=aw_node_cell(m,a),cb=aw_node_cell(m,b);x=aw_lerp(ca%64,cb%64,t)+.5f;z=aw_lerp(ca/64,cb/64,t)+.5f;q=aw_support_q(m,x,z,aw_lerp(aw_patrol_floor(m,a),aw_patrol_floor(m,b),t));}
    return (AwPatrolPoint){x,q,z};
}
#endif
