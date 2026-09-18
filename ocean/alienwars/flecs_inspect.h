#ifndef ALIENWARS_FLECS_INSPECT_H
#define ALIENWARS_FLECS_INSPECT_H
/* Included after AwMissionWorld, only in the inspection-enabled viewer.
 * Reflection reads the actual component columns. Pointer-owned route data is
 * intentionally absent; the endpoint cannot mutate fixed-topology unit tables. */
#include <stddef.h>
#define AW_MEMBER(T,field,type_id) {.name=#field,.type=type_id,.offset=offsetof(T,field),.use_offset=true}
#define AW_ARRAY(T,field,type_id,n) {.name=#field,.type=type_id,.count=n,.offset=offsetof(T,field),.use_offset=true}
#define AW_TYPE(T,name) aw_mission_component(e,name,sizeof(T),_Alignof(T))
static void aw_inspect_types(AwMissionWorld*w){
    ecs_world_t*e=w->ecs;
    ecs_entity_t f=ecs_id(ecs_f32_t),i=ecs_id(ecs_i32_t),d=ecs_id(ecs_f64_t);
    ecs_entity_t vec=AW_TYPE(AwSVec,"AwTypes.Vector");
    ecs_struct_init(e,&(ecs_struct_desc_t){.entity=vec,.members={AW_MEMBER(AwSVec,x,f),AW_MEMBER(AwSVec,y,f),AW_MEMBER(AwSVec,z,f)}});
    ecs_entity_t pose=AW_TYPE(AwSensorPose,"AwTypes.Pose");
    ecs_struct_init(e,&(ecs_struct_desc_t){.entity=pose,.members={AW_MEMBER(AwSensorPose,position,vec),AW_MEMBER(AwSensorPose,yaw,f),AW_MEMBER(AwSensorPose,pitch,f),AW_MEMBER(AwSensorPose,roll,f)}});
    ecs_entity_t body=AW_TYPE(AwVehicle,"AwTypes.Body");
    ecs_struct_init(e,&(ecs_struct_desc_t){.entity=body,.members={
        AW_MEMBER(AwVehicle,family,i),AW_MEMBER(AwVehicle,variant,i),AW_MEMBER(AwVehicle,contact,i),AW_MEMBER(AwVehicle,failed,i),
        AW_MEMBER(AwVehicle,position,vec),AW_MEMBER(AwVehicle,velocity,vec),AW_MEMBER(AwVehicle,yaw,f),AW_MEMBER(AwVehicle,yaw_rate,f),AW_MEMBER(AwVehicle,pitch,f)}});
    ecs_entity_t odometry=AW_TYPE(AwOdometry,"AwTypes.Odometry");
    ecs_struct_init(e,&(ecs_struct_desc_t){.entity=odometry,.members={AW_MEMBER(AwOdometry,delta,vec),AW_MEMBER(AwOdometry,velocity,vec),AW_MEMBER(AwOdometry,angle_delta,vec),AW_MEMBER(AwOdometry,angular_velocity,vec),AW_MEMBER(AwOdometry,distance,f)}});
    ecs_entity_t module=AW_TYPE(AwSensorConfig,"AwTypes.SensorModule");
    ecs_struct_init(e,&(ecs_struct_desc_t){.entity=module,.members={AW_MEMBER(AwSensorConfig,enabled,i),AW_MEMBER(AwSensorConfig,range,f),AW_MEMBER(AwSensorConfig,period,f),AW_MEMBER(AwSensorConfig,hfov,f),AW_MEMBER(AwSensorConfig,vfov,f),AW_MEMBER(AwSensorConfig,mount,pose)}});
    ecs_entity_t reading=AW_TYPE(AwSensorReading,"AwTypes.SensorSample");
    ecs_struct_init(e,&(ecs_struct_desc_t){.entity=reading,.members={AW_MEMBER(AwSensorReading,valid,i),AW_MEMBER(AwSensorReading,count,i),AW_MEMBER(AwSensorReading,sequence,ecs_id(ecs_u32_t)),AW_MEMBER(AwSensorReading,stamp,d),AW_MEMBER(AwSensorReading,next,d),AW_MEMBER(AwSensorReading,pose,pose)}});
    ecs_struct_init(e,&(ecs_struct_desc_t){.entity=w->sensor_id,.members={
        AW_MEMBER(AwSensorUnit,active,i),AW_MEMBER(AwSensorUnit,layer,i),AW_MEMBER(AwSensorUnit,radius,f),AW_MEMBER(AwSensorUnit,pose,pose),
        AW_MEMBER(AwSensorUnit,body_center,vec),AW_MEMBER(AwSensorUnit,body_extent,vec),AW_MEMBER(AwSensorUnit,body_yaw,f),AW_MEMBER(AwSensorUnit,odometry,odometry),
        AW_ARRAY(AwSensorUnit,config,module,4),AW_ARRAY(AwSensorUnit,reading,reading,4)}});
    ecs_struct_init(e,&(ecs_struct_desc_t){.entity=w->mission_id,.members={
        AW_MEMBER(AwMissionAgent,vehicle,body),AW_MEMBER(AwMissionAgent,cursor,i),AW_MEMBER(AwMissionAgent,ticks,i),AW_MEMBER(AwMissionAgent,limit,i),
        AW_MEMBER(AwMissionAgent,arrived,i),AW_MEMBER(AwMissionAgent,timeout,i),AW_MEMBER(AwMissionAgent,contacts,i),AW_MEMBER(AwMissionAgent,blocked_ticks,i),
        AW_MEMBER(AwMissionAgent,collision_events,i),AW_MEMBER(AwMissionAgent,along,f),AW_MEMBER(AwMissionAgent,remaining,f),AW_MEMBER(AwMissionAgent,reward,f),AW_MEMBER(AwMissionAgent,total,f),
        AW_ARRAY(AwMissionAgent,observation,f,AW_MISSION_INPUTS)}});
    ecs_primitive_init(e,&(ecs_primitive_desc_t){.entity=w->active_id,.kind=EcsU8});
    ecs_primitive_init(e,&(ecs_primitive_desc_t){.entity=w->paused_id,.kind=EcsU8});
    ecs_doc_set_name(e,EcsWorld,"AlienWars · Live Map Lab");
    ecs_doc_set_brief(e,w->mission_id,"Live body, mission and 645 PPO inputs. Family: ground=0, boat=1, quad=2, wing=3, submarine=4. Angles in radians.");
    ecs_doc_set_brief(e,w->sensor_id,"Actual attached modules, odometry and latest sample headers. Config/sample order: LiDAR, sonar, RF, depth camera.");
}
static void aw_inspect_names(AwMissionWorld*w){
    static const char*names[16]={"Inspection_scout","Tracked_rover","Cargo_hauler","Skiff","Patrol_boat","Cutter","Quadrotor","Recon_wing","Air_transport","Recon_submarine","Patrol_submarine","Heavy_submarine","Reserved_12","Reserved_13","Reserved_14","Reserved_15"};
    ecs_entity_t parent=ecs_entity_init(w->ecs,&(ecs_entity_desc_t){.name="Fleet"});
    for(int n=0;n<AW_SENSOR_UNITS;n++){
        ecs_add_pair(w->ecs,w->entities[n],EcsChildOf,parent);
        ecs_set_name(w->ecs,w->entities[n],names[n]);
    }
}
static const char*aw_inspect_request(AwMissionWorld*w,const char*method,const char*path){
    if(!w->ecs)return "{\"error\":\"World is rebuilding\",\"status\":503}";
    if(!method||strcmp(method,"GET"))return "{\"error\":\"Live inspector is read-only; use Map Lab controls to change units\",\"status\":405}";
    if(!path||!*path||strlen(path)>8192)return "{\"error\":\"Invalid request\",\"status\":400}";
    if(!w->inspection_server)w->inspection_server=ecs_rest_server_init(w->ecs,NULL);
    if(!w->inspection_server)return "{\"error\":\"Inspector unavailable\",\"status\":503}";
    ecs_http_reply_t reply=ECS_HTTP_REPLY_INIT;
    /* Direct in-process dispatcher: no listening socket, server thread or fetch. */
    char request[8194];snprintf(request,sizeof(request),"%s%s",path[0]=='/'?"":"/",path);
    ecs_http_server_request(w->inspection_server,"GET",request,NULL,&reply);
    ecs_os_free(w->inspection_reply);w->inspection_reply=ecs_strbuf_get(&reply.body);
    ecs_strbuf_reset(&reply.headers);
    if(reply.code>=400){
        ecs_strbuf_t error=ECS_STRBUF_INIT;
        ecs_strbuf_append(&error,"{\"error\":\"Flecs request failed\",\"status\":%d,\"detail\":%s}",reply.code,w->inspection_reply?w->inspection_reply:"null");
        ecs_os_free(w->inspection_reply);w->inspection_reply=ecs_strbuf_get(&error);
    }
    if(!w->inspection_reply)w->inspection_reply=ecs_os_strdup("{\"error\":\"Empty response\",\"status\":404}");
    return w->inspection_reply;
}
#undef AW_MEMBER
#undef AW_ARRAY
#undef AW_TYPE
#endif
