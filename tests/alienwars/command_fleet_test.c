#define AW_NAV_VERSION 3
#include "ocean/alienwars/command_fleet.h"
static AwCommandFleet fleet;static AwMap map;static AwPatrols patrol;
int main(void){
    assert(aw_generate(&map,73));unsigned hash=map.hash;aw_patrol_build(&map,&patrol);aw_command_fleet_init(&fleet,&map,&patrol);
    assert(fleet.ready&&fleet.active[6]);
    AwVehicle*v=&fleet.unit[6].vehicle;v->velocity=(AwSVec){.2f,.1f,.3f};AwVehicle before=*v;
    fleet.world.sensors.units[6].odometry.distance=1500;
    AwSVec goal=aw_sv_add(v->position,(AwSVec){4,0,4});
    fleet.recovery_attempts[6]=3;fleet.recovery_after[6]=1000;
    assert(aw_command_fleet_destination(&fleet,&map,6,goal,0));
    assert(!fleet.recovery_attempts[6]&&!fleet.recovery_after[6]);
    assert(!memcmp(v,&before,sizeof(before)));assert(!fleet.automatic[6]&&fleet.terminal[6]);
    assert(fleet.unit[6].observation[32+12]==0&&fleet.world.sensors.units[6].odometry.distance==1500);
    int points=fleet.route[6].count;AwSVec destination=fleet.destination[6];
    assert(!aw_command_fleet_destination(&fleet,&map,6,(AwSVec){NAN,0,0},0));
    assert(!aw_command_fleet_destination(&fleet,&map,6,(AwSVec){64,-50,64},0));
    assert(points==fleet.route[6].count&&!memcmp(&destination,&fleet.destination[6],sizeof(destination)));
    assert(fleet.status[6]==AW_MISSION_TRAVELLING);
    fleet.unit[6].max_deadlock_ticks=200;fleet.unit[6].deadlock_ticks=0;
    aw_command_fleet_continue(&fleet,&map);assert(!fleet.recovery_attempts[6]);
    for(int retry=1;retry<=3;retry++){
        fleet.unit[6].timeout=1;aw_command_fleet_continue(&fleet,&map);
        assert(fleet.recovery_attempts[6]==retry&&!fleet.unit[6].timeout);
        assert(!memcmp(v,&before,sizeof(before)));
        fleet.unit[6].timeout=1;aw_command_fleet_continue(&fleet,&map);
        assert(fleet.recovery_attempts[6]==retry&&fleet.unit[6].timeout);
        fleet.world.ticks+=300;
    }
    aw_command_fleet_continue(&fleet,&map);assert(fleet.recovery_attempts[6]==3&&fleet.unit[6].timeout);
    assert(aw_command_fleet_destination(&fleet,&map,6,goal,0));assert(!fleet.recovery_attempts[6]);
    aw_command_fleet_step(&fleet,&map,.2f,0,0);
    assert(!memcmp(&before.position,&v->position,sizeof(v->position))&&!memcmp(&before.velocity,&v->velocity,sizeof(v->velocity)));
    AwSensorConfig config=fleet.world.sensors.units[6].config[AW_SENSOR_LIDAR];config.enabled=0;
    aw_sensor_attach(&fleet.world.sensors,6,AW_SENSOR_LIDAR,config);aw_mission_observe(&fleet.world);
    for(int i=0;i<AW_SENSOR_SLOT_OBS;i++)assert(fleet.unit[6].observation[32+13+i]==0);
    assert(map.hash==hash);aw_command_fleet_close(&fleet);
    puts("COMMAND_FLEET valid_goal=PASS preserve_pose_momentum=PASS rejected_goal_retains_mission=PASS bounded_recovery=PASS new_goal_budget=PASS pause=PASS equipment_changes_observation=PASS map_unchanged=PASS");
}
