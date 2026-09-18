/* Run the real continuous viewer mission loop headlessly, with no world reset.
 * Same command/replanning/inference code as the browser; browser verification
 * remains necessary for rendering and interactive timing. */
#define AW_NAV_VERSION 3
#include "ocean/alienwars/command_fleet.h"
static AwMap map;static AwPatrols patrol;static AwCommandFleet fleet;
int main(int argc,char**argv){
    unsigned seed=argc>1?(unsigned)atoi(argv[1]):73;int decisions=argc>2?atoi(argv[2]):6000;
    if(!aw_generate_options(&map,seed,(AwOptions){1,6,6,AW_TEMPERATE,1}))return 2;
    aw_patrol_build(&map,&patrol);aw_command_fleet_init(&fleet,&map,&patrol);
    unsigned hash=map.hash;int initial=0;for(int i=0;i<12;i++)initial+=fleet.active[i];
    for(int tick=0;tick<decisions;tick++){
        aw_command_fleet_step(&fleet,&map,.1f,1,1);
        assert(map.hash==hash);
        for(int i=0;i<12;i++)if(fleet.active[i]){
            const AwVehicle*v=&fleet.unit[i].vehicle;assert(isfinite(v->position.x)&&isfinite(v->position.y)&&isfinite(v->position.z));
            for(int j=i+1;j<12;j++)if(fleet.active[j])assert(!aw_bodies_overlap(aw_vehicle_body(v),aw_vehicle_body(&fleet.unit[j].vehicle),0));
        }
    }
    for(int i=0;i<12;i++)printf("{\"unit\":%d,\"family\":%d,\"active\":%d,\"arrivals\":%d,\"contacts\":%d,\"collisions\":%d,\"timeout\":%d,\"failed\":%d,\"remaining\":%.3f,\"global_retries\":%d,\"terrain_contacts\":%d,\"unit_contacts\":%d}\n",i,fleet.unit[i].vehicle.family,fleet.active[i],fleet.arrivals[i],fleet.total_contacts[i],fleet.total_collisions[i],fleet.unit[i].timeout,fleet.unit[i].vehicle.failed,fleet.unit[i].remaining,fleet.recovery_attempts[i],fleet.total_terrain[i],fleet.total_units[i]);
    assert(fleet.world.ticks==decisions);printf("ENDURANCE seed=%u decisions=%d initial_active=%d trained=%d no_resets=PASS finite=PASS nonoverlap=PASS\n",seed,decisions,initial,fleet.trained);
    aw_command_fleet_close(&fleet);
}
