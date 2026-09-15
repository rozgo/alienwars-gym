#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/map.h"
static AwMap map,without;
int main(void){
    uint32_t digest=2166136261u;int worlds=0,bridges=0,by_sym[2]={0},min_slopes=4096,raised=0,wet=0;
    for(int sym=0;sym<2;sym++)for(int i=0;i<16;i++){
        uint32_t seed=i<4?(uint32_t[]){0,1,73,UINT32_MAX}[i]:aw_hash(i);
        assert(aw_generate_options(&map,seed,(AwOptions){sym,6,6,1,1}));
        int slopes=0;
        for(int c=0;c<AW_CELLS;c++)if(!map.cells[c].road&&map.walkable[c]){
            int lo=40,hi=0;for(int k=0;k<4;k++){int q=map.cells[c].q[k];if(q<lo)lo=q;if(q>hi)hi=q;}
            slopes+=hi>lo;
        }
        if(slopes<min_slopes)min_slopes=slopes;
        if(sym)for(int v=0;v<AW_VERT*AW_VERT;v++){
            assert(map.rolling[v]==map.rolling[AW_VERT*AW_VERT-1-v]);
            assert(map.blend[v]==map.blend[AW_VERT*AW_VERT-1-v]);
        }
        for(int c=0;c<AW_CELLS;c++)if(map.bridge_bins[c]&&map.walkable[c]){
            float q=aw_surface_q(&map,c,.5f,.5f);
            assert(aw_body_fits(&map,c%64+.5f,q,c/64+.5f,0));
        }
        without=map;without.bridge_count=0;memset(without.bridge_bins,0,sizeof(without.bridge_bins));aw_ocean_build(&without);
        assert(!memcmp(map.ocean_depth,without.ocean_depth,sizeof(map.ocean_depth)));
        assert(!memcmp(map.ocean_connected,without.ocean_connected,sizeof(map.ocean_connected)));
        worlds+=!!map.bridge_count;by_sym[sym]+=!!map.bridge_count;bridges+=map.bridge_count;
        for(int j=0;j<map.bridge_count;j++){
            const AwBridge*b=&map.bridges[j];assert(aw_bridge_validate(&map,b));int gaps=0;
            AwBridge broken=*b;broken.crown=0;assert(!aw_bridge_validate(&map,&broken));
            for(int u=0;u<=b->length;u++){
                float x=b->x+.5f+b->dx*u,z=b->z+.5f+b->dz*u,q=aw_bridge_q(b,u),bed=aw_height_q(&map,x,z);
                assert(fabsf(aw_support_q(&map,x,z,q)-q)<.001f);
                assert(aw_body_fits(&map,x,q,z,1));
                if(u)raised+=q!=aw_bridge_q(b,u-1);
                if(bed<1.44f&&q-bed>2){
                    gaps++;wet++;
                    assert(aw_density(&map,x,q-.5f,z)>0); /* Actual deck, not painted water. */
                    assert(aw_density(&map,x,q-1.5f,z)<0); /* Open underneath. */
                    assert(fabsf(aw_support_q(&map,x,z,bed)-aw_support_q(&without,x,z,bed))<.001f);
                    float ox=x+b->dz*1.5f,oz=z-b->dx*1.5f;
                    if(aw_height_q(&map,ox,oz)<q-2)assert(aw_density(&map,ox,q-.5f,oz)<0);
                }
            }
            assert(gaps>=2&&gaps*3>=b->length);
            if(sym){const AwBridge*a=&map.bridges[j^1];
                assert(a->x+b->x==63&&a->z+b->z==63&&a->dx==-b->dx&&a->dz==-b->dz&&a->length==b->length&&a->qa==b->qa&&a->qb==b->qb&&a->crown==b->crown);
            }
        }
        digest=(digest^map.hash)*16777619u;
    }
    assert(min_slopes>=40&&worlds>=16&&by_sym[0]>=6&&by_sym[1]>=6&&raised>20&&wet>20);
    printf("RELIEF_TEST version=%d worlds=32 digest=%08" PRIx32 " bridge_worlds=%d bridges=%d min_walkable_slopes=%d graded_segments=%d water_spans=%d deck_support=PASS underpass=PASS symmetry=PASS ocean_unchanged=PASS\n",AW_VERSION,digest,worlds,bridges,min_slopes,raised,wet);
}
