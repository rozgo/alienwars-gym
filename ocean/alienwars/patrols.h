#ifndef ALIENWARS_PATROLS_H
#define ALIENWARS_PATROLS_H
#include "map.h"
/* Eight ambient patrols plus the existing inspection scout = three of each
 * ground/naval/air class. Scripted traffic, independent of the map fingerprint
 * and the inspection route; no combat, learning or unit-to-unit avoidance. */
#define AW_PATROLS 8
#define AW_AIR_POINTS 512
enum {AW_PATROL_GROUND,AW_PATROL_NAVAL,AW_PATROL_AIR};
typedef struct {
    int layer,variant,count,route[AW_NODES];float progress,speed;
    float altitude[AW_AIR_POINTS];
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
    p->count=0;for(int n=best;;n=prev[n]){p->route[p->count++]=n;if(n==start)break;}
    for(int i=0;i<p->count/2;i++){int n=p->route[i];p->route[i]=p->route[p->count-1-i];p->route[p->count-1-i]=n;}
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
    p->count=0;for(int n=best;;n=prev[n]){if(p->count>=AW_NODES)return 0;p->route[p->count++]=n;if(n==start)break;}
    for(int i=0;i<p->count/2;i++){int n=p->route[i];p->route[i]=p->route[p->count-1-i];p->route[p->count-1-i]=n;}
    return p->count>1;
}
static float aw_patrol_roof(const AwMap*m,float x,float z){
    float top=fmaxf(1.44f,aw_height_q(m,x,z));int c=aw_clamp((int)z,0,63)*64+aw_clamp((int)x,0,63);
    if(m->bridge_bins[c])top=fmaxf(top,m->bridges[m->bridge_bins[c]-1].crown+2);
    return top;
}
static int aw_patrol_air(const AwMap*m,AwPatrol*p,uint32_t salt){
    static const int dirs[8][2]={{256,0},{181,181},{0,256},{-181,181},{-256,0},{-181,-181},{0,-256},{181,-181}};
    int sites[9];
    for(int i=0;i<8;i++){
        int r=17+aw_hash(salt+i)%12,x=32+dirs[i][0]*r/256,z=32+dirs[i][1]*r/256;
        sites[i]=z*64+x;
    }sites[8]=sites[0];p->count=0;
    for(int i=0;i<8;i++){
        int x=sites[i]%64,z=sites[i]/64,gx=sites[i+1]%64,gz=sites[i+1]/64;
        while(x!=gx||z!=gz){
            if(p->count>=AW_AIR_POINTS-1)return 0;p->route[p->count++]=z*64+x;
            x+=(gx>x)-(gx<x);z+=(gz>z)-(gz<z);
        }
    }
    p->route[p->count++]=sites[0];float clearance=6+p->variant*6;
    for(int i=0;i<p->count;i++)p->altitude[i]=aw_patrol_roof(m,p->route[i]%64+.5f,p->route[i]/64+.5f)+clearance;
    for(int i=0;i<p->count-1;i++){
        float high=0;
        for(int j=0;j<=8;j++){float t=j/8.0f,x=aw_lerp(p->route[i]%64,p->route[i+1]%64,t)+.5f,z=aw_lerp(p->route[i]/64,p->route[i+1]/64,t)+.5f;
            /* Include the aircraft footprint in the terrain envelope. */
            for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++)high=fmaxf(high,aw_patrol_roof(m,x+dx*.8f,z+dz*.8f)+clearance);
        }
        p->altitude[i]=fmaxf(p->altitude[i],high);p->altitude[i+1]=fmaxf(p->altitude[i+1],high);
    }
    /* Anticipate rising ground, then descend gradually; a cyclic Lipschitz
     * envelope bounds vertical grade without lowering any required clearance. */
    for(int pass=0;pass<p->count;pass++)for(int i=0;i<p->count-1;i++){
        int j=(i+1)%(p->count-1);p->altitude[i]=fmaxf(p->altitude[i],p->altitude[j]-1.2f);p->altitude[j]=fmaxf(p->altitude[j],p->altitude[i]-1.2f);
    }
    p->altitude[p->count-1]=p->altitude[0];return 1;
}
static int aw_patrol_build(AwMap*m,AwPatrols*f){
    memset(f,0,sizeof(*f));m->density_cache=calloc(AW_DENSITY_CACHE,sizeof(AwDensitySample));
    for(int i=0;i<AW_PATROLS;i++){
        AwPatrol*p=&f->units[i];p->layer=i<2?AW_PATROL_GROUND:i<5?AW_PATROL_NAVAL:AW_PATROL_AIR;
        p->variant=i<2?i+1:i<5?i-2:i-5;
        /* Route segments/second: the quadrotor deliberately cruises at a
         * third of the transport's pace, about a quarter of the gunship's. */
        static const float air_speed[3]={.65f,2.55f,1.9f};
        p->speed=i<2?(i?.65f:1.1f):i<5?(1.6f-p->variant*.35f):air_speed[p->variant];
        uint32_t salt=aw_hash(m->seed^(uint32_t)(i+1)*7193u);
        int ok=p->layer==AW_PATROL_GROUND?aw_patrol_ground(m,p,salt):p->layer==AW_PATROL_NAVAL?aw_patrol_naval(m,p,salt):aw_patrol_air(m,p,salt);
        if(ok){f->count++;p->progress=(salt%1000)/1000.0f*(p->count-1);}
    }
    free(m->density_cache);m->density_cache=NULL;return f->count;
}
static AwPatrolPoint aw_patrol_position(const AwMap*m,const AwPatrol*p,float progress){
    if(p->count<2)return (AwPatrolPoint){0,0,0};
    float cycle=fmodf(progress,(p->count-1)*2.0f),step=cycle>p->count-1?(p->count-1)*2-cycle:cycle;
    if(p->layer==AW_PATROL_AIR)step=fmodf(progress,(float)(p->count-1));
    int i=aw_clamp((int)step,0,p->count-2),a=p->route[i],b=p->route[i+1];float t=step-i,x,z,q;
    if(p->layer==AW_PATROL_NAVAL){x=aw_lerp(a%96,b%96,t)-16+.5f;z=aw_lerp(a/96,b/96,t)-16+.5f;q=1.44f;}
    else if(p->layer==AW_PATROL_AIR){x=aw_lerp(a%64,b%64,t)+.5f;z=aw_lerp(a/64,b/64,t)+.5f;q=aw_lerp(p->altitude[i],p->altitude[i+1],t);}
    else {int ca=aw_node_cell(m,a),cb=aw_node_cell(m,b);x=aw_lerp(ca%64,cb%64,t)+.5f;z=aw_lerp(ca/64,cb/64,t)+.5f;q=aw_support_q(m,x,z,aw_lerp(aw_patrol_floor(m,a),aw_patrol_floor(m,b),t));}
    return (AwPatrolPoint){x,q,z};
}
#endif
