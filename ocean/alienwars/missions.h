#ifndef ALIENWARS_MISSIONS_H
#define ALIENWARS_MISSIONS_H
#include "mission_routes.h"
#include "sensors.h"
#include "flecs_config.h"

#ifndef AW_NAV_VERSION
#define AW_NAV_VERSION 3
#endif

#define AW_MISSION_INPUTS (32+AW_SENSOR_OBS)
#define AW_MISSION_ACTIONS 4
typedef struct {AwSVec position,velocity;double stamp;int valid,samples,source;float horizontal_uncertainty,vertical_uncertainty;} AwNavTrack;
/* Longitudinal: reverse, stop, cruise, full. Wing reverse/stop both select
 * minimum positive airspeed. Other heads bias yaw/climb/quad strafe. */
typedef struct {
    AwVehicle vehicle;
    const AwMissionRoute *route;
    int cursor,ticks,limit,arrived,timeout,contacts,blocked_ticks,blocked_total,collision_events,invalid;
    int terrain_contacts,unit_contacts,max_blocked_ticks;
    int assist_enabled,control_version,safety_interventions,recoveries,yield_ticks,recovery_ticks;
    int yield_streak,yielding,recovery_phase,avoid_peer,avoid_ticks,deadlock_ticks,max_deadlock_ticks,deadlock_events;
    float closing,clearance;
    AwNavTrack tracks[AW_SENSOR_UNITS];
    AwSVec detour[64];int detour_count,detour_cursor,replan_cooldown,replans,replan_failures;
    float along,remaining,reward,total,previous_potential,odometry_origin;
    float observation[AW_MISSION_INPUTS];
} AwMissionAgent;
typedef struct {
    int count,ticks;
    ecs_world_t *ecs;
    ecs_query_t *units;
    ecs_entity_t entities[AW_SENSOR_UNITS];
    ecs_entity_t mission_id,sensor_id,active_id,paused_id;
#ifdef AW_FLECS_EXPLORER
    ecs_http_server_t *inspection_server;
    char *inspection_reply;
#endif
    /* Borrowed views of Flecs component columns, not mirrored entity state.
     * All 16 slots are created together; topology is fixed until close. */
    AwMissionAgent *agents;
    unsigned char *active,*paused;
    AwSensorUnit *perception;
    AwSensors sensors;
} AwMissionWorld;

static ecs_entity_t aw_mission_component(ecs_world_t*ecs,const char*name,size_t size,size_t alignment){
    ecs_entity_t entity=ecs_entity_init(ecs,&(ecs_entity_desc_t){.name=name});
    return ecs_component_init(ecs,&(ecs_component_desc_t){.entity=entity,.type={.size=(ecs_size_t)size,.alignment=(ecs_size_t)alignment}});
}
#ifdef AW_FLECS_EXPLORER
#include "flecs_inspect.h"
#endif
/* Obtain typed contiguous columns through the C query API. Never infer an actor
 * slot from an ECS numeric ID or carry a pointer across a structural change. */
