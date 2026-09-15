#ifndef ALIENWARS_NAV_CORE_H
#define ALIENWARS_NAV_CORE_H
/* Shared headless navigation task. Immutable maps and goal-distance fields are
 * prepared before rollouts; only unit state changes during an episode. */
#include "sensors.h"
#include "motion.h"
#include <assert.h>
#define AW_NAV_OBS (AW_SENSOR_OBS + 8)
#define AW_NAV_TASKS 48
#define AW_NAV_TRACE 601
#define AW_NAV_DT .1f
#define AW_NAV_GAMMA .99f
enum {AW_NAV_RUNNING, AW_NAV_SUCCESS, AW_NAV_FALL, AW_NAV_TIMEOUT};
enum {AW_NAV_SURFACE, AW_NAV_BRIDGE, AW_NAV_TUNNEL};
typedef struct {
    float x,z,q,speed;
    AwMotion motion;
    float attempted_x,attempted_z;
    int contact,failed;
} AwGroundUnit;
typedef struct {
    int start,goal,kind;
    float distance[AW_NODES];
} AwNavTask;
typedef struct {
    AwMap map;
    AwRayWorld rays;
    AwSVec positions[AW_NODES];
    int links[AW_NODES][AW_LINKS];
    unsigned char allowed[AW_NODES];
    AwNavTask tasks[AW_NAV_TASKS];
    int task_count,counts[3];
} AwNavWorld;
typedef struct {
    const AwNavWorld *world;
    const AwNavTask *task;
    AwGroundUnit unit;
    AwSensors sensors;
    float observations[AW_NAV_OBS];
    AwSVec trace[AW_NAV_TRACE];
    int trace_count,ticks,limit,result,contacts,invalid_actions;
    float reward,progress_reward,event_reward,total_reward,initial_distance,distance;
    int throttle,steer,reference_node;
} AwNavEpisode;

static uint32_t aw_nav_random(uint32_t *rng) {
    uint32_t x=*rng?*rng:0x9e3779b9u;
    x^=x<<13;x^=x>>17;x^=x<<5;return *rng=x;
}
static float aw_nav_node_q(const AwMap *m,int n) {
    return n>=AW_SPAN_START?m->spans[n-AW_SPAN_START].q:
        n>=AW_CELLS?m->cave[n-AW_CELLS].q:aw_surface_q(m,n,.5f,.5f);
}
static AwSVec aw_nav_position(const AwGroundUnit *u) {
    return (AwSVec){u->x,u->q*.75f-1.2f,u->z};
}
static int aw_nav_dry_support(const AwMap *m,float x,float q,float z){
    /* Water covers the unchanged surface bed at q=1.44. A cave floor below
     * a roof is a different story and must not be treated as a water bed. */
    float bed=aw_ocean_bed_q(m,x,z);
    return !(q<1.44f&&bed<1.44f&&fabsf(q-bed)<.5f);
}
/* Integrate a small supported ground body. At most 0.1 world units per 30 Hz
 * substep; reject wall penetration and terminate attempted unsupported travel.
 * Floor hints track the current story, never the topmost x/z surface. */
