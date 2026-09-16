#ifndef ALIENWARS_LOCAL_NAVIGATION_H
#define ALIENWARS_LOCAL_NAVIGATION_H
#include "vehicles.h"
#define AW_LOCAL_INPUTS 96
#define AW_LOCAL_ACTIONS 4
#define AW_LOCAL_PEERS 4
#define AW_LOCAL_BEAMS 24
/* Local policy contract: physical vehicle state + A* lookahead + scheduled
 * terrain range beams and observed neighboring bodies. No optimal actions. */
typedef struct {int count,family,variant;AwSVec point[AW_NODES];float distance[AW_NODES];} AwLocalRoute;
typedef struct {
    AwVehicle vehicle;const AwLocalRoute*route;int cursor,end,ticks,limit,contacts,success,timeout,invalid;
    float reward,total,last_remaining;float observation[AW_LOCAL_INPUTS];
    AwSVec previous_peer[AW_LOCAL_PEERS];unsigned char tracked[AW_LOCAL_PEERS];
} AwLocalEpisode;
static AwSVec aw_route_vector(AwRoutePoint p){return (AwSVec){p.x*2,p.q*.75f-1.2f,p.z*2};}
static void aw_local_route(const AwMap*m,const AwPatrol*p,AwLocalRoute*r){
    memset(r,0,sizeof(*r));r->count=p->count;r->family=aw_vehicle_family(p->layer,p->variant);r->variant=p->variant;
    for(int i=0;i<p->count;i++){
        AwRoutePoint a;if(p->layer==AW_PATROL_NAVAL||p->layer==AW_PATROL_SUB)a=(AwRoutePoint){p->route[i]%96-16+.5f,p->layer==AW_PATROL_SUB?p->altitude[i]:1.44f,p->route[i]/96-16+.5f};
        else if(p->layer==AW_PATROL_AIR)a=(AwRoutePoint){p->route[i]%64+.5f,p->altitude[i],p->route[i]/64+.5f};
        else{int c=aw_node_cell(m,p->route[i]);a=(AwRoutePoint){c%64+.5f,aw_patrol_floor(m,p->route[i]),c/64+.5f};}
        r->point[i]=aw_route_vector(a);if(i)r->distance[i]=r->distance[i-1]+aw_sv_length(aw_sv_add(r->point[i],aw_sv_scale(r->point[i-1],-1)));
    }
}
static float aw_local_remaining(const AwLocalEpisode*e){
    return e->route->distance[e->end]-e->route->distance[e->cursor]+aw_sv_length(aw_sv_add(e->route->point[e->cursor],aw_sv_scale(e->vehicle.position,-1)));
}
static AwSVec aw_local_target(const AwLocalEpisode*e){return e->route->point[e->cursor];}
static float aw_body_ray(AwBody b,AwSVec origin,AwSVec direction,float limit){
    float cy=cosf(b.yaw),sy=sinf(b.yaw),dx=origin.x-b.position.x,dz=origin.z-b.position.z;
    float o[3]={dx*cy-dz*sy,origin.y-b.position.y,dx*sy+dz*cy};
    float d[3]={direction.x*cy-direction.z*sy,direction.y,direction.x*sy+direction.z*cy};
    float extent[3]={b.width,b.height,b.length},near=0,far=limit;
    for(int i=0;i<3;i++){if(fabsf(d[i])<1e-8f){if(fabsf(o[i])>extent[i])return limit;continue;}
        float a=(-extent[i]-o[i])/d[i],z=(extent[i]-o[i])/d[i];if(a>z){float t=a;a=z;z=t;}near=fmaxf(near,a);far=fminf(far,z);if(near>far)return limit;}
    return near;
}
static void aw_local_observe(AwLocalEpisode*e,const AwMap*m,const AwRayWorld*rays,const AwBody*bodies,int count,int self,float dt){
    float*o=e->observation;memset(o,0,sizeof(e->observation));AwVehicle*v=&e->vehicle;AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);
    AwSVec p=v->position,g=aw_sv_add(aw_local_target(e),aw_sv_scale(p,-1));float cy=cosf(v->yaw),sy=sinf(v->yaw),distance=aw_sv_length(g);
    float goal_scale=fmaxf(1,distance);
    o[0]=(g.x*cy-g.z*sy)/goal_scale;o[1]=(g.x*sy+g.z*cy)/goal_scale;o[2]=g.y/goal_scale;o[3]=fminf(1,distance/32);
    o[4]=(v->velocity.x*cy-v->velocity.z*sy)/8;o[5]=(v->velocity.x*sy+v->velocity.z*cy)/8;o[6]=v->velocity.y/4;o[7]=v->yaw_rate/2;
    o[8]=v->pitch;o[9]=s.width/3;o[10]=s.length/3;o[11]=s.speed/8;o[12]=s.turn/2;o[13]=v->variant*.5f;o[14]=v->contact;o[15]=(float)(e->limit-e->ticks)/e->limit;
    for(int i=0;i<AW_VEHICLE_FAMILIES;i++)o[16+i]=v->family==i;
    if(e->cursor<e->end){AwSVec next=aw_sv_add(e->route->point[e->cursor+1],aw_sv_scale(p,-1));o[21]=(next.x*cy-next.z*sy)/40;o[22]=(next.x*sy+next.z*cy)/40;o[23]=next.y/20;}
    AwBody me=aw_vehicle_body(v);AwSVec origin=me.position;if(v->family==AW_VEHICLE_GROUND)origin.y+=.25f;
    float range=aw_vehicle_sensor_range(v->family,v->variant);
    for(int i=0;i<AW_LOCAL_BEAMS;i++){
        float angle=v->yaw+(i%8)*AW_MOTION_PI*.25f,elevation=((i/8)-1)*.45f;
        AwSVec direction={sinf(angle)*cosf(elevation),sinf(elevation),cosf(angle)*cosf(elevation)};
        AwSensorHit hit=aw_ray_terrain(m,rays,origin,direction,range,v->family!=AW_VEHICLE_BOAT);
        float d=hit.distance;for(int j=0;j<count;j++)if(j!=self&&bodies[j].active)d=fminf(d,aw_body_ray(bodies[j],origin,direction,range));
        o[24+i]=d/range;
    }
    int ids[AW_LOCAL_PEERS];float closest[AW_LOCAL_PEERS];for(int k=0;k<AW_LOCAL_PEERS;k++){ids[k]=-1;closest[k]=range;}
    for(int j=0;j<count;j++)if(j!=self&&bodies[j].active){AwSVec delta=aw_sv_add(bodies[j].position,aw_sv_scale(origin,-1));float d=aw_sv_length(delta);if(d<.001f||d>=closest[AW_LOCAL_PEERS-1])continue;
        AwSensorHit hit=aw_ray_terrain(m,rays,origin,aw_sv_scale(delta,1/d),d,0);if(hit.distance<d-.2f)continue;
        for(int k=0;k<AW_LOCAL_PEERS;k++)if(d<closest[k]){for(int q=AW_LOCAL_PEERS-1;q>k;q--){closest[q]=closest[q-1];ids[q]=ids[q-1];}closest[k]=d;ids[k]=j;break;}}
    /* Ideal visible-body tracks expose relative position and velocity; this
     * is a disclosed game sensor, not noisy visual velocity estimation. Cadence is
     * fixed here at the 10 Hz decision rate, independently of visualization. */
    for(int k=0;k<AW_LOCAL_PEERS;k++)if(ids[k]>=0){const AwBody*b=&bodies[ids[k]];AwSVec d=aw_sv_add(b->position,aw_sv_scale(origin,-1)),vel=aw_sv_add(b->velocity,aw_sv_scale(v->velocity,-1));float*z=o+48+k*10;
        z[0]=1;z[1]=(d.x*cy-d.z*sy)/24;z[2]=(d.x*sy+d.z*cy)/24;z[3]=d.y/16;z[4]=(vel.x*cy-vel.z*sy)/12;z[5]=(vel.x*sy+vel.z*cy)/12;z[6]=vel.y/4;z[7]=b->width/3;z[8]=b->length/3;z[9]=b->height/3;}
    o[88]=s.accel/6;o[89]=s.vertical/2;o[90]=s.reverse/8;o[91]=s.height/3;o[92]=fminf(1,aw_local_remaining(e)/256);o[93]=range/48;
    for(int i=0;i<AW_LOCAL_INPUTS;i++)o[i]=fminf(1,fmaxf(-1,o[i]));(void)dt;
}
static void aw_local_reset(AwLocalEpisode*e,const AwLocalRoute*r,int start,int end,float heading,int limit){
    memset(e,0,sizeof(*e));e->route=r;e->cursor=aw_clamp(start+1,1,r->count-1);e->end=aw_clamp(end,e->cursor,r->count-1);e->limit=limit;
    e->vehicle=(AwVehicle){.family=r->family,.variant=r->variant,.position=r->point[start],.yaw=heading};
    if(r->family==AW_VEHICLE_WING){float speed=aw_vehicle_spec(r->family,r->variant).reverse;e->vehicle.velocity=(AwSVec){sinf(heading)*speed,0,cosf(heading)*speed};}
    e->last_remaining=aw_local_remaining(e);
}
static void aw_local_step(AwLocalEpisode*e,const AwMap*m,const float actions[AW_LOCAL_ACTIONS],const AwBody*bodies,int count,int self){
    if(e->success||e->timeout||e->vehicle.failed)return;int a[4];
    for(int i=0;i<4;i++){float x=actions[i];if(!isfinite(x)||x<0||x>2||floorf(x)!=x){a[i]=1;e->invalid++;}else a[i]=(int)x;}
    e->vehicle.contact=0;for(int i=0;i<3&&!e->vehicle.failed;i++)aw_vehicle_step(m,&e->vehicle,a,1.0f/30,bodies,count,self);
    e->ticks++;e->contacts+=e->vehicle.contact;
    float tolerance=e->vehicle.family==AW_VEHICLE_WING?(e->cursor==e->end?6:14):e->vehicle.family==AW_VEHICLE_QUAD?1.7f:1.15f;
    while(aw_sv_length(aw_sv_add(aw_local_target(e),aw_sv_scale(e->vehicle.position,-1)))<tolerance){if(e->cursor==e->end){e->success=1;break;}e->cursor++;if(e->vehicle.family==AW_VEHICLE_WING&&e->cursor==e->end)tolerance=6;}
    float remaining=aw_local_remaining(e),progress=e->last_remaining-remaining;e->last_remaining=remaining;
    e->timeout=e->ticks>=e->limit&&!e->success;
    /* Finite potential difference at gamma=1: no discount-induced reward for
     * remaining far away. Arrival is a distinct terminal event. */
    e->reward=progress*.08f-.002f-.15f*e->vehicle.contact+(e->success?2:0)-(e->vehicle.failed?2:0);
    e->total+=e->reward;
}
static void aw_local_reference(const AwLocalEpisode*e,float*out){
    AwSVec d=aw_sv_add(aw_local_target(e),aw_sv_scale(e->vehicle.position,-1));const AwVehicle*v=&e->vehicle;
    float angle=aw_motion_angle(atan2f(d.x,d.z)-v->yaw),error=angle-.30f*v->yaw_rate;
    out[0]=fabsf(angle)<.35f?2:1;out[1]=error<-.08f?0:error>.08f?2:1;out[2]=d.y<-.4f?0:d.y>.4f?2:1;out[3]=1;
    if(v->family==AW_VEHICLE_WING)out[0]=0;
    if(v->family==AW_VEHICLE_QUAD){float cy=cosf(v->yaw),sy=sinf(v->yaw),right=d.x*cy-d.z*sy,forward=d.x*sy+d.z*cy;out[0]=forward<-.6f?0:forward>.6f?2:1;out[3]=right<-.6f?0:right>.6f?2:1;}
}
#endif