static void aw_mission_world_bind(AwMissionWorld*w){
    ecs_iter_t it=ecs_query_iter(w->ecs,w->units);int rows=0;
    while(ecs_query_next(&it)){
        assert(!rows&&it.count==AW_SENSOR_UNITS);
        for(int i=0;i<it.count;i++)assert(it.entities[i]==w->entities[i]);
        w->agents=ecs_field_w_size(&it,sizeof(AwMissionAgent),0);
        w->perception=ecs_field_w_size(&it,sizeof(AwSensorUnit),1);
        w->active=ecs_field_w_size(&it,sizeof(unsigned char),2);
        w->paused=ecs_field_w_size(&it,sizeof(unsigned char),3);
        rows+=it.count;
    }
    assert(rows==AW_SENSOR_UNITS);
}
static int aw_mission_world_init(AwMissionWorld*w){
    assert(!w->ecs);
#ifdef AW_FLECS_EXPLORER
    w->ecs=ecs_init();
#else
    w->ecs=ecs_mini();
#endif
    if(!w->ecs)return 0;
    w->mission_id=aw_mission_component(w->ecs,"AwMission",sizeof(AwMissionAgent),_Alignof(AwMissionAgent));
    w->sensor_id=aw_mission_component(w->ecs,"AwPerception",sizeof(AwSensorUnit),_Alignof(AwSensorUnit));
    w->active_id=aw_mission_component(w->ecs,"AwActive",sizeof(unsigned char),_Alignof(unsigned char));
    w->paused_id=aw_mission_component(w->ecs,"AwPaused",sizeof(unsigned char),_Alignof(unsigned char));
#ifdef AW_FLECS_EXPLORER
    aw_inspect_types(w);
#endif
    const ecs_entity_t*ids=ecs_bulk_init(w->ecs,&(ecs_bulk_desc_t){.count=AW_SENSOR_UNITS,
        .ids={w->mission_id,w->sensor_id,w->active_id,w->paused_id}});
    assert(ids);memcpy(w->entities,ids,sizeof(w->entities));
#ifdef AW_FLECS_EXPLORER
    aw_inspect_names(w);
#endif
    w->units=ecs_query_init(w->ecs,&(ecs_query_desc_t){.cache_kind=EcsQueryCacheNone,
        .terms={{.id=w->mission_id},{.id=w->sensor_id},{.id=w->active_id},{.id=w->paused_id}}});
    assert(w->units);aw_mission_world_bind(w);
    memset(w->agents,0,AW_SENSOR_UNITS*sizeof(*w->agents));
    memset(w->perception,0,AW_SENSOR_UNITS*sizeof(*w->perception));
    memset(w->active,0,AW_SENSOR_UNITS);memset(w->paused,0,AW_SENSOR_UNITS);
    return 1;
}
static void aw_mission_world_reset(AwMissionWorld*w,const AwMap*m,int count){
    assert(w->ecs&&count>=0&&count<=AW_SENSOR_UNITS);aw_mission_world_bind(w);
    w->count=count;w->ticks=0;
    memset(w->agents,0,AW_SENSOR_UNITS*sizeof(*w->agents));
    memset(w->perception,0,AW_SENSOR_UNITS*sizeof(*w->perception));
    memset(w->active,0,AW_SENSOR_UNITS);memset(w->paused,0,AW_SENSOR_UNITS);
    aw_sensors_init(&w->sensors,m,count,w->perception);
}
static void aw_mission_world_close(AwMissionWorld*w){
#ifdef AW_FLECS_EXPLORER
    if(w->inspection_server)ecs_rest_server_fini(w->inspection_server);
    if(w->inspection_reply)ecs_os_free(w->inspection_reply);
#endif
    if(w->units)ecs_query_fini(w->units);
    if(w->ecs)ecs_fini(w->ecs);
    memset(w,0,sizeof(*w));
}

