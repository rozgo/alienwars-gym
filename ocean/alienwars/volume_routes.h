#ifndef ALIENWARS_VOLUME_ROUTES_H
#define ALIENWARS_VOLUME_ROUTES_H
#include "map.h"
#include "astar.h"
/* Distinct flight and ocean-volume graphs in shared world coordinates.
 * No submarine node can alias a land, bridge, or cave node. */
#define AW_FLIGHT_SIDE 16
#define AW_FLIGHT_LEVELS 16
#define AW_FLIGHT_NODES (AW_FLIGHT_SIDE*AW_FLIGHT_SIDE*AW_FLIGHT_LEVELS)
#define AW_SUB_LEVELS 4
#define AW_SUB_NODES (AW_OCEAN_CELLS*AW_SUB_LEVELS)
typedef struct {float x,q,z;} AwRoutePoint;
typedef struct {const AwMap*map;int submarine,variant,nodes;unsigned char*allowed;} AwVolumeGraph;
static AwRoutePoint aw_volume_position(const AwVolumeGraph*g,int n){
    if(g->submarine)return (AwRoutePoint){n%AW_OCEAN_SIZE-AW_OCEAN_BELT+.5f,-1.5f-3*(n/AW_OCEAN_CELLS),(n/AW_OCEAN_SIZE)%AW_OCEAN_SIZE-AW_OCEAN_BELT+.5f};
    return (AwRoutePoint){2+4*(n%AW_FLIGHT_SIDE),6+4*(n/(AW_FLIGHT_SIDE*AW_FLIGHT_SIDE)),2+4*((n/AW_FLIGHT_SIDE)%AW_FLIGHT_SIDE)};
}
static float aw_route_distance(AwRoutePoint a,AwRoutePoint b){float dx=2*(a.x-b.x),dz=2*(a.z-b.z),dy=.75f*(a.q-b.q);return sqrtf(dx*dx+dy*dy+dz*dz);}
static float aw_volume_roof(const AwMap*m,float x,float z){
    float top=fmaxf(1.44f,aw_height_q(m,x,z));int c=aw_clamp((int)z,0,63)*64+aw_clamp((int)x,0,63);
    if(m->bridge_bins[c])top=fmaxf(top,m->bridges[m->bridge_bins[c]-1].crown+2);return top;
}
static int aw_volume_clear(const AwVolumeGraph*g,AwRoutePoint p){
    const AwMap*m=g->map;
    if(g->submarine){
        float radius=.68f+g->variant*.22f,halfq=.9f+g->variant*.27f;
        if(p.q+halfq>=1.44f)return 0;
        for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
            float x=p.x+dx*radius,z=p.z+dz*radius;int ox=(int)floorf(x)+AW_OCEAN_BELT,oz=(int)floorf(z)+AW_OCEAN_BELT;
            if(ox<0||oz<0||ox>=AW_OCEAN_SIZE||oz>=AW_OCEAN_SIZE||!m->ocean_connected[oz*AW_OCEAN_SIZE+ox])return 0;
            if(p.q-halfq<=aw_ocean_bed_q(m,x,z)+.15f)return 0;
            if(x>=0&&z>=0&&x<64&&z<64)for(int y=-1;y<=1;y++)if(aw_density(m,x,p.q+y*halfq,z)>0)return 0;
        }return 1;
    }
    float radius=g->variant==0?.68f:g->variant==1?1.1f:1.5f;
    if(p.x-radius<0||p.z-radius<0||p.x+radius>=64||p.z+radius>=64)return 0;
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++)if(p.q<aw_volume_roof(m,p.x+dx*radius,p.z+dz*radius)+3)return 0;
    return 1;
}
static int aw_volume_segment(const AwVolumeGraph*g,AwRoutePoint a,AwRoutePoint b){
    int steps=(int)ceilf(aw_route_distance(a,b)/.4f);if(steps<1)steps=1;
    for(int i=0;i<=steps;i++){float t=(float)i/steps;if(!aw_volume_clear(g,(AwRoutePoint){aw_lerp(a.x,b.x,t),aw_lerp(a.q,b.q,t),aw_lerp(a.z,b.z,t)}))return 0;}return 1;
}
static int aw_volume_edges(void*context,int n,AwAStarEdge*out){
    AwVolumeGraph*g=context;if(!g->allowed[n])return 0;int count=0;
    int side=g->submarine?AW_OCEAN_SIZE:AW_FLIGHT_SIDE,area=side*side,levels=g->submarine?AW_SUB_LEVELS:AW_FLIGHT_LEVELS;
    int x=n%side,z=(n/side)%side,y=n/area;AwRoutePoint a=aw_volume_position(g,n);
    for(int dy=-1;dy<=1;dy++)for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
        if(!dx&&!dy&&!dz)continue;
        if(g->submarine&&abs(dx)+abs(dy)+abs(dz)!=1)continue;
        if(!g->submarine&&g->variant>0&&!dx&&!dz)continue; /* Fixed wings cannot climb in place. */
        int xx=x+dx,zz=z+dz,yy=y+dy;if(xx<0||zz<0||yy<0||xx>=side||zz>=side||yy>=levels)continue;
        int b=yy*area+zz*side+xx;if(!g->allowed[b])continue;AwRoutePoint p=aw_volume_position(g,b);
        if(!aw_volume_segment(g,a,p))continue;
        out[count++]=(AwAStarEdge){b,aw_route_distance(a,p)*(dy?1.15f:1)};
    }return count;
}
static float aw_volume_estimate(void*context,int a,int b){AwVolumeGraph*g=context;return aw_route_distance(aw_volume_position(g,a),aw_volume_position(g,b));}
static int aw_volume_init(AwVolumeGraph*g,const AwMap*m,int submarine,int variant){
    *g=(AwVolumeGraph){m,submarine,variant,submarine?AW_SUB_NODES:AW_FLIGHT_NODES,NULL};g->allowed=calloc(g->nodes,1);if(!g->allowed)return 0;
    for(int n=0;n<g->nodes;n++)g->allowed[n]=aw_volume_clear(g,aw_volume_position(g,n));return 1;
}
static int aw_volume_nearest(const AwVolumeGraph*g,AwRoutePoint p){
    int best=-1;float distance=INFINITY;
    for(int n=0;n<g->nodes;n++)if(g->allowed[n]){float d=aw_route_distance(p,aw_volume_position(g,n));if(d<distance){distance=d;best=n;}}return best;
}
#endif
