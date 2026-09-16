#include <assert.h>
#include "ocean/alienwars/local_navigation.h"
static AwMap m;
static int small_edges(void*ctx,int n,AwAStarEdge*out){(void)ctx;if(n==0){out[0]=(AwAStarEdge){1,9};out[1]=(AwAStarEdge){2,1};return 2;}if(n==2){out[0]=(AwAStarEdge){1,1};return 1;}if(n==1){out[0]=(AwAStarEdge){3,1};return 1;}return 0;}
static float zero(void*ctx,int a,int b){(void)ctx;(void)a;(void)b;return 0;}
int main(){
 AwAStar a={0};assert(aw_astar_init(&a,8));int path[8];assert(aw_astar_path(&a,4,0,3,NULL,small_edges,zero,path,8)==4);assert(path[1]==2&&a.cost[3]==3);assert(!aw_astar_path(&a,4,3,0,NULL,small_edges,zero,path,8));assert(!aw_astar_path(&a,4,0,3,NULL,small_edges,zero,path,2));aw_astar_close(&a);
 assert(aw_generate(&m,73));
 AwVehicle wing={.family=AW_VEHICLE_WING,.variant=1,.position={64,45,64}};int brake[4]={0,2,1,2};AwVehicleSpec spec=aw_vehicle_spec(wing.family,wing.variant);
 for(int i=0;i<60;i++){aw_vehicle_step(&m,&wing,brake,1.0f/30,NULL,0,-1);float f=wing.velocity.x*sinf(wing.yaw)+wing.velocity.z*cosf(wing.yaw);assert(f>=spec.reverse-.001f);assert(fabsf(wing.yaw_rate)<=spec.turn+.001f);assert(fabsf(wing.velocity.x*cosf(wing.yaw)-wing.velocity.z*sinf(wing.yaw))<.001f);}assert(wing.yaw>0&&wing.yaw<=spec.turn*2+.001f&&!wing.failed);
 AwVehicle quad={.family=AW_VEHICLE_QUAD,.position={64,45,64}};int strafe[4]={1,1,1,2};for(int i=0;i<30;i++)aw_vehicle_step(&m,&quad,strafe,1.0f/30,NULL,0,-1);assert(quad.position.x>65&&fabsf(quad.position.z-64)<.001f&&quad.yaw==0);
 int reverse[4]={0,1,1,1};for(int i=0;i<30;i++)aw_vehicle_step(&m,&quad,reverse,1.0f/30,NULL,0,-1);assert(quad.position.z<63&&quad.yaw==0);
 AwVehicle sub={.family=AW_VEHICLE_SUB,.position={-20,-4,64}};assert(aw_vehicle_clear(&m,&sub));sub.position.y=0;assert(!aw_vehicle_clear(&m,&sub));sub.position.y=-20;assert(!aw_vehicle_clear(&m,&sub));
 AwBody x={{0,0,0},{0},0,.5f,2,.5f,1},y={{1.2f,0,0},{0},0,.5f,2,.5f,1};assert(!aw_bodies_overlap(x,y,0));y.yaw=AW_MOTION_PI*.5f;assert(aw_bodies_overlap(x,y,0));y.position.y=2;assert(!aw_bodies_overlap(x,y,0));
 AwVehicle near={.family=AW_VEHICLE_QUAD,.position={64,45,64}};AwBody obstacle={{65.7f,45,64},{0},0,.3f,1,1,1};for(int i=0;i<60;i++)aw_vehicle_step(&m,&near,strafe,1.0f/30,&obstacle,1,-1);assert(!aw_bodies_overlap(aw_vehicle_body(&near),obstacle,0));assert(near.position.x<65.4f);
 printf("VEHICLE_TEST astar=PASS fixed_wing_forward_and_turn_bounds=PASS quad_strafe_reverse=PASS sub_medium=PASS oriented_collision=PASS no_penetration=PASS\n");
}