static float aw_mission_project(AwMissionAgent*a){
    const AwMissionRoute*r=a->route;float best=INFINITY,along=a->along;int cursor=a->cursor;
    int last=aw_clamp(cursor+(a->vehicle.family==AW_VEHICLE_WING?48:12),1,r->count-1);
    for(int i=aw_clamp(cursor-3,1,r->count-1);i<=last;i++){
        AwSVec d=aw_sv_add(r->point[i],aw_sv_scale(r->point[i-1],-1)),v=aw_sv_add(a->vehicle.position,aw_sv_scale(r->point[i-1],-1));
        float length=aw_sv_dot(d,d),t=length>1e-8f?fminf(1,fmaxf(0,aw_sv_dot(v,d)/length)):0;
        float distance=aw_sv_length(aw_sv_add(v,aw_sv_scale(d,-t)));
        if(distance<best){best=distance;cursor=i;along=aw_lerp(r->distance[i-1],r->distance[i],t);}
    }
    a->cursor=cursor;a->along=along;a->remaining=r->distance[r->count-1]-along;
    return along-best*(a->control_version>=3?.3f:1);
}
static AwSVec aw_mission_lookahead(const AwMissionAgent*a,float length,int*index){
    if(a->control_version>=3&&a->detour_count){if(index)*index=a->cursor;return a->detour[a->detour_cursor];}
    const AwMissionRoute*r=a->route;float distance=a->along+length;int i=a->cursor;
    while(i<r->count-1&&r->distance[i]<distance)i++;
    float segment=r->distance[i]-r->distance[i-1],t=segment>1e-6f?(distance-r->distance[i-1])/segment:1;
    t=fminf(1,fmaxf(0,t));if(index)*index=i;
    return aw_sv_add(r->point[i-1],aw_sv_scale(aw_sv_add(r->point[i],aw_sv_scale(r->point[i-1],-1)),t));
}
static AwDrive aw_mission_control_legacy(AwMissionAgent*a,const float*actions){
    int action[4]={2,1,1,1};
    for(int i=0;i<4;i++){float v=actions[i];if(!isfinite(v)||v<0||v>(i?2:3)||floorf(v)!=v)a->invalid++;else action[i]=(int)v;}
    AwVehicle*v=&a->vehicle;AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);
    int index;float look=v->family==AW_VEHICLE_WING?6:1.1f+s.speed*.35f;
    AwSVec target=aw_mission_lookahead(a,look,&index),delta=aw_sv_add(target,aw_sv_scale(v->position,-1));
    float angle=aw_motion_angle(atan2f(delta.x,delta.z)-v->yaw);
    float turn=angle*2.0f-.35f*v->yaw_rate;
    float speed=s.speed*.8f*fmaxf(0,cosf(angle));
    float endpoint=aw_sv_length(aw_sv_add(a->route->point[a->route->count-1],aw_sv_scale(v->position,-1)));
    float stop=sqrtf(fmaxf(0,2*s.accel*fmaxf(a->remaining,endpoint)));speed=fminf(speed,stop);
    AwDrive drive={speed,turn,delta.y*1.8f-v->velocity.y*.3f,0};
    if(v->family==AW_VEHICLE_QUAD){
        float sy=sinf(v->yaw),cy=cosf(v->yaw);
        drive.speed=(delta.x*sy+delta.z*cy)*1.1f-(v->velocity.x*sy+v->velocity.z*cy)*.7f;
        drive.strafe=(delta.x*cy-delta.z*sy)*1.1f-(v->velocity.x*cy-v->velocity.z*sy)*.7f;
        drive.climb=delta.y*1.1f-v->velocity.y*.7f;
    }
    if(v->family==AW_VEHICLE_WING){
        drive.speed=s.reverse;
        /* Curvature feedforward plus cross-track correction; the primitive
         * yaw is a route property, not a privileged collision-avoidance label. */
        float heading=aw_motion_angle(a->route->heading[index]-v->yaw);
        drive.turn=a->route->drive[index].turn+heading*.65f+angle*.35f-.15f*v->yaw_rate;
    }
    if(v->family==AW_VEHICLE_WING)drive.speed=action[0]<2?s.reverse:action[0]==2?s.reverse:s.speed;
    else if(action[0]==0){drive.speed=-s.reverse;drive.strafe=0;}
    else if(action[0]==1){drive.speed=0;drive.strafe=0;}
    else if(action[0]==3){drive.speed*=1.25f;drive.strafe*=1.25f;}
    /* Neutral follows the route. Overrides give the policy enough authority
     * to leave the centreline and pass a hull, rather than converge to a
     * small offset that cannot resolve a head-on encounter. */
    if(action[1]!=1)drive.turn=(action[1]-1)*s.turn;
    if(action[2]!=1)drive.climb=(action[2]-1)*s.vertical;
    if(v->family==AW_VEHICLE_QUAD&&action[3]!=1)drive.strafe=(action[3]-1)*s.speed;
    return drive;
}
/* Contract 3 retains the four categorical heads. Steering is a bounded bias
 * around a corner-aware tracker; neutral retains the physical route baseline.
 * Contract 2 remains available for frozen traffic and historical comparisons. */