static void aw_ground_step(const AwMap *m,AwGroundUnit *u,int throttle,int steer,float dt) {
    if(u->failed||!isfinite(dt)||dt<=0||dt>1.0f/30+.00001f)return;
    float target=(throttle-1)*3.0f;
    u->speed+=aw_motion_clamp(target-u->speed,-6*dt,6*dt);
    float desired=(steer-1)*2.0f;
    u->motion.yaw_rate+=aw_motion_clamp(desired-u->motion.yaw_rate,-6*dt,6*dt);
    u->motion.yaw=aw_motion_angle(u->motion.yaw+u->motion.yaw_rate*dt);
    float dx=sinf(u->motion.yaw)*u->speed*dt,dz=cosf(u->motion.yaw)*u->speed*dt;
    float x=u->x+dx,z=u->z+dz;
    u->attempted_x=x;u->attempted_z=z;
    if(fabsf(dx)+fabsf(dz)<1e-7f)return;
    if(x<4||z<4||x>124||z>124){u->failed=1;u->speed=0;return;}
    float q=aw_support_q(m,x*.5f,z*.5f,u->q);
    if(q<u->q-1.05f){u->failed=1;u->speed=0;return;}
    if(q>u->q+.46f||!aw_nav_dry_support(m,x*.5f,q,z*.5f)||!aw_body_fits(m,x*.5f,q,z*.5f,0)){
        u->contact=1;u->speed=0;return;
    }
    float pitch=atan2f((q-u->q)*.75f,hypotf(dx,dz));
    if(u->speed<0)pitch=-pitch;
    aw_motion_axis(&u->motion.pitch,&u->motion.pitch_rate,pitch,1.95f,16.2f,13.5f,dt);
    u->x=x;u->z=z;u->q=q;
}
static int aw_nav_edge(const AwNavWorld *w,int a,int b) {
    AwSVec p=w->positions[a],q=w->positions[b];
    float qa=aw_nav_node_q(&w->map,a),qb=aw_nav_node_q(&w->map,b);
    if(fabsf(qa-qb)>1.1f)return 0;
    for(int j=1;j<8;j++){
        float t=j/8.0f,x=aw_lerp(p.x,q.x,t)*.5f,z=aw_lerp(p.z,q.z,t)*.5f,h=aw_lerp(qa,qb,t);
        float floor=aw_support_q(&w->map,x,z,h);
        if(fabsf(floor-h)>.3f||!aw_nav_dry_support(&w->map,x,floor,z)||!aw_body_fits(&w->map,x,floor,z,0))return 0;
    }
    return 1;
}
static void aw_nav_graph(AwNavWorld *w) {
    const AwMap *m=&w->map;
    for(int n=0;n<AW_NODES;n++){
        for(int d=0;d<AW_LINKS;d++)w->links[n][d]=-1;
        if(!m->walkable[n]||!m->reachable[n])continue;
        int c=aw_node_cell(m,n);float q=aw_nav_node_q(m,n);
        w->positions[n]=(AwSVec){(c%64+.5f)*2,q*.75f-1.2f,(c/64+.5f)*2};
        w->allowed[n]=aw_nav_dry_support(m,c%64+.5f,q,c/64+.5f)&&aw_body_fits(m,c%64+.5f,q,c/64+.5f,0);
    }
    for(int n=0;n<AW_NODES;n++)if(w->allowed[n])for(int d=0;d<AW_LINKS;d++){
        int b=aw_open(m,n,d);
        if(b>=0&&w->allowed[b]&&aw_nav_edge(w,n,b))w->links[n][d]=b;
    }
}
/* Indexed heap, distances in world units. Goal fields and their witness paths
 * are reward/debug data; the actor gets only the relative destination. */
static void aw_nav_distances(const AwNavWorld *w,AwNavTask *task) {
    int heap[AW_NODES],pos[AW_NODES],count=1;
    for(int n=0;n<AW_NODES;n++){task->distance[n]=1e9f;pos[n]=-1;}
    task->distance[task->goal]=0;heap[0]=task->goal;pos[task->goal]=0;
    while(count){
        int a=heap[0];pos[a]=-2;
        if(--count){
            heap[0]=heap[count];pos[heap[0]]=0;int p=0;
            while(p*2+1<count){
                int j=p*2+1;
                if(j+1<count&&task->distance[heap[j+1]]<task->distance[heap[j]])j++;
                if(task->distance[heap[p]]<=task->distance[heap[j]])break;
                int swap=heap[p];heap[p]=heap[j];heap[j]=swap;pos[heap[p]]=p;pos[heap[j]]=j;p=j;
            }
        }
        for(int d=0;d<AW_LINKS;d++){
            int b=w->links[a][d];if(b<0||pos[b]==-2)continue;
            float length=aw_sv_length(aw_sv_add(w->positions[a],aw_sv_scale(w->positions[b],-1)));
            float dist=task->distance[a]+fmaxf(.001f,length);
            if(dist>=task->distance[b])continue;
            task->distance[b]=dist;int p=pos[b];
            if(p<0){p=count++;heap[p]=b;}
            while(p){int parent=(p-1)/2;if(task->distance[heap[parent]]<=dist)break;heap[p]=heap[parent];pos[heap[p]]=p;p=parent;}
            heap[p]=b;pos[b]=p;
        }
    }
}
static int aw_nav_add_task(AwNavWorld *w,int start,int goal,int kind) {
    if(w->task_count>=AW_NAV_TASKS||start<0||goal<0||start>=AW_NODES||goal>=AW_NODES||
        !w->allowed[start]||!w->allowed[goal]||start==goal)return 0;
    AwNavTask *t=&w->tasks[w->task_count];
    t->start=start;t->goal=goal;t->kind=kind;aw_nav_distances(w,t);
    float d=t->distance[start];if(d<4||d>48)return 0;
    w->task_count++;w->counts[kind]++;return 1;
}
static int aw_nav_world_init(AwNavWorld *w,uint32_t seed) {
    memset(w,0,sizeof(*w));AwOptions options=aw_defaults();options.symmetry=0;
    if(!aw_generate_options(&w->map,seed,options))return 0;
    aw_ray_world_init(&w->rays,&w->map);aw_nav_graph(w);
    const AwMap *m=&w->map;
    for(int i=0;i<m->bridge_count;i++){
        const AwBridge *b=&m->bridges[i];
        int ca=b->z*64+b->x,cb=(b->z+b->dz*b->length)*64+b->x+b->dx*b->length;
        int a=aw_span_find(m,ca,b->qa),z=aw_span_find(m,cb,b->qb);
        if(a>=0&&z>=0){aw_nav_add_task(w,AW_SPAN_START+a,AW_SPAN_START+z,AW_NAV_BRIDGE);aw_nav_add_task(w,AW_SPAN_START+z,AW_SPAN_START+a,AW_NAV_BRIDGE);}
    }
    uint32_t rng=seed^0xa651982du;
    for(int attempt=0;attempt<256&&w->counts[AW_NAV_TUNNEL]<12;attempt++){
        if(!m->cave_count)break;
        int a=AW_CELLS+aw_nav_random(&rng)%m->cave_count,b=a;
        for(int step=0;step<5+(int)(attempt%7);step++){
            int d=aw_nav_random(&rng)%6,next=w->links[b][d];
            if(next>=AW_CELLS&&next<AW_SPAN_START)b=next;
        }
        aw_nav_add_task(w,a,b,AW_NAV_TUNNEL);
    }
    for(int attempt=0;attempt<1024&&w->task_count<AW_NAV_TASKS;attempt++){
        int a=aw_nav_random(&rng)%AW_CELLS,b=a;
        if(!w->allowed[a]||m->bridge_bins[a])continue;
        int heading=aw_nav_random(&rng)%4;
        for(int step=0;step<3+(int)(attempt%7);step++){
            if(aw_nav_random(&rng)%4==0)heading=aw_nav_random(&rng)%4;
            int next=w->links[b][heading];if(next>=0&&next<AW_CELLS)b=next;
        }
        aw_nav_add_task(w,a,b,AW_NAV_SURFACE);
    }
    return w->counts[AW_NAV_SURFACE]>0;
}
/* Find the current floor locally, including bridges and underground spans.
 * Disallow vertical jumps when projecting a pose onto reward/navigation data. */
