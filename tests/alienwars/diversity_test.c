/* Same settings, different seeds: measure geometry, not materials or hashes.
 * Coarse silhouettes catch the old fixed seven-island plan; road/cave/lake
 * occupancy catches cosmetic jitter over an otherwise repeated route graph. */
#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/map.h"
#define WORLDS 24
static AwMap map;
typedef struct {
    uint8_t road[AW_CELLS],cave[AW_CELLS],lake[AW_CELLS],height[256];
    uint64_t silhouette;
    int base,entrance,rooms,lakes,forms;
} Sample;
static Sample samples[WORLDS];
static int difference(const uint8_t*a,const uint8_t*b){
    int changed=0,either=0;
    for(int i=0;i<AW_CELLS;i++){changed+=(!!a[i])!=(!!b[i]);either+=!!(a[i]||b[i]);}
    return either?changed*1000/either:0;
}
int main(void){
    uint32_t digest=2166136261u;
    for(int symmetry=0;symmetry<=1;symmetry++){
        int bases[AW_CELLS]={0},entrances[AW_CELLS]={0},room_types=0,lake_counts=0,form_counts=0;
        int base_count=0,entrance_count=0;
        for(int i=0;i<WORLDS;i++){
            uint32_t seed=i<4?(uint32_t[]){0,1,73,UINT32_MAX}[i]:aw_hash(i);
            assert(aw_generate_options(&map,seed,(AwOptions){symmetry,6,6,1,1}));
            Sample*s=&samples[i];memset(s,0,sizeof(*s));
            s->base=map.spawns[0];s->entrance=map.cave[map.cave_entrances[0]].portal;
            s->rooms=2+map.cave_room_count;s->lakes=map.lake_count;s->forms=map.landform_count;
            base_count+=!bases[s->base]++;entrance_count+=!entrances[s->entrance]++;
            room_types|=1<<s->rooms;lake_counts|=1<<s->lakes;form_counts|=1<<s->forms;
            for(int c=0;c<AW_CELLS;c++){
                s->road[c]=map.cells[c].road;
                s->lake[c]=map.lake_mask[aw_corner_vertex(c,0)];
            }
            for(int n=0;n<map.cave_count;n++)s->cave[map.cave[n].z*64+map.cave[n].x]=1;
            for(int z=0;z<16;z++)for(int x=0;x<16;x++){
                int total=0;
                for(int dz=0;dz<4;dz++)for(int dx=0;dx<4;dx++)for(int k=0;k<4;k++)total+=map.cells[(z*4+dz)*64+x*4+dx].q[k];
                s->height[z*16+x]=(uint8_t)(total/64);
            }
            for(int z=0;z<8;z++)for(int x=0;x<8;x++){
                int land=0;
                for(int dz=0;dz<8;dz++)for(int dx=0;dx<8;dx++)land+=map.cells[(z*8+dz)*64+x*8+dx].q[0]>=4;
                if(land>=32)s->silhouette|=1ull<<(z*8+x);
            }
            digest=(digest^map.hash)*16777619u;
        }
        int pairs=0,silhouette=0,height=0,road=0,cave=0,lake=0,distinct_outline=0;
        for(int i=0;i<WORLDS;i++)for(int j=0;j<i;j++){
            Sample*a=&samples[i],*b=&samples[j];pairs++;
            int bits=__builtin_popcountll(a->silhouette^b->silhouette);silhouette+=bits;distinct_outline+=bits>=4;
            for(int c=0;c<256;c++)height+=aw_abs(a->height[c]-b->height[c]);
            road+=difference(a->road,b->road);cave+=difference(a->cave,b->cave);lake+=difference(a->lake,b->lake);
        }
        /* Floors/palette/symmetry stay identical within each cohort. */
        assert(base_count>=WORLDS*3/4&&entrance_count>=WORLDS*3/4);
        assert(__builtin_popcount(room_types)>=2&&__builtin_popcount(lake_counts)>=2&&__builtin_popcount(form_counts)>=4);
        assert(silhouette>=pairs*6&&distinct_outline>=pairs*3/4);
        assert(height>=pairs*256*2);
        assert(road>=pairs*450&&cave>=pairs*650&&lake>=pairs*650);
        printf("DIVERSITY sym=%d worlds=%d bases=%d entrances=%d outline_bits_x100=%d height_q_x100=%d road_change_permille=%d cave_change_permille=%d lake_change_permille=%d room_types=%d lake_counts=%d form_counts=%d PASS\n",
            symmetry,WORLDS,base_count,entrance_count,silhouette*100/pairs,height*100/(pairs*256),road/pairs,cave/pairs,lake/pairs,
            __builtin_popcount(room_types),__builtin_popcount(lake_counts),__builtin_popcount(form_counts));
    }
    printf("DIVERSITY_TEST version=%d digest=%08" PRIx32 " PASS\n",AW_VERSION,digest);
}