static AwDrive aw_mission_control(AwMissionAgent*a,const float*actions){
    if(a->control_version<3)return aw_mission_control_legacy(a,actions);
    float valid[4];for(int i=0;i<4;i++){
        float x=actions[i];if(!isfinite(x)||x<0||x>(i?2:3)||floorf(x)!=x){a->invalid++;x=i?1:2;}valid[i]=x;
    }
    AwVehicle*v=&a->vehicle;AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);
    if(v->family==AW_VEHICLE_WING){
        float neutral[4]={2,1,1,1};AwDrive d=aw_mission_control_legacy(a,neutral);
        d.turn+=(valid[1]-1)*s.turn*.20f;
        d.climb+=(valid[2]-1)*s.vertical*.20f;
        /* Staying near the speed used to plan the physical primitives keeps
         * their curvature attainable. Wings never stop or reverse. */
        if(valid[0]==3)d.speed=fminf(s.speed,s.reverse*1.08f);
        return d;
    }
    if(v->family==AW_VEHICLE_QUAD||v->family==AW_VEHICLE_SUB){
        float neutral[4]={valid[0],1,1,1};AwDrive d=aw_mission_control_legacy(a,neutral);
        d.turn+=(valid[1]-1)*s.turn*.35f;d.climb+=(valid[2]-1)*s.vertical*.35f;
        if(v->family==AW_VEHICLE_QUAD)d.strafe+=(valid[3]-1)*s.speed*.35f;
        return d;
    }
    float look=.7f+s.speed*.10f;
    AwSVec target=aw_mission_lookahead(a,look,NULL);
    int corner=aw_clamp(a->cursor,1,a->route->count-1);
    float distance=a->route->distance[corner]-a->along;
    if(!a->detour_count&&corner<a->route->count-1&&distance>.3f&&distance<look)target=a->route->point[corner];
    AwSVec delta=aw_sv_add(target,aw_sv_scale(v->position,-1));
    float angle=aw_motion_angle(atan2f(delta.x,delta.z)-v->yaw);
    float alignment=fmaxf(0,cosf(angle));
    AwDrive d={s.speed*.85f*alignment*alignment*alignment,angle*2.6f-v->yaw_rate*.55f,delta.y*1.8f-v->velocity.y*.3f,0};
    if(!a->detour_count&&corner<a->route->count-1){
        float bend=fabsf(aw_motion_angle(a->route->heading[corner+1]-a->route->heading[corner]));
        float corner_speed=s.speed*.85f/(1+8*bend);
        d.speed=fminf(d.speed,sqrtf(corner_speed*corner_speed+2*s.accel*fmaxf(0,distance-.6f)));
    }
    float endpoint=aw_sv_length(aw_sv_add(a->route->point[a->route->count-1],aw_sv_scale(v->position,-1)));
    d.speed=fminf(d.speed,sqrtf(fmaxf(0,2*s.accel*fmaxf(a->remaining,endpoint))));
    if(v->family==AW_VEHICLE_QUAD){
        float sy=sinf(v->yaw),cy=cosf(v->yaw);
        d.speed=(delta.x*sy+delta.z*cy)*1.1f-(v->velocity.x*sy+v->velocity.z*cy)*.7f;
        d.strafe=(delta.x*cy-delta.z*sy)*1.1f-(v->velocity.x*cy-v->velocity.z*sy)*.7f;
        d.climb=delta.y*1.1f-v->velocity.y*.7f;
    }
    if(valid[0]==0){d.speed=-s.reverse;d.strafe=0;}
    if(valid[0]==1){d.speed=0;d.strafe=0;}
    if(valid[0]==3){d.speed*=1.15f;d.strafe*=1.15f;}
    d.turn+=(valid[1]-1)*s.turn*.45f;
    d.climb+=(valid[2]-1)*s.vertical*.45f;
    if(v->family==AW_VEHICLE_QUAD)d.strafe+=(valid[3]-1)*s.speed*.45f;
    return d;
}
static void aw_mission_agent_reset(AwMissionAgent*a,const AwMissionRoute*r,int limit){
    memset(a,0,sizeof(*a));a->route=r;a->vehicle=r->start;a->cursor=1;a->limit=limit;a->control_version=AW_NAV_VERSION;a->assist_enabled=AW_NAV_VERSION>=3;
    a->previous_potential=aw_mission_project(a);
}
static int aw_mission_layer(int family){return family==AW_VEHICLE_GROUND?0:family==AW_VEHICLE_BOAT?1:family==AW_VEHICLE_SUB?3:2;}
static void aw_mission_equip(AwMissionWorld*w,int i){
    const AwVehicle*v=&w->agents[i].vehicle;AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);
    int layer=aw_mission_layer(v->family);aw_sensor_equip(&w->sensors,i,layer,hypotf(s.width,s.length));
    for(int t=0;t<4;t++){
        AwSensorConfig c=aw_sensor_default(t,layer);c.range=aw_vehicle_sensor_range(v->family,v->variant)*(t==AW_SENSOR_RF?1.5f:1);
        if(t==AW_SENSOR_CAMERA&&layer==2)c.mount.pitch=0;
        aw_sensor_attach(&w->sensors,i,t,c);
    }
}
#include "navigation_tracking.h"
static void aw_mission_observe(AwMissionWorld*w){
    for(int i=0;i<w->count;i++){
        aw_sensor_pack(&w->sensors,i);
        AwMissionAgent*a=&w->agents[i];float*o=a->observation;memset(o,0,sizeof(a->observation));if(!w->active[i])continue;
        AwVehicle*v=&a->vehicle;AwVehicleSpec spec=aw_vehicle_spec(v->family,v->variant);
        AwSVec goal=aw_sv_add(aw_mission_lookahead(a,v->family==AW_VEHICLE_WING?6:2,NULL),aw_sv_scale(v->position,-1));
        float sy=sinf(v->yaw),cy=cosf(v->yaw),norm=fmaxf(1,aw_sv_length(goal));
        o[0]=(goal.x*cy-goal.z*sy)/norm;o[1]=(goal.x*sy+goal.z*cy)/norm;o[2]=goal.y/norm;o[3]=fminf(1,norm/16);
        AwSVec next=aw_sv_add(aw_mission_lookahead(a,v->family==AW_VEHICLE_WING?20:8,NULL),aw_sv_scale(v->position,-1));
        o[4]=(next.x*cy-next.z*sy)/32;o[5]=(next.x*sy+next.z*cy)/32;o[6]=next.y/16;
        o[7]=v->yaw_rate/2;o[8]=v->pitch;o[9]=spec.width/3;o[10]=spec.length/3;o[11]=spec.speed/8;o[12]=spec.turn/2;
        o[13]=v->variant*.5f;o[14]=v->contact;o[15]=(float)(a->limit-a->ticks)/a->limit;
        for(int f=0;f<5;f++)o[16+f]=v->family==f;
        o[21]=spec.accel/6;o[22]=spec.vertical/2;o[23]=spec.reverse/8;o[24]=spec.height/3;o[25]=fminf(1,a->remaining/256);
        o[26]=fminf(1,a->blocked_ticks/100.0f);
        if(a->control_version>=3){o[27]=a->closing/8;o[28]=a->clearance/20;
            o[29]=a->yielding;o[30]=a->recovery_phase/3.0f;o[31]=a->avoid_ticks>0;}
        memcpy(o+32,w->sensors.observations[i],AW_SENSOR_OBS*sizeof(float));
        /* A recurrent mission starts a new odometer reference, as an episodic
         * training reset does. The sensor/display lifetime odometer stays intact. */
        o[32+12]=fminf(1,fmaxf(0,(w->sensors.units[i].odometry.distance-a->odometry_origin)/1024));
        for(int j=0;j<32;j++)o[j]=fminf(1,fmaxf(-1,o[j]));
    }
}
static void aw_mission_sense(AwMissionWorld*w,const AwMap*m,float dt){
    for(int i=0;i<w->count;i++){
        AwVehicle*v=&w->agents[i].vehicle;w->sensors.units[i].active=w->active[i];
        w->sensors.units[i].pose=(AwSensorPose){.position=v->position,.yaw=v->yaw,.pitch=v->pitch};
        AwBody body=aw_vehicle_body(v);w->sensors.units[i].body_center=body.position;
        w->sensors.units[i].body_extent=(AwSVec){body.width,body.height,body.length};w->sensors.units[i].body_yaw=v->yaw;
    }
    aw_sensors_step(&w->sensors,m,dt);aw_navigation_tracks(w);aw_mission_observe(w);
}
#include "navigation_recovery.h"
#include "navigation_control.h"
static void aw_mission_tick(AwMissionWorld*w,const AwMap*m,const float actions[][4]){
    aw_mission_world_bind(w);
    AwVehicle vehicle[AW_SENSOR_UNITS];AwDrive drive[AW_SENSOR_UNITS];unsigned char moving[AW_SENSOR_UNITS];int interventions[AW_SENSOR_UNITS];
    for(int i=0;i<w->count;i++){
        AwMissionAgent*a=&w->agents[i];interventions[i]=a->safety_interventions;vehicle[i]=a->vehicle;vehicle[i].contact=0;vehicle[i].contact_kind=0;a->reward=0;
        moving[i]=w->active[i];drive[i]=(AwDrive){0};
        if(moving[i]&&!w->paused[i]&&!a->arrived&&!a->timeout&&!a->vehicle.failed){
            drive[i]=aw_mission_control(a,actions[i]);
            if(a->control_version>=3&&a->assist_enabled)drive[i]=aw_navigation_control(w,m,i,drive[i]);
        }
    }
    for(int t=0;t<3;t++)aw_vehicles_step_masked(m,vehicle,drive,moving,w->paused,w->count,1.0f/30);
    w->ticks++;
    for(int i=0;i<w->count;i++)if(w->active[i]){
        AwMissionAgent*a=&w->agents[i];int finished=w->paused[i]||a->arrived||a->timeout||a->vehicle.failed;
        float moved=aw_sv_length(aw_sv_add(vehicle[i].position,aw_sv_scale(a->vehicle.position,-1)));
        int previous_contact=a->vehicle.contact;a->vehicle=vehicle[i];if(finished)continue;
        a->ticks++;a->contacts+=a->vehicle.contact;
        if(a->detour_count&&aw_sv_length(aw_sv_add(a->detour[a->detour_cursor],aw_sv_scale(a->vehicle.position,-1)))<.6f){
            if(++a->detour_cursor>=a->detour_count)a->detour_count=a->detour_cursor=0;
        }
        a->terrain_contacts+=!!(a->vehicle.contact_kind&AW_CONTACT_TERRAIN);
        a->unit_contacts+=!!(a->vehicle.contact_kind&AW_CONTACT_UNIT);
        a->collision_events+=a->vehicle.contact&&!previous_contact;
        float potential=aw_mission_project(a),progress=potential-a->previous_potential;a->previous_potential=potential;
        if(progress<.01f){a->blocked_ticks++;a->blocked_total++;}else a->blocked_ticks=0;
        if(a->blocked_ticks>a->max_blocked_ticks)a->max_blocked_ticks=a->blocked_ticks;
        a->yield_streak=a->yielding?a->yield_streak+1:0;
        if(moved<.005f&&(!a->yielding||a->yield_streak>50))a->deadlock_ticks++;else a->deadlock_ticks=0;
        if(a->deadlock_ticks>a->max_deadlock_ticks)a->max_deadlock_ticks=a->deadlock_ticks;
        if(a->deadlock_ticks==100)a->deadlock_events++;
        AwSVec goal=a->route->point[a->route->count-1];float distance=aw_sv_length(aw_sv_add(goal,aw_sv_scale(a->vehicle.position,-1)));
        float tolerance=a->vehicle.family==AW_VEHICLE_WING?4:1;
        a->arrived=distance<tolerance&&a->remaining<tolerance*2;
        a->timeout=a->ticks>=a->limit&&!a->arrived;
        a->reward=.06f*progress-.002f-.3f*a->vehicle.contact+(a->arrived?3:0)-(a->vehicle.failed?3:0);
        if(a->control_version>=3){a->reward-=.003f*(a->safety_interventions>interventions[i]);
            if(!a->yielding)a->reward-=.008f*fminf(1,fmaxf(0,(a->blocked_ticks-20)/80.0f));}
        a->total+=a->reward;
    }
    aw_mission_sense(w,m,.1f);
}
#endif
