#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/map.h"
static AwMap map,repeat;
static int connected(int a,int b){for(int d=0;d<5;d++)if(aw_open(&map,a,d)==b)return 1;return 0;}
int main(int argc,char**argv){
    int seeds=argc>1?atoi(argv[1]):256;uint32_t digest=2166136261u;int terrain_seen=0,max_structure=0;
    for(int i=0;i<seeds;i++){
        uint32_t seed=i<4?(uint32_t[]){0,1,73,UINT32_MAX}[i]:aw_hash(i);
        AwOptions o={i%2,1+(i/2)%10,1+(i*7)%10,(i/20)%5,(i/100)%2};
        if(!aw_generate_options(&map,seed,o)){
            fprintf(stderr,"FAIL seed=%u sym=%d a=%d b=%d biome=%d tunnels=%d resolved=%d reached=%d\n",seed,o.symmetry,o.floors_a,o.floors_b,o.biome,o.tunnels,map.order_count,map.reached_count);return 1;
        }
        assert(aw_generate_options(&repeat,seed,o));assert(map.hash==repeat.hash);assert(!memcmp(map.cells,repeat.cells,sizeof(map.cells)));
        assert(map.order_count==AW_CELLS&&map.decisions>0);
        int seen[AW_CELLS]={0};for(int c=0;c<AW_CELLS;c++){
            assert(map.order[c]>=0&&map.order[c]<AW_CELLS&&!seen[map.order[c]]++);
            AwCell*a=&map.cells[c];terrain_seen|=1<<a->material;
            if(o.symmetry){AwCell*b=&map.cells[AW_CELLS-1-c];assert(a->material==b->material&&a->road==b->road&&a->tunnel==b->tunnel&&a->portal==b->portal);for(int k=0;k<4;k++)assert(a->q[k]==b->q[(k+2)%4]);}
            if(a->road){assert(map.reachable[c]);for(int k=0;k<4;k++)assert(aw_abs(a->q[k]-a->q[(k+1)%4])<=1);}
            if(a->tunnel){assert(o.tunnels);assert(map.reachable[c+AW_CELLS]);if(!a->portal){assert(map.walkable[c+AW_CELLS]);for(int k=0;k<4;k++)assert(a->q[k]>=12);assert(aw_open(&map,c+AW_CELLS,4)<0);}}
        }
        for(int n=0;n<AW_NODES;n++)for(int d=0;d<5;d++){int next=aw_open(&map,n,d);if(next>=0){assert(aw_open(&map,next,d==4?4:(d+2)%4)==n);assert(aw_move_cost(&map,n,next)>0);}}
        assert(map.path[0]==map.spawns[0]&&map.path[map.path_length-1]==map.spawns[1]);
        int cost=0;for(int p=1;p<map.path_length;p++){assert(connected(map.path[p-1],map.path[p]));cost+=aw_move_cost(&map,map.path[p-1],map.path[p]);}assert(cost==map.path_cost);
        assert(!aw_find_path(&map,-1,0));assert(!aw_find_path(&map,0,AW_NODES));assert(!aw_find_path(&map,0,map.spawns[0]));
        assert(aw_find_path(&map,map.spawns[0],map.spawns[0])&&map.path_length==1&&map.path_cost==0);
        if(map.structure_decisions>max_structure)max_structure=map.structure_decisions;
        digest=(digest^map.hash)*16777619u;
    }
    assert(terrain_seen==AW_ALL);assert(max_structure>0);
    assert(aw_generate(&map,73));
    /* A real occupied roof blocks the camera; the tunnel's void does not. */
    assert(aw_solid(&map,31.5f,12,31.5f));assert(!aw_solid(&map,31.5f,7,31.5f));assert(aw_solid(&map,31.5f,3,31.5f));
    assert(aw_occluded(&map,31.5f,60,31.5f,31.5f,5,31.5f));
    assert(!aw_occluded(&map,29.5f,7,31.5f,33.5f,7,31.5f));
    assert(!aw_occluded(&map,31.5f,60,31.5f,31.5f,18,31.5f));
    /* No malformed ID may reach a palette or movement-cost lookup. */
    map.cells[0].material=255;assert(!aw_validate(&map));assert(aw_generate(&map,73));
    /* Remove both portals: upper road remains, but isolated tunnel must fail. */
    for(int c=0;c<AW_CELLS;c++)if(map.cells[c].portal){map.cells[c].portal=0;map.cells[c].tunnel=0;}assert(!aw_validate(&map));
    /* Incompatible road elevation sockets and terrain sockets reject explicitly. */
    uint64_t wave[2]={1ull<<40,1ull<<4};int ramp[2]={0,1},reductions=0;
    assert(!aw_road_propagate(wave,ramp,2,&reductions));
    assert(aw_generate(&map,73));map.wave[0]=1u<<AW_LAVA;map.wave[1]=1u<<AW_SNOW;assert(!aw_propagate(&map,0));
    /* Weighted navigation must choose a longer cheap road around expensive mud. */
    memset(&map,0,sizeof(map));int start=65,end=69;
    for(int z=1;z<=2;z++)for(int x=1;x<=5;x++){int c=z*64+x;aw_flat(&map.cells[c],4);map.cells[c].material=z==1&&x>1&&x<5?AW_MUD:AW_ROAD;map.walkable[c]=1;}
    assert(aw_find_path(&map,start,end));assert(map.path_cost==60&&map.path_length==7);
    printf("MAP_TEST version=%d seeds=%d digest=%08" PRIx32 " terrains=%d max_structure=%d PASS\n",AW_VERSION,seeds,digest,__builtin_popcount(terrain_seen),max_structure);
    return 0;
}
