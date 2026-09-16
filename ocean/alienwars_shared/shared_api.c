#include "shared_api.h"
#include "../alienwars/missions.h"
#include "../alienwars/local_navigation.h"
#include <time.h>

#define AW_SHARED_SCENARIOS 3
typedef struct {
    AwMap map;
    AwMissionRoute route[AW_SHARED_SCENARIOS][AW_SHARED_AGENTS];
    int available[AW_SHARED_SCENARIOS];
} AwSharedMap;
typedef struct AwSharedBank {
    int maps,curriculum,references;unsigned seed;
    AwSharedMap*worlds;struct AwSharedBank*next;
} AwSharedBank;
struct AwSharedTask {
    AwSharedBank*bank;AwSharedMap*map;AwMissionWorld world;
    unsigned rng,action_rng;int scenario,reset_pending;
    int event[AW_SHARED_AGENTS],terminal[AW_SHARED_AGENTS];
    float actions[AW_SENSOR_UNITS][4];
};
static AwSharedBank*banks;
static unsigned aw_shared_rng(unsigned*s){unsigned v=*s?*s:173;v^=v<<13;v^=v>>17;v^=v<<5;return *s=v;}
int aw_shared_family(int unit){return unit<3?0:unit<6?1:unit==6?2:unit<9?3:4;}
static int aw_shared_variant(int unit){return unit<6?unit%3:unit<9?unit-6:unit-9;}
static int aw_shared_index(const AwLocalRoute*r,float distance){
    int i=0;while(i<r->count-1&&r->distance[i]<distance)i++;return i;
}
static int aw_shared_prepare(AwSharedMap*w,AwMissionPlanner*p,int curriculum){
    AwPatrols*patrol=calloc(1,sizeof(*patrol));AwLocalRoute*base=calloc(AW_SHARED_AGENTS,sizeof(*base));
    if(!patrol||!base){free(patrol);free(base);return 0;}
    aw_patrol_build(&w->map,patrol);AwPatrol scout={0};
    if(aw_patrol_scout(&w->map,&scout,w->map.spawns[0],w->map.spawns[1]))aw_local_route(&w->map,&scout,&base[0]);
    for(int i=1;i<AW_SHARED_AGENTS;i++)aw_local_route(&w->map,&patrol->units[i-1],&base[i]);
    w->map.density_cache=calloc(AW_DENSITY_CACHE,sizeof(AwDensitySample));
    if(!aw_mission_planner_init(p,&w->map)){free(w->map.density_cache);w->map.density_cache=NULL;free(patrol);free(base);return 0;}
    for(int scenario=0;scenario<AW_SHARED_SCENARIOS;scenario++)for(int unit=0;unit<AW_SHARED_AGENTS;unit++){
        int family=aw_shared_family(unit),variant=aw_shared_variant(unit),source=unit;
        /* Shared corridors deliberately create encounters among different hulls.
         * The first curriculum spreads starts and keeps missions short. */
        if(family==0)source=2;else if(family==1)source=5;else if(family==4)source=11;
        const AwLocalRoute*r=&base[source];if(r->count<3)r=&base[unit];if(r->count<3)continue;
        float length=r->distance[r->count-1];
        float fraction=curriculum==0?.12f+.24f*variant:.18f+.29f*variant;
        fraction=fminf(.88f,fraction+scenario*.035f);
        int first=aw_shared_index(r,length*fraction),reverse=curriculum>0&&variant==1;
        float goal_distance=curriculum==0?12:curriculum==1?48:length;
        if(family==AW_VEHICLE_WING)goal_distance=curriculum==0?35:curriculum==1?65:length*.45f;
        int last=aw_shared_index(r,aw_motion_clamp(r->distance[first]+(reverse?-goal_distance:goal_distance),0,length));
        if(first==last)continue;
        AwSVec delta=aw_sv_add(r->point[first+(reverse?-1:1)],aw_sv_scale(r->point[first],-1));
        AwVehicle vehicle={.family=family,.variant=variant,.position=r->point[first],.yaw=atan2f(delta.x,delta.z)};
        if(family==AW_VEHICLE_WING){float speed=aw_vehicle_spec(family,variant).reverse;vehicle.velocity=(AwSVec){sinf(vehicle.yaw)*speed,0,cosf(vehicle.yaw)*speed};}
        AwMissionRoute*out=&w->route[scenario][unit];
        int planned=aw_mission_plan(p,&vehicle,r->point[last],out);
#ifdef AW_MISSION_TRACE
        fprintf(stderr,"MISSION_PLAN scenario=%d unit=%d count=%d expanded=%d\n",scenario,unit,out->count,family==3?p->expanded:p->search.expanded);
#endif
        if(!planned)continue;
        int overlap=0;for(int j=0;j<unit;j++)if(w->route[scenario][j].count>1&&aw_bodies_overlap(aw_vehicle_body(&vehicle),aw_vehicle_body(&w->route[scenario][j].start),.3f))overlap=1;
        if(overlap){out->count=0;continue;}w->available[scenario]++;
    }
    aw_mission_planner_close(p);free(w->map.density_cache);w->map.density_cache=NULL;free(patrol);free(base);return 1;
}
AwSharedTask*aw_shared_create(int maps,unsigned seed,unsigned instance,int curriculum){
    if(maps<1||maps>32||curriculum<0||curriculum>2)return NULL;
    AwSharedBank*b=banks;while(b&&(b->maps!=maps||b->seed!=seed||b->curriculum!=curriculum))b=b->next;
    if(!b){
        b=calloc(1,sizeof(*b));if(!b)return NULL;b->maps=maps;b->seed=seed;b->curriculum=curriculum;
        b->worlds=calloc(maps,sizeof(*b->worlds));AwMissionPlanner*planner=calloc(1,sizeof(*planner));
        if(!b->worlds||!planner){free(planner);free(b->worlds);free(b);return NULL;}
        clock_t timer=clock();int family_count[5]={0};
        for(int i=0;i<maps;i++){
            AwSharedMap*w=&b->worlds[i];
            if(!aw_generate_options(&w->map,seed+i,(AwOptions){i%2,1+(i*3)%10,1+(i*5)%10,i%4,1})||!aw_shared_prepare(w,planner,curriculum)){
                aw_mission_planner_close(planner);free(planner);free(b->worlds);free(b);return NULL;}
            for(int s=0;s<AW_SHARED_SCENARIOS;s++)for(int u=0;u<AW_SHARED_AGENTS;u++)family_count[aw_shared_family(u)]+=w->route[s][u].count>1;
            fprintf(stderr,"SHARED_MAP seed=%u hash=%08x available=%d,%d,%d\n",w->map.seed,w->map.hash,w->available[0],w->available[1],w->available[2]);
        }
        free(planner);for(int f=0;f<5;f++)if(!family_count[f]){free(b->worlds);free(b);return NULL;}
        fprintf(stderr,"SHARED_BANK worlds=%d curriculum=%d prep_seconds=%.3f routes=%d,%d,%d,%d,%d\n",maps,curriculum,(clock()-timer)/(double)CLOCKS_PER_SEC,family_count[0],family_count[1],family_count[2],family_count[3],family_count[4]);
        b->next=banks;banks=b;
    }
    AwSharedTask*t=calloc(1,sizeof(*t));if(!t)return NULL;t->bank=b;b->references++;t->rng=aw_hash(instance^seed^773);t->action_rng=aw_hash(instance^9817);return t;
}
void aw_shared_destroy(AwSharedTask*t){
    if(!t)return;AwSharedBank*b=t->bank;if(!--b->references){AwSharedBank**link=&banks;while(*link!=b)link=&(*link)->next;*link=b->next;free(b->worlds);free(b);}free(t);
}
void aw_shared_reset(AwSharedTask*t){
    t->map=&t->bank->worlds[aw_shared_rng(&t->rng)%t->bank->maps];t->scenario=aw_shared_rng(&t->rng)%AW_SHARED_SCENARIOS;
    memset(&t->world,0,sizeof(t->world));memset(t->event,0,sizeof(t->event));memset(t->terminal,0,sizeof(t->terminal));
    t->world.count=AW_SHARED_AGENTS;t->reset_pending=0;aw_sensors_init(&t->world.sensors,&t->map->map,AW_SHARED_AGENTS);
    for(int i=0;i<AW_SHARED_AGENTS;i++){
        const AwMissionRoute*r=&t->map->route[t->scenario][i];
        if(r->count<2){t->terminal[i]=1;continue;}
        int limit=t->bank->curriculum==0?400:t->bank->curriculum==1?1000:2400;
        aw_mission_agent_reset(&t->world.agents[i],r,limit);t->world.active[i]=1;aw_mission_equip(&t->world,i);
        /* Small equipment dropout is disclosed and visible in the observation.
         * It never changes overlay state or supplies substitute measurements. */
        if(t->bank->curriculum>0&&aw_shared_rng(&t->rng)%5==0){int type=aw_shared_rng(&t->rng)%4;AwSensorConfig c=t->world.sensors.units[i].config[type];c.enabled=0;aw_sensor_attach(&t->world.sensors,i,type,c);}
    }
    aw_mission_sense(&t->world,&t->map->map,.1f);
}
void aw_shared_action(AwSharedTask*t,int unit,const float*action){if(unit>=0&&unit<AW_SHARED_AGENTS)memcpy(t->actions[unit],action,4*sizeof(float));}
void aw_shared_step(AwSharedTask*t){
    if(t->reset_pending){aw_shared_reset(t);for(int i=0;i<AW_SHARED_AGENTS;i++)t->terminal[i]=1;return;}
    aw_mission_tick(&t->world,&t->map->map,t->actions);int running=0;
    for(int i=0;i<AW_SHARED_AGENTS;i++){
        AwMissionAgent*a=&t->world.agents[i];int done=!t->world.active[i]||a->arrived||a->timeout||a->vehicle.failed;
        t->event[i]=done&&!t->terminal[i]&&t->world.active[i];t->terminal[i]=done;running+=!done;
        /* A completed aircraft departs the episodic scenario. Ground/water
         * arrivals remain parked obstacles for the remaining participants. */
        if(done&&a->vehicle.family==AW_VEHICLE_WING)t->world.active[i]=0;
    }if(!running)t->reset_pending=1;
}
void aw_shared_read(AwSharedTask*t,int unit,float*obs,float*reward,int*terminal,int*event,int*arrived,int*contacts,int*blocked){
    AwMissionAgent*a=&t->world.agents[unit];memcpy(obs,a->observation,sizeof(a->observation));*reward=a->reward;
    *terminal=t->terminal[unit];*event=t->event[unit];*arrived=a->arrived;*contacts=a->contacts;*blocked=a->blocked_ticks;
}
void aw_shared_reference(AwSharedTask*t,int unit,float*actions,int random){
    for(int i=0;i<4;i++)actions[i]=random?(float)(aw_shared_rng(&t->action_rng)%(i?3:4)):(i?1:2);
    (void)unit;
}
void aw_shared_mask(AwSharedTask*t,int unit,unsigned char*mask){
    if(!mask)return;memset(mask,1,13);
    AwMissionAgent*a=&t->world.agents[unit];
    if(!t->world.active[unit]||a->arrived||a->timeout||a->vehicle.failed){
        memset(mask,0,13);mask[2]=mask[5]=mask[8]=mask[11]=1;
    }
}
