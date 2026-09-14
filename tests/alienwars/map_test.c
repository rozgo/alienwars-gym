#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/map.h"

int main(int argc, char **argv) {
    int seeds = argc > 1 ? atoi(argv[1]) : 256;
    AwMap *map = calloc(1, sizeof(AwMap));
    AwMap *repeat = calloc(1, sizeof(AwMap));
    assert(map && repeat && seeds > 0);
    uint32_t digest = 2166136261u;
    int retries = 0, min_walk = AW_CELLS, max_decisions = 0;
    for (int i = 0; i < seeds; i++) {
        uint32_t seed = i < 4 ? (uint32_t[]){0,1,73,UINT32_MAX}[i] : aw_hash((uint32_t)i);
        if (!aw_generate(map, seed)) {
            fprintf(stderr,"FAIL seed=%" PRIu32 " attempts=%d collapsed=%d decisions=%d path=%d reached=%d walk=%d\n",
                seed,map->attempts,map->order_count,map->decisions,map->path_length,map->reached_count,map->walk_count);
            for (int z=0;z<AW_SIZE;z++) {
                for(int x=0;x<AW_SIZE;x++) {
                    int c=z*AW_SIZE+x;
                    fputc(map->ramp[c]>=0?'R':map->reachable[c]?'.':map->walkable[c]?'!':' ',stderr);
                }
                fputc('\n',stderr);
            }
            return 1;
        }
        assert(aw_generate(repeat, seed));
        assert(map->hash == repeat->hash);
        assert(memcmp(map->tile, repeat->tile, sizeof(map->tile)) == 0);
        assert(map->order_count == AW_CELLS && map->decisions > 0 && map->reductions > 0);
        int seen[AW_CELLS] = {0};
        for (int c=0;c<AW_CELLS;c++) {
            assert(map->order[c]>=0 && map->order[c]<AW_CELLS);
            assert(!seen[map->order[c]]++);
            for (int d=0;d<4;d++) {
                int nb=aw_neighbor(c,d);
                if(nb>=0) assert(aw_open(map,c,nb)==aw_open(map,nb,c));
            }
        }
        assert(map->path[0]==map->spawns[0]);
        assert(map->path[map->path_length-1]==map->spawns[1]);
        for (int p=1;p<map->path_length;p++) assert(aw_open(map,map->path[p-1],map->path[p]));
        assert(!aw_find_path(map,-1,0,repeat->path));
        assert(!aw_find_path(map,0,AW_CELLS,repeat->path));
        assert(!aw_find_path(map,0,map->spawns[0],repeat->path));
        assert(aw_find_path(map,map->spawns[0],map->spawns[0],repeat->path)==1);
        retries += map->attempts-1;
        if(map->walk_count<min_walk)min_walk=map->walk_count;
        if(map->decisions>max_decisions)max_decisions=map->decisions;
        digest = (digest ^ map->hash) * 16777619u;
    }
    /* Break an actual connection and verify validation notices it. */
    for(int z=35;z<38;z++) for(int x=30;x<34;x++) map->ramp[z*AW_SIZE+x]=-1;
    assert(!aw_validate(map));
    /* Contradictory fixed corners must fail rather than emit a fallback map. */
    aw_layout(map);
    map->allowed[0]=1;
    map->allowed[1]=4;
    assert(!aw_solve(map));
    printf("MAP_TEST version=%d seeds=%d digest=%08" PRIx32 " retries=%d min_walk=%d max_decisions=%d PASS\n",
        AW_VERSION,seeds,digest,retries,min_walk,max_decisions);
    free(map);
    free(repeat);
    return 0;
}
