/* Viewer lifecycle: these are demo episode restarts, never training successes. */
#ifndef AW_NAV_VERSION
#define AW_NAV_VERSION 2
#endif
#include "ocean/alienwars/command_fleet.h"
static AwCommandFleet fleet;static AwMap map;static AwPatrols patrol;
static void finish_restart(int i){
    int arrivals=fleet.arrivals[i],restarts=fleet.patrol_restarts[i];
    aw_command_fleet_continue(&fleet,&map);
    assert(fleet.restart_at[i]&&fleet.status[i]==AW_MISSION_RESTARTING);
    fleet.world.ticks=fleet.restart_at[i];aw_command_fleet_continue(&fleet,&map);
    assert(!fleet.unit[i].timeout&&!fleet.unit[i].vehicle.failed&&!fleet.restart_at[i]);
    assert(fleet.patrol_restarts[i]==restarts+1&&fleet.arrivals[i]==arrivals);
    assert(fleet.terminal[i]&&fleet.status[i]==AW_MISSION_TRAVELLING);
    assert(!memcmp(&fleet.previous[i],&fleet.unit[i].vehicle,sizeof(AwVehicle)));
    assert(aw_vehicle_clear(&map,&fleet.unit[i].vehicle));
    for(int j=0;j<AW_UNITS;j++)if(j!=i&&fleet.active[j])
        assert(!aw_bodies_overlap(aw_vehicle_body(&fleet.unit[i].vehicle),aw_vehicle_body(&fleet.unit[j].vehicle),0));
}
int main(void){
    assert(aw_generate_options(&map,73,(AwOptions){1,6,6,AW_TEMPERATE,1}));
    unsigned hash=map.hash;aw_patrol_build(&map,&patrol);aw_command_fleet_init(&fleet,&map,&patrol);
    assert(fleet.ready&&fleet.loop_patrols);
    AwVehicle untouched=fleet.unit[10].vehicle;
    for(int i=0;i<AW_UNITS;i++)assert(fleet.active[i]);
    for(int i=0;i<AW_UNITS;i++){
        AwSensorConfig equipment=fleet.world.sensors.units[i].config[AW_SENSOR_LIDAR];equipment.enabled=0;
        aw_sensor_attach(&fleet.world.sensors,i,AW_SENSOR_LIDAR,equipment);
        fleet.total_contacts[i]=17;fleet.arrivals[i]=4;
        fleet.world.sensors.units[i].odometry.distance=123;
        if(fleet.unit[i].vehicle.family==AW_VEHICLE_WING){fleet.unit[i].vehicle.failed=1;fleet.unit[i].vehicle.velocity=(AwSVec){0};}
        else fleet.unit[i].timeout=1;
        finish_restart(i);
        assert(!fleet.world.sensors.units[i].config[AW_SENSOR_LIDAR].enabled);
        assert(fleet.total_contacts[i]==17&&fleet.unit[i].odometry_origin==123);
        assert(fleet.world.sensors.units[i].odometry.distance==123);
        if(i<10)assert(!memcmp(&untouched,&fleet.unit[10].vehicle,sizeof(untouched)));
        if(fleet.unit[i].vehicle.family==AW_VEHICLE_WING)assert(aw_sv_length(fleet.unit[i].vehicle.velocity)>0);
    }
    /* Pausing on the exact restart boundary must take effect immediately. */
    fleet.unit[6].timeout=1;aw_command_fleet_continue(&fleet,&map);
    fleet.world.ticks=fleet.restart_at[6];int loops=fleet.patrol_restarts[6];AwVehicle paused=fleet.unit[6].vehicle;
    aw_command_fleet_step(&fleet,&map,.1f,0,0);
    assert(fleet.unit[6].timeout&&fleet.patrol_restarts[6]==loops);
    assert(!memcmp(&paused,&fleet.unit[6].vehicle,sizeof(paused)));
    fleet.world.paused[6]=0;aw_command_fleet_continue(&fleet,&map);assert(fleet.patrol_restarts[6]==loops+1);
    memset(fleet.world.paused,0,AW_UNITS);
    /* A parked peer at every stored anchor delays restart, without spawning
     * overlapping hulls. Clearing one anchor allows a later attempt. */
    int blockers[]={0,1,2,3};AwVehicle saved[4];
    for(int k=0;k<4;k++){saved[k]=fleet.unit[blockers[k]].vehicle;fleet.unit[blockers[k]].vehicle=fleet.patrol_spawn[6][k];}
    fleet.unit[6].timeout=1;loops=fleet.patrol_restarts[6];aw_command_fleet_continue(&fleet,&map);
    fleet.world.ticks=fleet.restart_at[6];aw_command_fleet_continue(&fleet,&map);
    assert(fleet.unit[6].timeout&&fleet.patrol_restarts[6]==loops&&fleet.restart_at[6]>fleet.world.ticks);
    for(int k=0;k<4;k++)fleet.unit[blockers[k]].vehicle=saved[k];
    fleet.world.ticks=fleet.restart_at[6];aw_command_fleet_continue(&fleet,&map);assert(fleet.patrol_restarts[6]==loops+1);
    /* A stalled automatic mission loops; deliberate manual commands don't. */
    fleet.unit[6].deadlock_ticks=300;finish_restart(6);
    fleet.unit[6].arrived=1;fleet.arrival_handled[6]=1;fleet.status[6]=AW_MISSION_UNAVAILABLE;finish_restart(6);
    fleet.automatic[6]=0;fleet.unit[6].timeout=1;loops=fleet.patrol_restarts[6];
    fleet.unit[6].control_version=2;aw_command_fleet_continue(&fleet,&map);
    assert(fleet.unit[6].timeout&&!fleet.restart_at[6]&&fleet.patrol_restarts[6]==loops);
    /* Explicit endurance mode retains the old no-respawn measurement. */
    fleet.loop_patrols=0;fleet.unit[7].vehicle.failed=1;aw_command_fleet_continue(&fleet,&map);
    assert(fleet.unit[7].vehicle.failed&&!fleet.restart_at[7]);
    assert(map.hash==hash);aw_command_fleet_close(&fleet);
    puts("PATROL_LOOP all_12_roles=PASS safe_respawn=PASS blocked_spawn_retry=PASS pause=PASS manual_commands=PASS recurrent_terminal=PASS equipment=PASS lifetime_counters=PASS no_false_arrivals=PASS endurance_opt_out=PASS map_unchanged=PASS");
}