static int aw_nav_nearest(const AwNavEpisode *e) {
    const AwNavWorld *w=e->world;const AwMap *m=&w->map;
    int cx=(int)(e->unit.x*.5f),cz=(int)(e->unit.z*.5f),best=-1;float score=1e9f;
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
        int x=cx+dx,z=cz+dz;if(x<0||x>=64||z<0||z>=64)continue;
        int c=z*64+x,n=c;
        for(;;){
            if(w->allowed[n]&&e->task->distance[n]<1e8f){
                float q=aw_nav_node_q(m,n),dq=fabsf(q-e->unit.q);
                float dist=hypotf(w->positions[n].x-e->unit.x,w->positions[n].z-e->unit.z);
                if(dq<1.1f&&dist+dq<score){best=n;score=dist+dq;}
            }
            int s=n<AW_SPAN_START?m->span_first[c]:m->spans[n-AW_SPAN_START].next;
            if(s<0)break;n=AW_SPAN_START+s;
        }
    }
    return best;
}
static float aw_nav_distance(const AwNavEpisode *e) {
    int n=aw_nav_nearest(e);if(n<0)return 64;
    const AwNavWorld *w=e->world;float best=1e9f;
    for(int d=-1;d<AW_LINKS;d++){
        int j=d<0?n:w->links[n][d];if(j<0)continue;
        float q=aw_nav_node_q(&w->map,j);if(fabsf(q-e->unit.q)>1.1f)continue;
        float local=aw_sv_length(aw_sv_add(aw_nav_position(&e->unit),aw_sv_scale(w->positions[j],-1)));
        best=fminf(best,local+e->task->distance[j]);
    }
    return fminf(64,best);
}
static void aw_nav_observe(AwNavEpisode *e) {
    memcpy(e->observations,e->sensors.observations[0],AW_SENSOR_OBS*sizeof(float));
    AwSVec p=aw_nav_position(&e->unit),g=e->world->positions[e->task->goal];
    float dx=g.x-p.x,dz=g.z-p.z,y=e->unit.motion.yaw;
    float *o=e->observations+AW_SENSOR_OBS;
    o[0]=aw_sensor_clip((dx*cosf(y)-dz*sinf(y))/32);
    o[1]=aw_sensor_clip((dx*sinf(y)+dz*cosf(y))/32);
    o[2]=aw_sensor_clip((g.y-p.y)/12);
    o[3]=fminf(1,sqrtf(dx*dx+dz*dz+(g.y-p.y)*(g.y-p.y))/48);
    o[4]=e->unit.speed/3;o[5]=e->unit.motion.yaw_rate/2;
    o[6]=e->unit.contact;o[7]=fmaxf(0,1-e->ticks/(float)e->limit);
}
static void aw_nav_sense(AwNavEpisode *e,float dt) {
    e->sensors.units[0].pose=(AwSensorPose){.position=aw_nav_position(&e->unit),
        .yaw=e->unit.motion.yaw,.pitch=e->unit.motion.pitch};
    aw_sensors_step(&e->sensors,&e->world->map,dt);
}
static void aw_nav_reset(AwNavEpisode *e,const AwNavWorld *w,int task,float yaw,int limit) {
    memset(e,0,sizeof(*e));e->world=w;e->task=&w->tasks[task];e->limit=limit;e->reference_node=e->task->start;
    AwSVec p=w->positions[e->task->start];
    e->unit=(AwGroundUnit){.x=p.x,.z=p.z,.q=aw_nav_node_q(&w->map,e->task->start),
        .motion={.yaw=yaw,.initialized=1},.attempted_x=p.x,.attempted_z=p.z};
    e->sensors.rays=w->rays;e->sensors.count=1;aw_sensor_equip(&e->sensors,0,0,.6f);
    e->sensors.units[0].config[AW_SENSOR_RF].enabled=0;
    e->sensors.units[0].config[AW_SENSOR_CAMERA].mount.pitch=-.45f;
    aw_nav_sense(e,1.0f/30);e->trace[e->trace_count++]=p;
    e->distance=e->initial_distance=aw_nav_distance(e);aw_nav_observe(e);
}
static int aw_nav_action(float value) {
    return isfinite(value)&&value>=0&&value<=2&&value==floorf(value)?(int)value:-1;
}
static void aw_nav_step(AwNavEpisode *e,float throttle,float steer) {
    if(e->result)return;
    int a=aw_nav_action(throttle),b=aw_nav_action(steer);
    if(a<0||b<0){a=b=1;e->invalid_actions++;}
    e->throttle=a;e->steer=b;e->unit.contact=0;
    float old_phi=-e->distance/64;
    for(int i=0;i<3&&!e->unit.failed;i++){
        aw_ground_step(&e->world->map,&e->unit,a,b,1.0f/30);
        aw_nav_sense(e,1.0f/30);
    }
    e->ticks++;e->contacts+=e->unit.contact;
    if(e->trace_count<AW_NAV_TRACE)e->trace[e->trace_count++]=aw_nav_position(&e->unit);
    AwSVec p=aw_nav_position(&e->unit),g=e->world->positions[e->task->goal];
    if(e->unit.failed)e->result=AW_NAV_FALL;
    else if(hypotf(g.x-p.x,g.z-p.z)<.9f&&fabsf(g.y-p.y)<.3f)e->result=AW_NAV_SUCCESS;
    else if(e->ticks>=e->limit)e->result=AW_NAV_TIMEOUT;
    e->distance=aw_nav_distance(e);
    float phi=e->result?0:-e->distance/64;
    e->progress_reward=AW_NAV_GAMMA*phi-old_phi;
    e->event_reward=e->result==AW_NAV_SUCCESS?1:e->result==AW_NAV_FALL?-1:0;
    e->reward=e->event_reward+e->progress_reward-.001f-.01f*e->unit.contact;
    e->total_reward+=e->reward;aw_nav_observe(e);
}
/* Disclosed comparison controllers: random, direct-goal, or graph reference.
 * The reference controller is diagnostic and is never fed to the actor. */
static void aw_nav_baseline(AwNavEpisode *e,int mode,uint32_t *rng,float out[2]) {
    if(mode==1){out[0]=aw_nav_random(rng)%3;out[1]=aw_nav_random(rng)%3;return;}
    AwSVec goal=e->world->positions[e->task->goal];
    if(mode==3){
        int n=e->reference_node;
        for(int k=0;k<AW_NODES;k++){
            goal=e->world->positions[n];
            if(hypotf(goal.x-e->unit.x,goal.z-e->unit.z)>.4f)break;
            int best=n;
            for(int d=0;d<AW_LINKS;d++){int j=e->world->links[n][d];if(j>=0&&e->task->distance[j]<e->task->distance[best])best=j;}
            if(best==n)break;n=best;
        }
        e->reference_node=n;goal=e->world->positions[n];
    }
    float angle=aw_motion_angle(atan2f(goal.x-e->unit.x,goal.z-e->unit.z)-e->unit.motion.yaw);
    out[0]=fabsf(angle)<(mode==3?.18f:.6f)?2:1;
    float steering=angle-.2f*e->unit.motion.yaw_rate;
    out[1]=steering<-.08f?0:steering>.08f?2:1;
}
#endif
