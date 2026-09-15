#include <assert.h>
#include "ocean/alienwars/detail.h"
static AwMap map;
int main(void){
    /* Periodic samples on both sides of every wrap must remain continuous;
     * this catches texture seams before triplanar blending hides some of them. */
    for(int i=0;i<257;i++){
        float u=i/257.0f,v=(i*97%257)/257.0f,h[4],x[4],y[4],edge[4],near[4];
        aw_detail_sample(u,v,h);aw_detail_sample(u+1,v,x);aw_detail_sample(u,v-1,y);
        aw_detail_sample(0,v,edge);aw_detail_sample(-.00001f,v,near);
        for(int k=0;k<4;k++){
            assert(isfinite(h[k])&&h[k]>=0&&h[k]<=1);
            assert(fabsf(h[k]-x[k])<.0001f&&fabsf(h[k]-y[k])<.0001f);
            assert(fabsf(edge[k]-near[k])<.005f);
        }
    }
    for(int c=0;c<AW_CELLS;c++)map.cells[c].material=AW_GRASS;
    float w[4];aw_detail_weights(&map,0,0,w);assert(w[0]==1&&w[1]+w[2]+w[3]==0);
    map.cells[0].material=AW_ROAD;map.cells[1].material=AW_SNOW;
    map.cells[64].material=AW_DIRT;map.cells[65].material=AW_ROCK;
    aw_detail_weights(&map,1,1,w);assert(w[0]==0&&w[1]==.25f&&w[2]==.25f&&w[3]==.25f);
    aw_detail_weights(&map,64,64,w);assert(w[0]==1);
    printf("DETAIL_TEST periodicity=257 edge continuity, finite channels, shared material weights/corners PASS\n");
}
