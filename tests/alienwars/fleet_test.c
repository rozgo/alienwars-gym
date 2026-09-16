#include "ocean/alienwars/fleet.h"
static AwFleet fleet;static AwMap map;
int main(void){
    /* Missing routes must disable the old collision body as well as its model. */
    fleet.body[0].active=fleet.active[0]=1;aw_fleet_scout_route(&fleet,&map,0);
    assert(!fleet.active[0]&&!fleet.body[0].active);
    /* Display interpolation crosses the yaw seam without changing simulation. */
    fleet.unit[0].vehicle=(AwVehicle){.position={1,2,3},.yaw=-3.12f};
    fleet.previous[0]=(AwVehicle){.position={0,0,0},.yaw=3.12f};fleet.accumulator=.05f;
    AwVehicle physical=fleet.unit[0].vehicle,display=aw_fleet_pose(&fleet,0);
    assert(display.position.x==.5f&&display.position.y==1&&display.position.z==1.5f);
    assert(fabsf(display.yaw)>3.1f&&!memcmp(&physical,&fleet.unit[0].vehicle,sizeof(physical)));
    puts("FLEET_TEST unavailable_route_body=PASS interpolation_yaw_seam=PASS simulation_unchanged=PASS");
}
