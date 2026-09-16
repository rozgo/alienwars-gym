#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/patrols.h"
static AwMap map;static AwPatrols fleet,repeat;
int main(void){
 uint32_t digest=2166136261u;int routes=0,ground=0,naval=0,air=0,sub=0;
 for(int sym=0;sym<2;sym++)for(int seed_index=0;seed_index<4;seed_index++){
  uint32_t seed=(uint32_t[]){0,1,73,UINT32_MAX}[seed_index];
  assert(aw_generate_options(&map,seed,(AwOptions){sym,6,6,1,1}));
  int path[AW_NODES],length=map.path_length,cost=map.path_cost;memcpy(path,map.path,sizeof(path));uint32_t hash=map.hash;
  assert(aw_patrol_build(&map,&fleet)==AW_PATROLS);
  assert(fleet.units[5].speed<=fleet.units[6].speed*.26f&&fleet.units[5].speed<=fleet.units[7].speed*.35f);
  assert(map.hash==hash&&aw_fingerprint(&map)==hash&&map.path_length==length&&map.path_cost==cost&&!memcmp(path,map.path,sizeof(path)));
  assert(aw_patrol_build(&map,&repeat)==AW_PATROLS);
  for(int unit=0;unit<AW_PATROLS;unit++){
   AwPatrol*p=&fleet.units[unit];assert(p->count>1&&p->count<AW_NODES&&p->count==repeat.units[unit].count);
   assert(!memcmp(p->route,repeat.units[unit].route,p->count*sizeof(int)));routes++;
   for(int i=0;i<p->count;i++){
    digest=(digest^(uint32_t)p->route[i])*16777619u;
    if(p->layer==AW_PATROL_GROUND){ground++;if(i)assert(aw_patrol_ground_edge(&map,p->route[i-1],p->route[i]));}
    if(p->layer==AW_PATROL_NAVAL){naval++;assert(aw_patrol_water_cell(&map,p->route[i],p->variant));if(i){int d=aw_abs(p->route[i]-p->route[i-1]);assert(d==1||d==96);}}
    if(p->layer==AW_PATROL_SUB){sub++;AwVolumeGraph g={.map=&map,.submarine=1,.variant=p->variant};AwPatrolPoint a=aw_patrol_position(&map,p,(float)i);assert(aw_volume_clear(&g,(AwRoutePoint){a.x,a.q,a.z}));}
    if(p->layer==AW_PATROL_AIR){air++;assert(p->altitude[i]>=aw_patrol_roof(&map,p->route[i]%64+.5f,p->route[i]/64+.5f)+2.99f);}
   }
   for(int i=0;i<(p->count-1)*8;i++){
    AwPatrolPoint a=aw_patrol_position(&map,p,i/8.0f);assert(isfinite(a.x)&&isfinite(a.q)&&isfinite(a.z));
    if(p->layer==AW_PATROL_SUB){AwVolumeGraph g={.map=&map,.submarine=1,.variant=p->variant};assert(aw_volume_clear(&g,(AwRoutePoint){a.x,a.q,a.z}));}
    if(p->layer==AW_PATROL_GROUND)assert(aw_body_fits(&map,a.x,a.q,a.z,1));
    if(p->layer==AW_PATROL_AIR){assert(a.q>=aw_patrol_roof(&map,a.x,a.z)+2.99f);assert(aw_density(&map,a.x,a.q,a.z)<0);}
   }
   AwPatrolPoint a=aw_patrol_position(&map,p,.5f),b=aw_patrol_position(&map,p,p->count+.5f);
   assert(fabsf(a.x-b.x)+fabsf(a.z-b.z)>0); /* Both continuous motion and a return/loop phase. */
  }
 }
 printf("PATROL_TEST worlds=8 routes=%d digest=%08" PRIx32 " ground_nodes=%d naval_nodes=%d air_nodes=%d large_body=PASS draft_and_mast=PASS flight_clearance=PASS deterministic=PASS map_unchanged=PASS\n",routes,digest,ground,naval,air);assert(sub>0);
}
