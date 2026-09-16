#ifndef ALIENWARS_MISSIONS_H
#define ALIENWARS_MISSIONS_H
#include "mission_routes.h"
#include "sensors.h"

#define AW_MISSION_INPUTS (32+AW_SENSOR_OBS)
#define AW_MISSION_ACTIONS 4
/* Longitudinal: reverse, stop, cruise, full. Wing reverse/stop both select
 * minimum positive airspeed. Other heads bias yaw/climb/quad strafe. */
typedef struct {
    AwVehicle vehicle;
    const AwMissionRoute *route;
    int cursor,ticks,limit,arrived,timeout,contacts,blocked_ticks,blocked_total,collision_events,invalid;
    float along,remaining,reward,total,previous_potential;
    float observation[AW_MISSION_INPUTS];
} AwMissionAgent;
typedef struct {
    int count,ticks;
    AwMissionAgent agents[AW_SENSOR_UNITS];
    unsigned char active[AW_SENSOR_UNITS];
    AwSensors sensors;
} AwMissionWorld;

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
    return along-best;
}
static AwSVec aw_mission_lookahead(const AwMissionAgent*a,float length,int*index){
    const AwMissionRoute*r=a->route;float distance=a->along+length;int i=a->cursor;
    while(i<r->count-1&&r->distance[i]<distance)i++;
    float segment=r->distance[i]-r->distance[i-1],t=segment>1e-6f?(distance-r->distance[i-1])/segment:1;
    t=fminf(1,fmaxf(0,t));if(index)*index=i;
    return aw_sv_add(r->point[i-1],aw_sv_scale(aw_sv_add(r->point[i],aw_sv_scale(r->point[i-1],-1)),t));
}
static AwDrive aw_mission_control(AwMissionAgent*a,const float*actions){
    int action[4]={2,1,1,1};
    for(int i=0;i<4;i++){float v=actions[i];if(!isfinite(v)||v<0||v>(i?2:3)||floorf(v)!=v)a->invalid++;else action[i]=(int)v;}
    AwVehicle*v=&a->vehicle;AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);
    int index;float look=v->family==AW_VEHICLE_WING?6:1.1f+s.speed*.35f;
    AwSVec target=aw_mission_lookahead(a,look,&index),delta=aw_sv_add(target,aw_sv_scale(v->position,-1));
    float angle=aw_motion_angle(atan2f(delta.x,delta.z)-v->yaw);
    float turn=angle*2.0f-.35f*v->yaw_rate;
    float speed=s.speed*.8f*fmaxf(0,cosf(angle));
    float stop=sqrtf(fmaxf(0,2*s.accel*a->remaining));speed=fminf(speed,stop);
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
    drive.turn+=(action[1]-1)*s.turn*.7f;
    drive.climb+=(action[2]-1)*s.vertical*.6f;
    if(v->family==AW_VEHICLE_QUAD)drive.strafe+=(action[3]-1)*s.speed*.6f;
    return drive;
}
static void aw_mission_agent_reset(AwMissionAgent*a,const AwMissionRoute*r,int limit){
    memset(a,0,sizeof(*a));a->route=r;a->vehicle=r->start;a->cursor=1;a->limit=limit;
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
static void aw_mission_sense(AwMissionWorld*w,const AwMap*m,float dt){
    for(int i=0;i<w->count;i++){
        AwVehicle*v=&w->agents[i].vehicle;w->sensors.units[i].active=w->active[i];
        w->sensors.units[i].pose=(AwSensorPose){.position=v->position,.yaw=v->yaw,.pitch=v->pitch};
        AwBody body=aw_vehicle_body(v);w->sensors.units[i].body_center=body.position;
        w->sensors.units[i].body_extent=(AwSVec){body.width,body.height,body.length};w->sensors.units[i].body_yaw=v->yaw;
    }
    aw_sensors_step(&w->sensors,m,dt);
    for(int i=0;i<w->count;i++){
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
        memcpy(o+32,w->sensors.observations[i],AW_SENSOR_OBS*sizeof(float));
        for(int j=0;j<32;j++)o[j]=fminf(1,fmaxf(-1,o[j]));
    }
}
static void aw_mission_tick(AwMissionWorld*w,const AwMap*m,const float actions[][4]){
    AwVehicle vehicle[AW_SENSOR_UNITS];AwDrive drive[AW_SENSOR_UNITS];unsigned char moving[AW_SENSOR_UNITS];
    for(int i=0;i<w->count;i++){
        AwMissionAgent*a=&w->agents[i];vehicle[i]=a->vehicle;vehicle[i].contact=0;a->reward=0;
        moving[i]=w->active[i];drive[i]=(AwDrive){0};
        if(moving[i]&&!a->arrived&&!a->timeout&&!a->vehicle.failed)drive[i]=aw_mission_control(a,actions[i]);
    }
    for(int t=0;t<3;t++)aw_vehicles_step(m,vehicle,drive,moving,w->count,1.0f/30);
    w->ticks++;
    for(int i=0;i<w->count;i++)if(w->active[i]){
        AwMissionAgent*a=&w->agents[i];int finished=a->arrived||a->timeout||a->vehicle.failed;
        int previous_contact=a->vehicle.contact;a->vehicle=vehicle[i];if(finished)continue;
        a->ticks++;a->contacts+=a->vehicle.contact;
        a->collision_events+=a->vehicle.contact&&!previous_contact;
        float potential=aw_mission_project(a),progress=potential-a->previous_potential;a->previous_potential=potential;
        if(progress<.01f){a->blocked_ticks++;a->blocked_total++;}else a->blocked_ticks=0;
        AwSVec goal=a->route->point[a->route->count-1];float distance=aw_sv_length(aw_sv_add(goal,aw_sv_scale(a->vehicle.position,-1)));
        float tolerance=a->vehicle.family==AW_VEHICLE_WING?4:1;
        a->arrived=distance<tolerance&&a->remaining<tolerance*2;
        a->timeout=a->ticks>=a->limit&&!a->arrived;
        a->reward=.06f*progress-.002f-.3f*a->vehicle.contact+(a->arrived?3:0)-(a->vehicle.failed?3:0);
        a->total+=a->reward;
    }
    aw_mission_sense(w,m,.1f);
}
#endif
