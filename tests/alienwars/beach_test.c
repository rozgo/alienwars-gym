/* Beaches must be actual shared geometry, with navigable dry shore and no
 * flooding, road clipping, lost cliffs or rotational seams. */
#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/map.h"
static AwMap map;
int main(void){
    map.options=(AwOptions){1,1,1,1,0};map.layout_seed=73;map.landmarks[0]=0;map.landmarks[1]=4095;
    for(int z=12;z<=52;z++)for(int x=12;x<=52;x++)map.macro_q[z*65+x]=4;
    for(int z=29;z<=35;z++)for(int x=29;x<=35;x++)map.macro_q[z*65+x]=0;
    /* Mirrored high cliffs remain high; a road near the north shore stays dry. */
    for(int z=12;z<=16;z++)for(int x=28;x<=36;x++){map.macro_q[z*65+x]=16;map.macro_q[(64-z)*65+64-x]=16;}
    map.cells[14*64+20].road=1;aw_flat(&map.cells[14*64+20],4);
    map.cells[4095-(14*64+20)].road=1;aw_flat(&map.cells[4095-(14*64+20)],4);
    aw_beach_shelves(&map);aw_rolling_hills(&map);
    int levels[5]={0};for(int v=0;v<65*65;v++){
        assert(map.macro_q[v]==map.macro_q[65*65-1-v]);assert(map.rolling[v]==map.rolling[65*65-1-v]);
        if(map.rolling[v]&&map.macro_q[v]<=4)levels[map.macro_q[v]]++;
    }
    assert(levels[1]>0&&levels[2]>0&&levels[3]>0);
    assert(map.macro_q[12*65+32]==16&&map.macro_q[14*65+20]==4);
    assert(map.macro_q[32*65+32]==0&&map.macro_q[5*65+5]==0);
    assert(aw_shape_wfc(&map));
    int retained[4]={0};
    for(int c=0;c<AW_CELLS;c++)for(int k=0;k<4;k++){
        int v=aw_corner_vertex(c,k),q=map.cells[c].q[k];
        if(map.rolling[v]&&map.macro_q[v]<4){assert(map.blend[v]==255);if(q<4)retained[q]++;}
    }
    /* Road shoulders may raise a beach grade, but WFC must keep all three
     * intermediate shore levels rather than rounding everything to 0/4. */
    assert(retained[1]>0&&retained[2]>0&&retained[3]>0);
    /* A 2→3 beach ramp remains linear, including at shared edge samples. */
    memset(&map,0,sizeof(map));memset(map.blend,255,sizeof(map.blend));
    map.cells[0]=(AwCell){.q={2,3,3,2},.material=AW_SAND};map.cells[1]=(AwCell){.q={3,4,4,3},.material=AW_SAND};
    map.spawns[0]=0;aw_navigation(&map);assert(map.walkable[0]&&map.walkable[1]);
    for(int i=0;i<=AW_SUBDIV;i++){
        float t=(float)i/AW_SUBDIV;assert(fabsf(aw_surface_q(&map,0,t,.5f)-(2+t))<.00001f);
        assert(aw_surface_q(&map,0,1,t)==aw_surface_q(&map,1,0,t));
    }
    aw_flat(&map.cells[0],1);aw_navigation(&map);assert(!map.walkable[0]);
    printf("BEACH_TEST quarter_grades=PASS dry_shore_navigation=PASS wet_shore_blocked=PASS linear_support=PASS shared_edges=PASS cliffs=PASS roads=PASS symmetry=PASS\n");
}
