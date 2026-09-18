#ifndef ALIENWARS_COMMAND_FLEET_H
#define ALIENWARS_COMMAND_FLEET_H
#ifndef AW_NAV_VERSION
#define AW_NAV_VERSION 2 /* Public checkpoint manifest; release build overrides explicitly. */
#endif
#include "missions.h"
#include "local_navigation.h"
#include "../../src/puffercpu.c"
enum {AW_MISSION_TRAVELLING,AW_MISSION_ARRIVED,AW_MISSION_UNAVAILABLE,AW_MISSION_IMPACT,AW_MISSION_STALLED};
typedef struct {
    AwMissionWorld world;
    AwMissionAgent*unit;
    unsigned char*active;
    AwMissionRoute route[AW_UNITS],candidate;
    AwMissionPlanner planner;
    AwLocalRoute scratch;
    AwVehicle previous[AW_UNITS];
    AwSVec home[AW_UNITS],destination[AW_UNITS];
    PufferNet*net[AW_UNITS];Weights*weights[AW_VEHICLE_FAMILIES];
    float accumulator,terminal[AW_UNITS];
    int ready,trained,arrivals[AW_UNITS],automatic[AW_UNITS],status[AW_UNITS],arrival_handled[AW_UNITS];
    int selection,command_result,command_requested;
    int recovery_attempts[AW_UNITS],recovery_after[AW_UNITS];
    int total_terrain[AW_UNITS],total_units[AW_UNITS];
    int total_contacts[AW_UNITS],total_blocked[AW_UNITS],total_collisions[AW_UNITS];
} AwCommandFleet;
static void aw_command_fleet_close(AwCommandFleet*f){
    for(int i=0;i<AW_UNITS;i++)if(f->net[i])free_puffernet(f->net[i]);
    for(int i=0;i<AW_VEHICLE_FAMILIES;i++)free(f->weights[i]);
    aw_mission_planner_close(&f->planner);aw_mission_world_close(&f->world);memset(f,0,sizeof(*f));
}
static int aw_command_fleet_destination(AwCommandFleet*f,const AwMap*m,int i,AwSVec goal,int automatic){
    if(i<0||i>=AW_UNITS||!f->active[i]||f->unit[i].vehicle.failed)return 0;
    AwVehicle vehicle=f->unit[i].vehicle;
    if(!aw_mission_plan(&f->planner,&vehicle,goal,&f->candidate)){
        if(automatic)f->status[i]=AW_MISSION_UNAVAILABLE;return 0;
    }
    f->route[i]=f->candidate;
    aw_mission_agent_reset(&f->unit[i],&f->route[i],6000);
    f->unit[i].vehicle=vehicle; /* new command preserves momentum and pose */
    f->unit[i].odometry_origin=f->world.sensors.units[i].odometry.distance;
    f->unit[i].previous_potential=aw_mission_project(&f->unit[i]);
    if(!automatic)f->home[i]=vehicle.position;
    f->destination[i]=f->route[i].point[f->route[i].count-1];f->automatic[i]=automatic;f->terminal[i]=1;f->status[i]=AW_MISSION_TRAVELLING;f->arrival_handled[i]=0;
    f->recovery_attempts[i]=0;f->recovery_after[i]=0;
    aw_mission_observe(&f->world);(void)m;return 1;
}
static void aw_command_fleet_scout_route(AwCommandFleet*f,const AwMap*m,int start){
    AwPatrol patrol={.layer=0,.variant=0,.count=m->path_length};memcpy(patrol.route,m->path,patrol.count*sizeof(int));
    aw_local_route(m,&patrol,&f->scratch);const AwLocalRoute*r=&f->scratch;
    if(r->count<2){f->active[0]=0;return;}
    start=aw_clamp(start,0,r->count-2);AwSVec delta=aw_sv_add(r->point[start+1],aw_sv_scale(r->point[start],-1));
    AwVehicle vehicle={.family=0,.variant=0,.position=r->point[start],.yaw=atan2f(delta.x,delta.z)};
    if(!aw_mission_plan(&f->planner,&vehicle,r->point[r->count-1],&f->route[0])){f->active[0]=0;return;}
    f->active[0]=1;aw_mission_agent_reset(&f->unit[0],&f->route[0],6000);
    if(f->weights[0]&&!f->net[0]){int sizes[]={4,3,3,3};f->weights[0]->idx=0;f->net[0]=make_puffernet(f->weights[0],1,AW_MISSION_INPUTS,128,2,sizes,4);}
    f->status[0]=AW_MISSION_TRAVELLING;f->arrival_handled[0]=0;
    f->previous[0]=vehicle;f->home[0]=vehicle.position;f->destination[0]=r->point[r->count-1];f->automatic[0]=1;f->terminal[0]=1;
    if(f->ready){
        aw_sensor_teleport(&f->world.sensors,0);
        f->world.sensors.units[0].pose=(AwSensorPose){.position=vehicle.position,.yaw=vehicle.yaw,.pitch=vehicle.pitch};
        aw_mission_observe(&f->world);
    }
}
static void aw_command_fleet_init(AwCommandFleet*f,const AwMap*m,const AwPatrols*p){
    AwSensorConfig equipment[AW_UNITS][4];int preserve=f->ready;
    if(preserve)for(int i=0;i<AW_UNITS;i++)memcpy(equipment[i],f->world.sensors.units[i].config,sizeof(equipment[i]));
    aw_command_fleet_close(f);if(!aw_mission_world_init(&f->world))return;
    aw_mission_world_reset(&f->world,m,AW_UNITS);
    f->unit=f->world.agents;f->active=f->world.active;f->selection=1;
    if(!aw_mission_planner_init(&f->planner,m))return;
    aw_command_fleet_scout_route(f,m,0);
    for(int i=1;i<AW_UNITS;i++){
        aw_local_route(m,&p->units[i-1],&f->scratch);const AwLocalRoute*r=&f->scratch;if(r->count<2)continue;
        int start=(i*13)%(r->count-1),end=r->family==AW_VEHICLE_WING||r->family==AW_VEHICLE_QUAD||r->family==AW_VEHICLE_SUB?(start+r->count/2)%(r->count-1):r->count-1;
        if(start==end)start=0;
        AwSVec delta=aw_sv_add(r->point[start+1],aw_sv_scale(r->point[start],-1));
        AwVehicle vehicle={.family=r->family,.variant=r->variant,.position=r->point[start],.yaw=atan2f(delta.x,delta.z)};
        if(r->family==AW_VEHICLE_WING){float speed=aw_vehicle_spec(r->family,r->variant).reverse;vehicle.velocity=(AwSVec){sinf(vehicle.yaw)*speed,0,cosf(vehicle.yaw)*speed};}
        int planned=0;
        for(int attempt=0;attempt<12&&!planned;attempt++){
            if(attempt){start=(start+3)%(r->count-1);vehicle.position=r->point[start];
                delta=aw_sv_add(r->point[start+1],aw_sv_scale(vehicle.position,-1));vehicle.yaw=atan2f(delta.x,delta.z);}
            if(aw_vehicle_clear(m,&vehicle))planned=aw_mission_plan(&f->planner,&vehicle,r->point[end],&f->route[i]);
        }
        if(!planned){f->status[i]=AW_MISSION_UNAVAILABLE;continue;}
        int overlap=0;for(int j=0;j<i;j++)if(f->active[j]&&aw_bodies_overlap(aw_vehicle_body(&vehicle),aw_vehicle_body(&f->unit[j].vehicle),.2f))overlap=1;
        if(overlap){f->status[i]=AW_MISSION_UNAVAILABLE;continue;}
        aw_mission_agent_reset(&f->unit[i],&f->route[i],6000);f->active[i]=1;f->home[i]=vehicle.position;f->destination[i]=r->point[end];f->automatic[i]=1;
    }
    const char*directory=getenv("AW_MISSION_MODELS");if(!directory||!*directory)directory="resources/alienwars";
    char metadata[2048],json[1024]={0};snprintf(metadata,sizeof(metadata),"%s/contract.json",directory);
    int model_contract=2;FILE*config=fopen(metadata,"r");
    if(config){fread(json,1,sizeof(json)-1,config);fclose(config);char*key=strstr(json,"\"contract\"");char*colon=key?strchr(key,':'):NULL;model_contract=colon?atoi(colon+1):0;}
    int sizes[]={4,3,3,3};f->trained=model_contract==AW_NAV_VERSION;

    for(int family=0;family<5;family++){
        if(model_contract!=AW_NAV_VERSION)continue;
        char file[2048];snprintf(file,sizeof(file),"%s/mission-%d.bin",directory,family);Weights*w=f->weights[family]=load_weights(file);
        int expected=128*AW_MISSION_INPUTS+14*128+2*3*128*128;
        if(!w){f->trained=0;continue;}
        int valid=w->size-7==expected;for(int j=0;valid&&j<expected;j++)if(!isfinite(w->data[j]))valid=0;
        if(!valid){free(w);f->weights[family]=NULL;f->trained=0;}
    }
    for(int i=0;i<AW_UNITS;i++)if(f->active[i]){
        f->previous[i]=f->unit[i].vehicle;f->terminal[i]=1;aw_mission_equip(&f->world,i);
        if(preserve)for(int t=0;t<4;t++)aw_sensor_attach(&f->world.sensors,i,t,equipment[i][t]);
        Weights*w=f->weights[f->unit[i].vehicle.family];if(w){w->idx=0;f->net[i]=make_puffernet(w,1,AW_MISSION_INPUTS,128,2,sizes,4);}
    }
    aw_mission_sense(&f->world,m,.1f);f->ready=1;
}
static AwVehicle aw_command_fleet_pose(const AwCommandFleet*f,int i){
    AwVehicle pose=f->unit[i].vehicle;const AwVehicle*old=&f->previous[i];float t=fminf(1,fmaxf(0,f->accumulator/.1f));
    pose.position=aw_sv_add(old->position,aw_sv_scale(aw_sv_add(pose.position,aw_sv_scale(old->position,-1)),t));
    pose.yaw=aw_motion_angle(old->yaw+aw_motion_angle(pose.yaw-old->yaw)*t);pose.pitch=aw_lerp(old->pitch,pose.pitch,t);return pose;
}
/* Planning is outside the fixed simulation tick and reuses its prepared
 * scratch. A rejected user command leaves the current mission intact. */
