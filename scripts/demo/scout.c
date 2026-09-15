/* Rank existing generator outputs for editorial selection. No map mutations. */
#include "ocean/alienwars/map.h"
static AwMap map;
int main(int argc,char **argv) {
    int count=argc>1?atoi(argv[1]):96;
    for(int i=0;i<count;i++) {
        uint32_t seed=i==0?73:aw_hash((uint32_t)i+1000);
        int sym=i%2, a=(int[]){2,6,8,9,4,3}[i%6], b=sym?a:11-a;
        int biome=(i/2)%4;
        if(i==0){sym=1;a=b=6;biome=2;}
        AwOptions options={sym,a,b,biome,1};
        if(!aw_generate_options(&map,seed,options)) {
            printf("{\"seed\":%u,\"valid\":false}\n",seed);fflush(stdout);continue;
        }
        int branches=0,lo=100,hi=-100,land=0,covered=0;
        for(int n=0;n<map.cave_count;n++) {
            int degree=0;for(int d=0;d<6;d++)degree+=map.cave[n].links[d]>=0;
            branches+=degree>2;
            if(map.cave[n].q<lo)lo=map.cave[n].q;
            if(map.cave[n].q>hi)hi=map.cave[n].q;
        }
        for(int n=0;n<map.trail_count;n++)covered+=map.trail[n].mode==AW_TRAIL_COVERED;
        for(int n=0;n<AW_CELLS;n++)land+=map.cells[n].q[0]>=4;
        printf("{\"seed\":%u,\"valid\":true,\"sym\":%d,\"a\":%d,\"b\":%d,\"biome\":%d,\"hash\":\"%08x\",\"nodes\":%d,\"rooms\":%d,\"branches\":%d,\"cycles\":%d,\"low_q\":%d,\"high_q\":%d,\"mountains\":%d,\"covered_trail_nodes\":%d,\"bridges\":%d,\"lakes\":%d,\"landforms\":%d,\"land_cells\":%d}\n",
            seed,sym,a,b,biome,map.hash,map.cave_count,2+map.cave_room_count,
            branches,map.cave_edge_count-map.cave_count+1,lo,hi,map.mountain_count,
            covered,map.bridge_count,map.lake_count,map.landform_count,land);
        fflush(stdout);
    }
}
