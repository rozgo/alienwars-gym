#include <assert.h>
#include "ocean/alienwars/missions.h"
static AwMap map;
static AwMissionPlanner planner;
static AwMissionRoute route;
static AwMissionWorld world;
int main(void){
    assert(aw_generate(&map,73));uint32_t hash=map.hash;
    assert(aw_mission_planner_init(&planner,&map));
    AwVehicle wing={.family=AW_VEHICLE_WING,.variant=1,.position={32,35,32},.velocity={0,0,5}};
    assert(aw_mission_plan(&planner,&wing,(AwSVec){96,35,96},&route));
    assert(route.count>30);
    /* The exported path must be reproducible by the actual actuator model,
     * including yaw acceleration, rather than merely pass point occupancy. */
    AwVehicle replay=wing;
    for(int i=1;i<route.count;i++){
        for(int t=0;t<3;t++)aw_vehicle_drive(&map,&replay,route.drive[i],1.0f/30,NULL,0,-1);
        assert(!replay.failed);
        assert(aw_sv_length(aw_sv_add(replay.position,aw_sv_scale(route.point[i],-1)))<1e-5f);
        assert(fabsf(aw_motion_angle(replay.yaw-route.heading[i]))<1e-5f);
    }
    int wing_points=route.count;
    world.count=1;world.active[0]=1;aw_sensors_init(&world.sensors,&map,1);
    aw_mission_agent_reset(&world.agents[0],&route,1200);aw_mission_equip(&world,0);aw_mission_sense(&world,&map,.1f);
    float actions[16][4]={{2,1,1,1}};
    while(!world.agents[0].arrived&&!world.agents[0].timeout&&!world.agents[0].vehicle.failed)aw_mission_tick(&world,&map,actions);
    assert(world.agents[0].arrived);
    for(int i=0;i<AW_MISSION_INPUTS;i++)assert(isfinite(world.agents[0].observation[i]));
    assert(world.agents[0].observation[32+13]==1);
    AwSensorConfig config=world.sensors.units[0].config[AW_SENSOR_LIDAR];config.enabled=0;
    assert(aw_sensor_attach(&world.sensors,0,AW_SENSOR_LIDAR,config));aw_mission_sense(&world,&map,.1f);
    for(int i=0;i<AW_SENSOR_SLOT_OBS;i++)assert(world.agents[0].observation[32+13+i]==0);
    assert(!aw_mission_plan(&planner,&wing,(AwSVec){64,-20,64},&route));
    assert(!aw_mission_plan(&planner,&wing,(AwSVec){NAN,0,0},&route));
    AwVehicle a[2]={{.family=AW_VEHICLE_QUAD,.position={64,45,64},.velocity={2,0,0}},
                    {.family=AW_VEHICLE_QUAD,.position={65.95f,45,64},.velocity={-2,0,0}}};
    AwVehicle b[2]={a[1],a[0]};AwDrive da[2]={{0,0,0,2},{0,0,0,-2}},db[2]={da[1],da[0]};unsigned char active[2]={1,1};
    aw_vehicles_step(&map,a,da,active,2,1.0f/30);aw_vehicles_step(&map,b,db,active,2,1.0f/30);
    assert(a[0].contact&&a[1].contact);
    assert(memcmp(&a[0],&b[1],sizeof(AwVehicle))==0&&memcmp(&a[1],&b[0],sizeof(AwVehicle))==0);
    assert(!aw_bodies_overlap(aw_vehicle_body(&a[0]),aw_vehicle_body(&a[1]),0));
    /* Projection past a segment end must not stop beside the goal. */
    AwMissionRoute finish={.count=2,.point={{0,-5,0},{0,-5,10}},.distance={0,10}};
    AwMissionAgent near={.route=&finish,.cursor=1,.along=10,.remaining=0,.vehicle={.family=AW_VEHICLE_SUB,.position={2,-5,10},.yaw=-AW_MOTION_PI/2}};
    AwDrive correction=aw_mission_control(&near,actions[0]);assert(correction.speed>0);
    assert(map.hash==hash);aw_mission_planner_close(&planner);
    printf("MISSION_ROUTE_TEST wing_replay=PASS wing_tracker=PASS points=%d sensor_equipment=PASS synchronous_collision=PASS invalid_goal=PASS map_unchanged=PASS\n",wing_points);
}