static void aw_command_fleet_continue(AwCommandFleet*f,const AwMap*m){
    /* Retry from the current pose; never respawn a stuck unit. Reuse the
     * full planner's fixed scratch outside the 10 Hz physics tick. */
    for(int i=0;i<AW_UNITS;i++)if(f->active[i]&&!f->world.paused[i]&&!f->unit[i].vehicle.failed&&
        !f->unit[i].arrived&&f->unit[i].control_version>=3&&(f->unit[i].timeout||f->unit[i].deadlock_ticks>=100)&&
        f->world.ticks>=f->recovery_after[i]&&f->recovery_attempts[i]<3){
        int attempts=f->recovery_attempts[i]+1,after=f->world.ticks+300;
        aw_command_fleet_destination(f,m,i,f->destination[i],f->automatic[i]);
        /* This is the same mission: retain its bounded retry budget. A real
         * new user/patrol destination starts a fresh budget instead. */
        f->recovery_attempts[i]=attempts;f->recovery_after[i]=after;
    }
    for(int i=0;i<AW_UNITS;i++)if(f->active[i]&&f->unit[i].arrived&&!f->arrival_handled[i]){
        f->arrivals[i]++;f->recovery_attempts[i]=0;f->status[i]=AW_MISSION_ARRIVED;f->arrival_handled[i]=1;
        if(f->automatic[i]||f->unit[i].vehicle.family==AW_VEHICLE_WING){
            AwSVec target=f->home[i],previous=f->destination[i];
            if(aw_command_fleet_destination(f,m,i,target,1))f->home[i]=previous;
        }
    }
}
static void aw_command_fleet_step(AwCommandFleet*f,const AwMap*m,float dt,int scout_live,int others_live){
    if(!f->ready)return;
    aw_command_fleet_continue(f,m);
    f->accumulator+=dt;
    while(f->accumulator>=.1f){f->accumulator-=.1f;float actions[16][4];int contacts[AW_UNITS],blocked[AW_UNITS],collisions[AW_UNITS],terrain[AW_UNITS],units[AW_UNITS];
        for(int i=0;i<AW_UNITS;i++){
            terrain[i]=f->unit[i].terrain_contacts;units[i]=f->unit[i].unit_contacts;contacts[i]=f->unit[i].contacts;blocked[i]=f->unit[i].blocked_total;collisions[i]=f->unit[i].collision_events;
            f->previous[i]=f->unit[i].vehicle;f->world.paused[i]=!(i?others_live:scout_live);
            actions[i][0]=2;actions[i][1]=actions[i][2]=actions[i][3]=1;
            if(!f->active[i]||f->world.paused[i])continue;
            AwMissionAgent*a=&f->unit[i];
            if(f->net[i]&&!a->arrived&&!a->timeout&&!a->vehicle.failed){
                forward_puffernet(f->net[i],a->observation,actions[i],NULL,&f->terminal[i]);
                multidiscrete(f->net[i]->multidiscrete,f->net[i]->decoder->output,actions[i],1,NULL);
            }f->terminal[i]=0;
        }
        aw_mission_tick(&f->world,m,actions);
        for(int i=0;i<AW_UNITS;i++)if(f->active[i]){
            f->total_terrain[i]+=f->unit[i].terrain_contacts-terrain[i];f->total_units[i]+=f->unit[i].unit_contacts-units[i];f->total_contacts[i]+=f->unit[i].contacts-contacts[i];f->total_blocked[i]+=f->unit[i].blocked_total-blocked[i];f->total_collisions[i]+=f->unit[i].collision_events-collisions[i];
            if(f->unit[i].vehicle.failed)f->status[i]=AW_MISSION_IMPACT;
            else if(f->unit[i].timeout)f->status[i]=AW_MISSION_STALLED;
        }
    }
}
#endif
