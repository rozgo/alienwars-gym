#include <assert.h>
#include "ocean/alienwars/missions.h"
static AwMap map;
static AwMissionRoute routes[2];
static AwMissionWorld world;
static void flat(int q){
    memset(&map,0,sizeof(map));
    for(int c=0;c<AW_CELLS;c++){for(int k=0;k<4;k++)map.cells[c].q[k]=q;map.cells[c].material=AW_ROAD;}
}
static void route(int i,AwSVec start,AwSVec goal){
    AwSVec d=aw_sv_add(goal,aw_sv_scale(start,-1));float yaw=atan2f(d.x,d.z);
    routes[i]=(AwMissionRoute){.count=2,.family=0,.variant=0,.point={start,goal},
        .distance={0,aw_sv_length(d)},.heading={yaw,yaw},.start={.family=0,.position=start,.yaw=yaw}};
    aw_mission_agent_reset(&world.agents[i],&routes[i],1200);world.active[i]=1;aw_mission_equip(&world,i);
}
int main(void){
    /* A graph's conservative 3x3 water neighborhood must not create invisible
     * collision walls outside the actual oriented hull. */
    flat(0);for(int i=0;i<AW_OCEAN_VERT*AW_OCEAN_VERT;i++)map.shelf_drop[i]=1200;
    map.shelf_drop[(32+AW_OCEAN_BELT)*AW_OCEAN_VERT+34+AW_OCEAN_BELT]=0;
    aw_ocean_build(&map);
    AwVehicle boat={.family=AW_VEHICLE_BOAT,.variant=1,.position={64,-.12f,64}};
    int cell=(32+16)*96+32+16;
    assert(!aw_patrol_water_cell(&map,cell,1));assert(aw_vehicle_clear(&map,&boat));
    for(int z=31;z<=33;z++)for(int x=31;x<=33;x++)map.shelf_drop[(z+16)*AW_OCEAN_VERT+x+16]=0;
    aw_ocean_build(&map);assert(!aw_vehicle_clear(&map,&boat));

    flat(4);assert(aw_mission_world_init(&world));aw_mission_world_reset(&world,&map,2);
    route(0,(AwSVec){64,1.8f,40},(AwSVec){64,1.8f,78});
    route(1,(AwSVec){64,1.8f,78},(AwSVec){64,1.8f,40});
    aw_mission_sense(&world,&map,.1f);
    float actions[16][4]={{2,1,1,1},{2,1,1,1}};
    int ticks=0;
    for(;ticks<1200&&!(world.agents[0].arrived&&world.agents[1].arrived);ticks++){
        aw_mission_tick(&world,&map,actions);
        assert(!aw_bodies_overlap(aw_vehicle_body(&world.agents[0].vehicle),aw_vehicle_body(&world.agents[1].vehicle),0));
    }
    fprintf(stderr,"HEADON ticks=%d arrivals=%d,%d contacts=%d,%d remain=%.2f,%.2f\n",ticks,world.agents[0].arrived,world.agents[1].arrived,world.agents[0].contacts,world.agents[1].contacts,world.agents[0].remaining,world.agents[1].remaining);
    assert(world.agents[0].arrived&&world.agents[1].arrived);
    assert(!world.agents[0].contacts&&!world.agents[1].contacts);
    aw_mission_world_reset(&world,&map,2);
    route(0,(AwSVec){64,1.8f,59},(AwSVec){64,1.8f,78});
    route(1,(AwSVec){64,1.8f,64},(AwSVec){64,1.8f,64.1f});world.paused[1]=1;
    aw_mission_sense(&world,&map,.1f);assert(aw_navigation_replan(&world,&map,0));
    assert(world.agents[0].detour_count>2);
    int lateral=0;for(int i=0;i<world.agents[0].detour_count;i++)lateral|=fabsf(world.agents[0].detour[i].x-64)>1;
    assert(lateral);
    for(ticks=0;ticks<1200&&!world.agents[0].arrived;ticks++)aw_mission_tick(&world,&map,actions);
    fprintf(stderr,"DETOUR ticks=%d arrival=%d contacts=%d remain=%.2f replans=%d\n",ticks,world.agents[0].arrived,world.agents[0].contacts,world.agents[0].remaining,world.agents[0].replans);
    assert(world.agents[0].arrived&&!world.agents[0].contacts);
    /* Disable equipment after measured tracks have been acquired. Neither
     * stale history nor simulator neighbor state may fill that channel. */
    for(int type=0;type<4;type++){AwSensorConfig config=world.sensors.units[0].config[type];config.enabled=0;
        aw_sensor_attach(&world.sensors,0,type,config);}
    aw_mission_sense(&world,&map,.1f);
    assert(!world.agents[0].tracks[1].valid&&world.agents[0].closing==0);
    aw_mission_world_reset(&world,&map,2);
    route(0,(AwSVec){64,1.8f,59},(AwSVec){64,1.8f,78});route(1,(AwSVec){64,1.8f,64},(AwSVec){64,1.8f,64.1f});world.paused[1]=1;
    AwSensorConfig rf=world.sensors.units[0].config[AW_SENSOR_RF];rf.enabled=0;aw_sensor_attach(&world.sensors,0,AW_SENSOR_RF,rf);
    aw_mission_sense(&world,&map,.1f);assert(world.agents[0].tracks[1].valid&&world.agents[0].tracks[1].source!=AW_SENSOR_RF+1);
    AwSVec measured=world.agents[0].tracks[1].position;world.agents[1].vehicle.position.x+=30;
    aw_navigation_tracks(&world);assert(!memcmp(&measured,&world.agents[0].tracks[1].position,sizeof(measured)));
    world.agents[1].vehicle.position.x-=30;
    for(ticks=0;ticks<1200&&!world.agents[0].arrived;ticks++)aw_mission_tick(&world,&map,actions);
    fprintf(stderr,"RANGE_DETOUR ticks=%d arrived=%d contacts=%d\n",ticks,world.agents[0].arrived,world.agents[0].contacts);
    assert(world.agents[0].arrived&&!world.agents[0].contacts);
    aw_mission_world_reset(&world,&map,2);
    route(0,(AwSVec){64,15,60},(AwSVec){64,15,80});route(1,(AwSVec){82,15,64},(AwSVec){30,15,64});
    routes[0].family=routes[0].start.family=AW_VEHICLE_QUAD;
    routes[1].family=routes[1].start.family=AW_VEHICLE_WING;routes[1].variant=routes[1].start.variant=1;routes[1].start.velocity=(AwSVec){-5,0,0};
    for(int i=0;i<2;i++){aw_mission_agent_reset(&world.agents[i],&routes[i],600);aw_mission_equip(&world,i);}
    aw_mission_sense(&world,&map,.1f);
    for(ticks=0;ticks<600&&!(world.agents[0].arrived&&world.agents[1].arrived);ticks++)aw_mission_tick(&world,&map,actions);
    fprintf(stderr,"AIR_CROSS ticks=%d arrived=%d,%d contacts=%d,%d\n",ticks,world.agents[0].arrived,world.agents[1].arrived,world.agents[0].contacts,world.agents[1].contacts);
    assert(world.agents[0].arrived&&world.agents[1].arrived&&!world.agents[0].contacts&&!world.agents[1].contacts&&!world.agents[1].vehicle.failed);
    aw_mission_world_close(&world);
    printf("NAV_RELIABILITY physical_boat_clearance=PASS draft=PASS measured_track_expiry=PASS head_on_clear_arrivals=2 parked_obstacle_detour=PASS range_fallback=PASS no_hidden_pose=PASS crossing_aircraft=PASS\n");
}
