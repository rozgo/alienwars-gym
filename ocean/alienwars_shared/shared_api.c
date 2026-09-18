#include "shared_api.h"
#include "../alienwars/missions.h"
#include "../alienwars/local_navigation.h"
#include <time.h>
#include "frozen_policy.h"

#if AW_NAV_VERSION >= 3
#define AW_SHARED_SCENARIOS 8
#else
#define AW_SHARED_SCENARIOS 3
#endif
/* corridor, head-on, overtaking, crossing, queue, tunnel approach, long trip, repeated destination */
typedef struct {
    AwMap map;
    AwMissionRoute route[AW_SHARED_SCENARIOS][AW_SENSOR_UNITS];
    int available[AW_SHARED_SCENARIOS];
    AwMissionRoute return_route[AW_SHARED_AGENTS];
} AwSharedMap;
typedef struct AwSharedBank {
    int maps,curriculum,references;unsigned seed;
    float *frozen[5];
    AwSharedMap*worlds;struct AwSharedBank*next;
} AwSharedBank;
typedef struct {int arrived,impact,timeout,ticks,contacts,events,blocked,terrain,units,stall,deadlocks,yields,replans,interventions;float length,remaining;} AwSharedOutcome;
static AwSharedOutcome aw_shared_outcome(const AwMissionAgent*a){return (AwSharedOutcome){a->arrived,a->vehicle.failed,a->timeout,a->ticks,a->contacts,a->collision_events,a->blocked_total,a->terrain_contacts,a->unit_contacts,a->max_blocked_ticks,a->deadlock_events,a->yield_ticks,a->replans,a->safety_interventions,a->route->distance[a->route->count-1],a->remaining};}
struct AwSharedTask {
    AwSharedOutcome outcome[12];
    AwSharedBank*bank;AwSharedMap*map;AwMissionWorld world;
    unsigned rng,action_rng;int scenario,reset_pending;
    int event[AW_SHARED_AGENTS],terminal[AW_SHARED_AGENTS];
    float actions[AW_SENSOR_UNITS][4];
    AwFrozenState frozen_state[4];
    float event_reward[AW_SHARED_AGENTS];
    int event_arrived[AW_SHARED_AGENTS],event_contacts[AW_SHARED_AGENTS],event_blocked[AW_SHARED_AGENTS];
    int renewed[AW_SHARED_AGENTS],pending_renew[AW_SHARED_AGENTS];
};
static AwSharedBank*banks;
static unsigned aw_shared_rng(unsigned*s){unsigned v=*s?*s:173;v^=v<<13;v^=v>>17;v^=v<<5;return *s=v;}
int aw_shared_family(int unit){return unit<3?0:unit<6?1:unit==6?2:unit<9?3:4;}
static int aw_shared_variant(int unit){return unit<6?unit%3:unit<9?unit-6:unit-9;}
static int aw_shared_index(const AwLocalRoute*r,float distance){
    int i=0;while(i<r->count-1&&r->distance[i]<distance)i++;return i;
}
static int aw_shared_prepare(AwSharedMap*w,AwMissionPlanner*p,int curriculum){
    AwPatrols*patrol=calloc(1,sizeof(*patrol));AwLocalRoute*base=calloc(AW_SHARED_AGENTS+1,sizeof(*base));
    if(!patrol||!base){free(patrol);free(base);return 0;}
    aw_patrol_build(&w->map,patrol);AwPatrol scout={0};
    if(aw_patrol_scout(&w->map,&scout,w->map.spawns[0],w->map.spawns[1]))aw_local_route(&w->map,&scout,&base[0]);
    for(int i=1;i<AW_SHARED_AGENTS;i++)aw_local_route(&w->map,&patrol->units[i-1],&base[i]);
    #if AW_NAV_VERSION >= 3
    if(w->map.cave_count&&aw_patrol_scout(&w->map,&scout,AW_CELLS+w->map.cave_entrances[0],AW_CELLS+w->map.cave_hubs[1]))aw_local_route(&w->map,&scout,&base[12]);
    #endif
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
        #if AW_NAV_VERSION >= 3
        if(scenario==2||scenario==4)fraction=.2f+.07f*variant;
        if(scenario==3)fraction=.30f+.11f*variant;

        if(scenario==5&&family==0&&base[0].count>2){r=&base[0];length=r->distance[r->count-1];fraction=.12f+.20f*variant;}
        if(scenario==5&&unit==0&&base[12].count>2){r=&base[12];length=r->distance[r->count-1];fraction=0;}
        #endif
        int first=aw_shared_index(r,length*fraction),reverse=curriculum>0&&variant==1&&(AW_NAV_VERSION<3||(scenario!=2&&scenario!=4));
        float goal_distance=curriculum==0?12:curriculum==1?48:length;
        if(family==AW_VEHICLE_WING)goal_distance=curriculum==0?35:curriculum==1?65:length*.45f;
        if(scenario==4)goal_distance=18+variant*6;
        int last=aw_shared_index(r,aw_motion_clamp(r->distance[first]+(reverse?-goal_distance:goal_distance),0,length));
        if(first==last)continue;
        AwSVec delta=aw_sv_add(r->point[first+(reverse?-1:1)],aw_sv_scale(r->point[first],-1));
        AwVehicle vehicle={.family=family,.variant=variant,.position=r->point[first],.yaw=atan2f(delta.x,delta.z)};
        if(family==AW_VEHICLE_WING){float speed=aw_vehicle_spec(family,variant).reverse;vehicle.velocity=(AwSVec){sinf(vehicle.yaw)*speed,0,cosf(vehicle.yaw)*speed};}
        AwMissionRoute*out=&w->route[scenario][unit];
        int planned=0;
        AwSVec crossing_goal=r->point[last];
        if(scenario==3&&family!=AW_VEHICLE_WING&&variant==1){
            int center=aw_shared_index(r,length*.45f);AwSVec tangent=aw_sv_add(r->point[aw_clamp(center+1,0,r->count-1)],aw_sv_scale(r->point[aw_clamp(center-1,0,r->count-1)],-1));
            float norm=hypotf(tangent.x,tangent.z);
            if(norm>.01f){AwSVec side={tangent.z/norm*9,0,-tangent.x/norm*9};
                vehicle.position=aw_sv_add(r->point[center],side);crossing_goal=aw_sv_add(r->point[center],aw_sv_scale(side,-1));
                if(family==0){vehicle.position.y=aw_support_q(&w->map,vehicle.position.x*.5f,vehicle.position.z*.5f,(vehicle.position.y+1.2f)/.75f)*.75f-1.2f;crossing_goal.y=aw_support_q(&w->map,crossing_goal.x*.5f,crossing_goal.z*.5f,(crossing_goal.y+1.2f)/.75f)*.75f-1.2f;}
                vehicle.yaw=atan2f(-side.x,-side.z);
            }
        }
        /* Independent parking destinations must leave room for actual hulls.
         * A shared corridor is useful traffic; overlapping terminal poses are
         * an impossible task and must never be counted as a learning failure. */
        static const int offsets[13][2]={{0,0},{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,1},{-1,1},{1,-1},{-2,0},{2,0},{0,-2},{0,2}};
        for(int attempt=0;attempt<13&&!planned;attempt++){
            AwSVec goal=crossing_goal;goal.x+=offsets[attempt][0]*5;goal.z+=offsets[attempt][1]*5;
            if(family==AW_VEHICLE_GROUND)goal.y=aw_support_q(&w->map,goal.x*.5f,goal.z*.5f,(goal.y+1.2f)/.75f)*.75f-1.2f;
            if(!aw_mission_plan(p,&vehicle,goal,out))continue;
            planned=1;AwSVec end=out->point[out->count-1];float radius=aw_vehicle_spec(family,variant).length;
            for(int j=0;j<unit;j++)if(w->route[scenario][j].count>1){
                const AwMissionRoute*other=&w->route[scenario][j];AwSVec other_end=other->point[other->count-1];
                float clearance=radius+aw_vehicle_spec(other->family,other->variant).length+2;
                if(aw_sv_length(aw_sv_add(end,aw_sv_scale(other_end,-1)))<clearance){planned=0;break;}
            }
            if(!planned)out->count=0;
        }
#ifdef AW_MISSION_TRACE
        fprintf(stderr,"MISSION_PLAN scenario=%d unit=%d count=%d expanded=%d\n",scenario,unit,out->count,family==3?p->expanded:p->search.expanded);
#endif
        if(!planned)continue;
        int overlap=0;for(int j=0;j<unit;j++)if(w->route[scenario][j].count>1&&aw_bodies_overlap(aw_vehicle_body(&vehicle),aw_vehicle_body(&w->route[scenario][j].start),.3f))overlap=1;
        if(overlap){out->count=0;continue;}w->available[scenario]++;
    }
    for(int scenario=0;scenario<AW_SHARED_SCENARIOS;scenario++)for(int slot=0;slot<4;slot++){
        int family=(slot+scenario)%5,unit=family==0?slot%3:family==1?3+slot%3:family==2?6:family==3?7+slot%2:9+slot%3;
        const AwMissionRoute*r=&w->route[scenario][unit];if(r->count<4)continue;
        AwVehicle vehicle=r->start;int start=aw_clamp(r->count/3,1,r->count-2);vehicle.position=r->point[start];vehicle.yaw=r->heading[start+1];
        if(family==AW_VEHICLE_WING){float speed=aw_vehicle_spec(family,vehicle.variant).reverse;vehicle.velocity=(AwSVec){sinf(vehicle.yaw)*speed,0,cosf(vehicle.yaw)*speed};}
        int clear=1;for(int j=0;j<12+slot;j++)if(w->route[scenario][j].count>1&&aw_bodies_overlap(aw_vehicle_body(&vehicle),aw_vehicle_body(&w->route[scenario][j].start),.5f))clear=0;
        if(!clear)continue;
        AwSVec goal=r->point[r->count-1];goal.x+=7+slot*3;goal.z+=7;
        if(family==0)goal.y=aw_support_q(&w->map,goal.x*.5f,goal.z*.5f,(goal.y+1.2f)/.75f)*.75f-1.2f;
        for(int j=0;j<12+slot;j++)if(w->route[scenario][j].count>1){const AwMissionRoute*o=&w->route[scenario][j];
            if(aw_sv_length(aw_sv_add(goal,aw_sv_scale(o->point[o->count-1],-1)))<aw_vehicle_spec(family,vehicle.variant).length+aw_vehicle_spec(o->family,o->variant).length+2)clear=0;}
        if(clear)aw_mission_plan(p,&vehicle,goal,&w->route[scenario][12+slot]);
    }
    #if AW_NAV_VERSION >= 3
    for(int i=0;i<12;i++){
        const AwMissionRoute*r=&w->route[7][i];if(r->count<2)continue;
        AwVehicle vehicle=r->start;vehicle.position=r->point[r->count-1];vehicle.yaw=r->heading[r->count-1];
        if(vehicle.family==AW_VEHICLE_WING){float speed=aw_vehicle_spec(vehicle.family,vehicle.variant).reverse;vehicle.velocity=(AwSVec){sinf(vehicle.yaw)*speed,0,cosf(vehicle.yaw)*speed};}
        aw_mission_plan(p,&vehicle,r->point[0],&w->return_route[i]);
    }
    #endif
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
            if(!aw_generate_options(&w->map,seed+i,(AwOptions){i%2,1+(i*3)%10,1+(i*5)%10,AW_TEMPERATE+i%AW_BIOMES,1})||!aw_shared_prepare(w,planner,curriculum)){
                aw_mission_planner_close(planner);free(planner);free(b->worlds);free(b);return NULL;}
            for(int s=0;s<AW_SHARED_SCENARIOS;s++)for(int u=0;u<AW_SHARED_AGENTS;u++)family_count[aw_shared_family(u)]+=w->route[s][u].count>1;
            fprintf(stderr,"SHARED_MAP seed=%u hash=%08x available=%d,%d,%d\n",w->map.seed,w->map.hash,w->available[0],w->available[1],w->available[2]);
        }
        const char*pool=getenv("AW_SHARED_FROZEN_DIR");
        if(pool&&*pool){for(int f=0;f<5;f++){b->frozen[f]=aw_frozen_load(pool,f);if(!b->frozen[f]){fprintf(stderr,"Invalid frozen policy family %d\n",f);for(int k=0;k<5;k++)free(b->frozen[k]);free(planner);free(b->worlds);free(b);return NULL;}}}
        free(planner);for(int f=0;f<5;f++)if(!family_count[f]){free(b->worlds);free(b);return NULL;}
        fprintf(stderr,"SHARED_BANK worlds=%d curriculum=%d prep_seconds=%.3f routes=%d,%d,%d,%d,%d\n",maps,curriculum,(clock()-timer)/(double)CLOCKS_PER_SEC,family_count[0],family_count[1],family_count[2],family_count[3],family_count[4]);
        b->next=banks;banks=b;
    }
    AwSharedTask*t=calloc(1,sizeof(*t));if(!t)return NULL;
    if(!aw_mission_world_init(&t->world)){free(t);return NULL;}
    t->bank=b;b->references++;t->rng=aw_hash(instance^seed^773);t->action_rng=aw_hash(instance^9817);return t;
}
void aw_shared_destroy(AwSharedTask*t){
    if(!t)return;aw_mission_world_close(&t->world);AwSharedBank*b=t->bank;if(!--b->references){AwSharedBank**link=&banks;while(*link!=b)link=&(*link)->next;*link=b->next;for(int f=0;f<5;f++)free(b->frozen[f]);free(b->worlds);free(b);}free(t);
}
static void aw_shared_reset_at(AwSharedTask*t,int map,int scenario){
    t->map=&t->bank->worlds[map];t->scenario=scenario;
    aw_mission_world_reset(&t->world,&t->map->map,t->bank->frozen[0]?AW_SENSOR_UNITS:AW_SHARED_AGENTS);memset(t->event,0,sizeof(t->event));memset(t->terminal,0,sizeof(t->terminal));
    t->reset_pending=0;memset(t->frozen_state,0,sizeof(t->frozen_state));memset(t->renewed,0,sizeof(t->renewed));memset(t->pending_renew,0,sizeof(t->pending_renew));
    for(int i=0;i<AW_SHARED_AGENTS;i++){
        const AwMissionRoute*r=&t->map->route[t->scenario][i];
        if(r->count<2){t->terminal[i]=1;continue;}
        int limit=t->bank->curriculum==0?400:t->bank->curriculum==1?1000:2400;
        aw_mission_agent_reset(&t->world.agents[i],r,limit);t->world.active[i]=1;aw_mission_equip(&t->world,i);
        /* Small equipment dropout is disclosed and visible in the observation.
         * It never changes overlay state or supplies substitute measurements. */
        if(t->bank->curriculum>0&&aw_shared_rng(&t->rng)%5==0){int type=aw_shared_rng(&t->rng)%4;AwSensorConfig c=t->world.sensors.units[i].config[type];c.enabled=0;aw_sensor_attach(&t->world.sensors,i,type,c);}
    }
    for(int i=12;i<t->world.count;i++){
        const AwMissionRoute*r=&t->map->route[scenario][i];if(r->count<2)continue;
        aw_mission_agent_reset(&t->world.agents[i],r,2400);t->world.agents[i].control_version=2;
        t->world.active[i]=1;aw_mission_equip(&t->world,i);
    }
    aw_mission_sense(&t->world,&t->map->map,.1f);
}
void aw_shared_reset(AwSharedTask*t){
    int map=aw_shared_rng(&t->rng)%t->bank->maps,scenario=aw_shared_rng(&t->rng)%AW_SHARED_SCENARIOS;
    aw_shared_reset_at(t,map,scenario);
}
void aw_shared_action(AwSharedTask*t,int unit,const float*action){if(unit>=0&&unit<AW_SHARED_AGENTS)memcpy(t->actions[unit],action,4*sizeof(float));}
void aw_shared_step(AwSharedTask*t){
    if(t->reset_pending){aw_shared_reset(t);for(int i=0;i<AW_SHARED_AGENTS;i++)t->terminal[i]=1;return;}
    for(int i=12;i<t->world.count;i++)if(t->world.active[i]){
        AwMissionAgent*a=&t->world.agents[i];
        if(!a->arrived&&!a->timeout&&!a->vehicle.failed)aw_frozen_forward(t->bank->frozen[a->vehicle.family],&t->frozen_state[i-12],a->observation,t->actions[i]);
    }
    for(int i=0;i<12;i++)if(t->pending_renew[i]){t->terminal[i]=0;t->pending_renew[i]=0;}
    aw_mission_tick(&t->world,&t->map->map,t->actions);int running=0;
    for(int i=0;i<AW_SHARED_AGENTS;i++){
        AwMissionAgent*a=&t->world.agents[i];int done=!t->world.active[i]||a->arrived||a->timeout||a->vehicle.failed;
        t->event[i]=done&&!t->terminal[i]&&t->world.active[i];t->terminal[i]=done;running+=!done;
        if(t->event[i])t->outcome[i]=aw_shared_outcome(a);
        t->event_reward[i]=a->reward;t->event_arrived[i]=a->arrived;t->event_contacts[i]=a->contacts;t->event_blocked[i]=a->blocked_total;
        if(t->event[i]&&a->arrived&&t->scenario==7&&!t->renewed[i]&&t->map->return_route[i].count>1){
            AwVehicle vehicle=a->vehicle;int version=a->control_version,assist=a->assist_enabled;float origin=t->world.sensors.units[i].odometry.distance;
            aw_mission_agent_reset(a,&t->map->return_route[i],2400);a->vehicle=vehicle;a->control_version=version;a->assist_enabled=assist;a->odometry_origin=origin;
            a->previous_potential=aw_mission_project(a);t->renewed[i]=1;t->pending_renew[i]=1;running++;done=0;
        }
        /* A completed aircraft departs the episodic scenario. Ground/water
         * arrivals remain parked obstacles for the remaining participants. */
        if(done&&a->vehicle.family==AW_VEHICLE_WING)t->world.active[i]=0;
    }int refresh=0;for(int i=0;i<12;i++)refresh|=t->pending_renew[i];if(refresh)aw_mission_observe(&t->world);if(!running)t->reset_pending=1;
}
void aw_shared_read(AwSharedTask*t,int unit,float*obs,float*reward,int*terminal,int*event,int*arrived,int*contacts,int*blocked){
    AwMissionAgent*a=&t->world.agents[unit];memcpy(obs,a->observation,sizeof(a->observation));*reward=t->event[unit]?t->event_reward[unit]:a->reward;
    *terminal=t->terminal[unit];*event=t->event[unit];
    *arrived=t->event[unit]?t->event_arrived[unit]:a->arrived;*contacts=t->event[unit]?t->event_contacts[unit]:a->contacts;*blocked=t->event[unit]?t->event_blocked[unit]:a->blocked_total;
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
