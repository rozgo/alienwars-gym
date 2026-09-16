#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/map.h"
static AwMap map,repeat;
static int connected(int a,int b){for(int d=0;d<AW_LINKS;d++)if(aw_open(&map,a,d)==b)return 1;return 0;}
int main(int argc,char**argv){
    int seeds=argc>1?atoi(argv[1]):256;uint32_t digest=2166136261u;int terrain_seen=0,max_structure=0,geometry_choices=0;
    int entrance_seen[AW_CELLS]={0},depth_seen=0,layouts=0,independent=0;
    for(int i=0;i<seeds;i++){
        uint32_t seed=i<4?(uint32_t[]){0,1,73,UINT32_MAX}[i]:aw_hash(i);
        AwOptions o={i%2,1+(i/2)%10,1+(i*7)%10,AW_TEMPERATE+(i/20)%AW_BIOMES,(i/(20*AW_BIOMES))%2};
        if(!aw_generate_options(&map,seed,o)){
            fprintf(stderr,"FAIL seed=%u sym=%d a=%d b=%d biome=%d tunnels=%d resolved=%d reached=%d\n",seed,o.symmetry,o.floors_a,o.floors_b,o.biome,o.tunnels,map.order_count,map.reached_count);return 1;
        }
        assert(aw_generate_options(&repeat,seed,o));assert(map.hash==repeat.hash);assert(!memcmp(map.cells,repeat.cells,sizeof(map.cells)));
        assert(map.lake_count>=1&&map.lake_count<=AW_LAKES&&aw_lakes_valid(&map));
        assert(map.lake_count==repeat.lake_count&&!memcmp(map.lakes,repeat.lakes,sizeof(map.lakes)));
        for(int l=0;l<map.lake_count;l++){
            AwLake*a=&map.lakes[l];assert(aw_height_q(&map,a->x,a->z)<1.44f);
            assert(!map.ocean_connected[(a->z+AW_OCEAN_BELT)*AW_OCEAN_SIZE+a->x+AW_OCEAN_BELT]);
            if(o.symmetry){int partner=0;for(int j=0;j<map.lake_count;j++){AwLake*b=&map.lakes[j];partner|=a->x+b->x==64&&a->z+b->z==64&&a->rx==b->rx&&a->rz==b->rz;}assert(partner);}
        }
        assert(map.ocean_count>=AW_OCEAN_CELLS-AW_CELLS);
        assert(!memcmp(map.ocean_depth,repeat.ocean_depth,sizeof(map.ocean_depth)));
        int ocean_count=0;
        for(int c=0;c<AW_OCEAN_CELLS;c++){
            ocean_count+=map.ocean_connected[c];
            if(map.ocean_connected[c])assert(map.ocean_depth[c]>=100);
            if(o.symmetry){assert(map.ocean_connected[c]==map.ocean_connected[AW_OCEAN_CELLS-1-c]);assert(map.ocean_depth[c]==map.ocean_depth[AW_OCEAN_CELLS-1-c]);}
        }
        assert(ocean_count==map.ocean_count);
        if(i%16==0){
            int route[AW_OCEAN_CELLS],length=aw_ocean_path(&map,0,AW_OCEAN_CELLS-1,600,route,AW_OCEAN_CELLS);
            assert(length>=AW_OCEAN_SIZE&&route[0]==0&&route[length-1]==AW_OCEAN_CELLS-1);
            for(int j=1;j<length;j++){assert(map.ocean_depth[route[j]]>=600);int adjacent=0;for(int d=0;d<4;d++)adjacent|=aw_ocean_neighbor(route[j-1],d)==route[j];assert(adjacent);}
            assert(!aw_ocean_path(&map,0,AW_OCEAN_CELLS-1,1400,route,AW_OCEAN_CELLS));
            assert(!aw_ocean_path(&map,-1,0,100,route,AW_OCEAN_CELLS));
            assert(!aw_ocean_path(&map,0,AW_OCEAN_CELLS-1,100,route,1));
        }
        assert(map.order_count==AW_CELLS&&map.decisions>0);assert(!memcmp(map.order,repeat.order,sizeof(map.order)));
        geometry_choices+=map.shape_decisions;
        int seen[AW_CELLS]={0};for(int c=0;c<AW_CELLS;c++){
            assert(map.order[c]>=0&&map.order[c]<AW_CELLS&&!seen[map.order[c]]++);
            AwCell*a=&map.cells[c];terrain_seen|=1<<a->material;
            if(o.biome!=AW_FROZEN)assert(a->material!=AW_SNOW&&a->material!=AW_ICE);
            if(o.biome!=AW_TEMPERATE)assert(a->material!=AW_GRASS&&a->material!=AW_FOREST&&a->material!=AW_MUD);
            for(int k=0;k<4;k++){
                int v=aw_corner_vertex(c,k),x=v%65,z=v/65;
                if(x<=AW_OCEAN_MARGIN||z<=AW_OCEAN_MARGIN||x>=64-AW_OCEAN_MARGIN||z>=64-AW_OCEAN_MARGIN)assert(a->q[k]==0);
                if(a->road)assert(!map.lake_mask[v]);
            }
            if(o.symmetry){AwCell*b=&map.cells[AW_CELLS-1-c];assert(a->material==b->material&&a->road==b->road&&a->tunnel==b->tunnel&&a->portal==b->portal);for(int k=0;k<4;k++)assert(a->q[k]==b->q[(k+2)%4]);}
            for(int d=0;d<4;d++){
                int next=aw_neighbor(c,d);if(next<0)continue;
                assert(aw_edge_matches(&map,c,next,d));
                if(i%16==0){
                    for(int sample=0;sample<=AW_SUBDIV;sample++){
                        float t=(float)sample/AW_SUBDIV;
                        float ax=d==1?1:d==3?0:t,az=d==0?0:d==2?1:t;
                        float bx=d==1?0:d==3?1:t,bz=d==0?1:d==2?0:t;
                        assert(fabsf(aw_surface_q(&map,c,ax,az)-aw_surface_q(&map,next,bx,bz))<0.00002f);
                    }
                }
            }
            if(a->road||map.cave_access[c]){assert(map.reachable[c]);if(map.cave_bin_count[c]){float q=aw_surface_q(&map,c,.5f,.5f);assert(fabsf(q-aw_support_q(&map,c%64+.5f,c/64+.5f,q))<.05f);}for(int k=0;k<4;k++)assert(aw_abs(a->q[k]-a->q[(k+1)%4])<=1);}
            if(a->tunnel)assert(o.tunnels);

        }
        if(o.tunnels){
            assert(map.cave_room_count==0||map.cave_room_count==2);
            for(int r=0;r<map.cave_room_count;r++){
                int n=map.cave_rooms[r];assert(n>=0&&n<map.cave_count&&map.cave[n].q<0&&map.cave[n].profile==2);
            }
            int a=map.cave_entrances[0],b=map.cave_entrances[1];
            assert(a>=0&&b>=0&&a!=b);
            AwCaveNode*pa=&map.cave[a],*pb=&map.cave[b];
            assert(pa->x>=35&&pa->z<=29&&pb->x<=28&&pb->z>=34);
            assert(aw_abs(pa->x-pb->x)+aw_abs(pa->z-pb->z)>=20);
            layouts+=!entrance_seen[pa->portal]++;depth_seen|=1<<(-map.cave[map.cave_hubs[0]].q);
            uint8_t surface[AW_CELLS];memcpy(surface,map.walkable,AW_CELLS);memset(map.walkable,0,AW_CELLS);
            assert(aw_find_path(&map,AW_CELLS+a,AW_CELLS+b));
            for(int k=0;k<map.path_length;k++)assert(map.path[k]>=AW_CELLS);
            memcpy(map.walkable,surface,AW_CELLS);assert(aw_find_path(&map,map.spawns[0],map.spawns[1]));
            if(o.symmetry)assert(pa->x+pb->x==63&&pa->z+pb->z==63);
            else independent+=pa->x+pb->x!=63||pa->z+pb->z!=63||map.cave[map.cave_hubs[0]].q!=map.cave[map.cave_hubs[1]].q;
            assert(!memcmp(map.cave,repeat.cave,sizeof(AwCaveNode)*map.cave_count));
        }
        for(int n=0;n<map.cave_count;n++){
            AwCaveNode*a=&map.cave[n];assert(map.reachable[AW_CELLS+n]);
            if(o.symmetry){int partner=-1;for(int j=0;j<map.cave_count;j++){AwCaveNode*b=&map.cave[j];if(a->x+b->x==63&&a->z+b->z==63&&a->q==b->q)partner=j;}assert(partner>=0&&a->profile==map.cave[partner].profile);
                for(int d=0;d<6;d++)if(a->links[d]>=0){
                    AwCaveNode*next=&map.cave[a->links[d]];int matched=0;
                    for(int e=0;e<6;e++)if(map.cave[partner].links[e]>=0){AwCaveNode*b=&map.cave[map.cave[partner].links[e]];matched|=next->x+b->x==63&&next->z+b->z==63&&next->q==b->q;}
                    assert(matched);
                }
            }
        }
        for(int n=0;n<AW_NODES;n++)for(int d=0;d<AW_LINKS;d++){int next=aw_open(&map,n,d);if(next>=0){assert(connected(next,n));assert(aw_move_cost(&map,n,next)>0);}}
        assert(map.path[0]==map.spawns[0]&&map.path[map.path_length-1]==map.spawns[1]);
        int cost=0;for(int p=1;p<map.path_length;p++){assert(connected(map.path[p-1],map.path[p]));cost+=aw_move_cost(&map,map.path[p-1],map.path[p]);}assert(cost==map.path_cost);
        assert(!aw_find_path(&map,-1,0));assert(!aw_find_path(&map,0,AW_NODES));assert(!aw_find_path(&map,0,map.spawns[0]));
        assert(aw_find_path(&map,map.spawns[0],map.spawns[0])&&map.path_length==1&&map.path_cost==0);
        if(map.structure_decisions>max_structure)max_structure=map.structure_decisions;
        digest=(digest^map.hash)*16777619u;
    }
    if(seeds>=200){assert(layouts>=10);assert(__builtin_popcount(depth_seen)>=4);assert(independent>=10);}
    assert(terrain_seen==AW_ALL);assert(max_structure>0&&geometry_choices>0);
    /* Retired mixed/volcanic and malformed IDs match the Temperate default. */
    assert(aw_generate(&repeat,73));
    assert(aw_defaults().biome==AW_TEMPERATE);
    for(int i=0;i<4;i++){
        AwOptions legacy=aw_defaults();legacy.biome=(int[]){0,4,-1,INT_MAX}[i];
        assert(aw_generate_options(&map,73,legacy));
        assert(map.options.biome==AW_TEMPERATE&&map.hash==repeat.hash);
        assert(!memcmp(map.cells,repeat.cells,sizeof(map.cells)));
    }
    assert(aw_generate(&map,73));
    /* Signed depths and independent spans, with field-derived clearance. */
    assert(map.cave_count>20&&map.cave_decisions>0&&map.cave_expanded>0);
    int deep=-1,ramps=0;
    for(int i=0;i<map.cave_count;i++)if(map.cave[i].q<0){deep=i;break;}
    assert(deep>=0);AwCaveNode*d=&map.cave[deep];float x=d->x+0.5f,z=d->z+0.5f,q=d->q;
    assert(!aw_solid(&map,x,q+1,z));assert(aw_solid(&map,x,q-1,z));
    assert(aw_occluded(&map,x,60,z,x,q+1,z));
    for(int i=0;i<map.cave_edge_count;i++)ramps+=map.cave[map.cave_edges[i].a].q!=map.cave[map.cave_edges[i].b].q;
    assert(ramps>=20);
    /* Lake interiors must remain enclosed; ocean sockets must survive even
     * when all copies of an edge vertex are edited consistently. */
    repeat=map;map.lakes[0].x=4;map.lakes[0].z=4;
    for(int c=0;c<AW_CELLS;c++)for(int k=0;k<4;k++){
        int v=aw_corner_vertex(c,k);if(v/65==4&&v%65<=4)map.cells[c].q[k]=0;
    }
    assert(!aw_lakes_valid(&map));map=repeat;
    for(int c=0;c<AW_CELLS;c++)for(int k=0;k<4;k++)if(aw_corner_vertex(c,k)==65)map.cells[c].q[k]=4;
    assert(!aw_validate(&map));map=repeat;
    map.cave_room_count=2;map.cave_rooms[0]=AW_CAVE_NODES;assert(!aw_validate(&map));map=repeat;
    /* No malformed ID may reach a palette or movement-cost lookup. */
    map.cells[0].material=255;assert(!aw_validate(&map));assert(aw_generate(&map,73));
    /* Missing entrance references and disconnected graph edges fail. */
    for(int i=0;i<map.cave_count;i++)if(map.cave[i].portal>=0)map.cells[map.cave[i].portal].portal=0;
    assert(!aw_validate(&map));assert(aw_generate(&map,73));
    map.cave[map.cave_edges[0].a].links[5]=-1;assert(!aw_validate(&map));
    assert(aw_generate(&map,73));map.cave[0].links[0]=AW_CAVE_NODES;assert(!aw_validate(&map));
    assert(aw_generate(&map,73));map.cave_entrances[0]=-1;assert(!aw_validate(&map));
    assert(aw_generate(&map,73));map.cave[map.cave_hubs[0]].profile=0;assert(!aw_validate(&map));
    /* Contradictory passage profiles cannot bypass socket propagation. */
    assert(aw_generate(&map,73));uint8_t profiles[AW_CAVE_NODES];memset(profiles,15,sizeof(profiles));
    profiles[map.cave_edges[0].a]=1;profiles[map.cave_edges[0].b]=8;assert(!aw_cave_propagate(&map,profiles));
    /* Incompatible road elevation sockets and terrain sockets reject explicitly. */
    uint64_t wave[2]={1ull<<40,1ull<<4};int ramp[2]={0,1},reductions=0;
    assert(!aw_road_propagate(wave,ramp,2,&reductions));
    assert(aw_generate(&map,73));map.wave[0]=1u<<AW_SAND;map.wave[1]=1u<<AW_SNOW;assert(!aw_propagate(&map,0));
    /* A mismatched ordinary terrain boundary is invalid even if bases connect. */
    assert(aw_generate(&map,73));map.cells[200].q[0]^=4;assert(!aw_validate(&map));
    /* Shape WFC must reject contradictory corner sockets. */
    memset(&map,0,sizeof(map));
    for(int c=0;c<AW_CELLS;c++){map.shape_wave[c]=1;for(int k=0;k<4;k++){map.shape_lo[c][k]=4;map.shape_hi[c][k]=8;}}
    map.shape_wave[0]=1u<<15;aw_shape_catalog(&map);assert(!aw_shape_propagate(&map,0));
    /* Cliff tiles retain shelves and a sloping continuation within the tile. */
    memset(&map,0,sizeof(map));map.cells[0].q[0]=map.cells[0].q[3]=4;map.cells[0].q[1]=map.cells[0].q[2]=8;
    assert(aw_tile_sample_q(&map,0,0.2f,0.5f)==4);assert(aw_tile_sample_q(&map,0,0.8f,0.5f)==8);
    assert(fabsf(aw_tile_sample_q(&map,0,0.5f,0.5f)-6)<0.00001f);
    /* Weighted navigation must choose a longer cheap road around expensive mud. */
    memset(&map,0,sizeof(map));int start=65,end=69;
    for(int z=1;z<=2;z++)for(int x=1;x<=5;x++){int c=z*64+x;aw_flat(&map.cells[c],4);map.cells[c].material=z==1&&x>1&&x<5?AW_MUD:AW_ROAD;map.walkable[c]=1;}
    assert(aw_find_path(&map,start,end));assert(map.path_cost==60&&map.path_length==7);
    printf("MAP_TEST version=%d seeds=%d digest=%08" PRIx32 " terrains=%d max_structure=%d geometry_choices=%d entrance_sites=%d depths=%d independent=%d PASS\n",AW_VERSION,seeds,digest,__builtin_popcount(terrain_seen),max_structure,geometry_choices,layouts,__builtin_popcount(depth_seen),independent);
    return 0;
}
