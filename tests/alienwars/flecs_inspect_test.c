#include "ocean/alienwars/missions.h"
static AwMap map;
int main(void){
    for(int n=0;n<AW_CELLS;n++)aw_flat(&map.cells[n],4);
    AwMissionWorld world={0};aw_mission_world_close(&world);assert(aw_mission_world_init(&world));aw_mission_world_reset(&world,&map,12);
    AwMissionAgent*agents=world.agents;
    world.agents[0].vehicle.position=(AwSVec){12.5f,3,8};world.agents[0].ticks=77;world.active[0]=1;
    world.perception[0].odometry.distance=43.5f;
    assert(ecs_lookup(world.ecs,"Fleet.Inspection_scout")==world.entities[0]);
    const char*value=aw_inspect_request(&world,"GET","entity/Fleet/Inspection_scout?values=true&type_info=true");
    assert(strstr(value,"12.5")&&strstr(value,"43.5")&&strstr(value,"77")&&strstr(value,"AwMission"));
    puts(value);
    value=aw_inspect_request(&world,"GET","query?expr=AwMission&values=true&count=16");
    assert(strstr(value,"Inspection_scout")&&strstr(value,"Quadrotor"));
    assert(strstr(aw_inspect_request(&world,"DELETE","entity/Fleet/Inspection_scout"),"405"));
    assert(strstr(aw_inspect_request(&world,"PUT","entity/Injected"),"405"));
    assert(!ecs_lookup(world.ecs,"Injected"));
    assert(strstr(aw_inspect_request(&world,"GET","entity/NoSuchEntity"),"404"));
    assert(strstr(aw_inspect_request(&world,"GET",""),"400"));
    aw_mission_world_bind(&world);assert(world.agents==agents&&world.agents[0].ticks==77);
    aw_mission_world_reset(&world,&map,12);
    assert(world.agents==agents&&world.agents[0].ticks==0);
    value=aw_inspect_request(&world,"GET","entity/Fleet/Inspection_scout?values=true");
    assert(strstr(value,"AwMission"));aw_mission_world_close(&world);
    assert(aw_mission_world_init(&world));aw_mission_world_reset(&world,&map,12);
    assert(strstr(aw_inspect_request(&world,"GET","entity/Fleet/Inspection_scout?values=true"),"AwMission"));
    aw_mission_world_close(&world);
    fprintf(stderr,"FLECS_INSPECT live_values=PASS named_units=PASS queries=PASS immutable_topology=PASS reset_recreate=PASS\n");
}
