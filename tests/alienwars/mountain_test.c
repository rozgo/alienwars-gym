#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/map.h"
static AwMap map;
static void line(int z,int q,int profile,int branch){
    int first=map.trail_count;
    for(int x=10;x<=20;x++){
        int i=map.trail_count++;map.trail[i]=(AwTrailNode){.x=x,.z=z,.q=q,.profile=profile,.mode=branch?AW_TRAIL_CUT:AW_TRAIL_COVERED};
        if(i>first)map.trail_edges[map.trail_edge_count++]=(AwTrailEdge){i-1,i,0,branch};
    }
}
int main(void){
    /* Clearance is measured from actual volume, not a width label. Two
     * underground floors in the same column must not acquire a phantom link. */
    for(int c=0;c<AW_CELLS;c++)aw_flat(&map.cells[c],24);
    line(10,4,0,0);line(10,12,0,0);line(15,4,3,1);line(20,4,7,0);
    assert(aw_mountain_index(&map)&&aw_traversal_build(&map));
    int low=aw_span_find(&map,10*64+15,4),high=aw_span_find(&map,10*64+15,12),wide=aw_span_find(&map,15*64+15,4);
    assert(low>=0&&high>=0&&wide>=0&&low!=high);
    assert(map.spans[low].fits==1&&map.spans[high].fits==1&&map.spans[wide].fits==3);
    assert(aw_span_find(&map,21*64+15,4)>=0); /* Walk across a chamber, off its centerline. */
    for(int d=0;d<4;d++)if(map.spans[low].links[d]>=0)assert(map.spans[map.spans[low].links[d]].q<8);
    assert(aw_density(&map,15.5f,6,10.5f)<0&&aw_density(&map,15.5f,10,10.5f)>0);
    assert(aw_density(&map,15.5f,20,15.5f)<0); /* Open cutting has no cap. */
    assert(!aw_trail_compatible(0,7)&&aw_trail_compatible(3,7));
    uint8_t impossible[2]={1,128};assert(!aw_trail_profiles(impossible,2));
    uint16_t grades[3]={1<<4,1<<8,1<<4};uint8_t ramp[3]={0,1,1};assert(!aw_trail_grades(grades,ramp,3));
    uint16_t sockets[25];for(int i=0;i<25;i++)sockets[i]=1;
    sockets[5]=1<<3;sockets[9]=1<<9;assert(!aw_mountain_propagate(sockets,5,9));

    uint32_t digest=2166136261u,topologies[32];int shapes=0,raised=0,multiple=0,normal=0,min_covered=1000,min_large=1000;
    for(int sym=0;sym<2;sym++)for(int i=0;i<16;i++){
        uint32_t seed=i<4?(uint32_t[]){0,1,73,UINT32_MAX}[i]:aw_hash(i);
        assert(aw_generate_options(&map,seed,(AwOptions){sym,6,6,1,1}));
        assert(aw_mountain_validate(&map)&&map.span_count>map.cave_count);
        const AwMountain*r=&map.mountains[0];normal+=r->step>=3;
        uint32_t topology=2166136261u;for(int c=0;c<25;c++)topology=(topology^r->sockets[c])*16777619u;
        topologies[sym*16+i]=topology;
        int covered=0,runs=0,prev=0,large=0;
        for(int branch=0;branch<2;branch++){
            int earlier=4;
            for(int j=0;j<r->trail_length[branch];j++){
                int id=r->trail[branch][j],span=map.trail_span[id];const AwTrailNode*n=&map.trail[id];
                assert(n->profile<8&&span>=0&&map.reachable[AW_SPAN_START+span]);shapes|=1<<n->profile;
                if(j){
                    assert(aw_abs(n->q-earlier)<=1);raised+=n->q!=earlier;
                    int previous=map.trail_span[r->trail[branch][j-1]],large_edge=0;
                    for(int d=0;d<4;d++)if(map.spans[previous].links[d]==span)large_edge=map.spans[previous].edge_fits[d]&2;
                    if(branch)assert(large_edge);
                }
                earlier=n->q;
                if(branch){large+=!!(map.spans[span].fits&2);continue;}
                int roof=0;
                for(float q=n->q+3;q<aw_height_q(&map,n->x+.5f,n->z+.5f)+1;q+=.5f)
                    roof|=aw_density(&map,n->x+.5f,q,n->z+.5f)>0;
                covered+=roof;runs+=roof&&!prev;prev=roof;
            }
        }
        if(covered<min_covered)min_covered=covered;
        int fraction=large*1000/r->trail_length[1];if(fraction<min_large)min_large=fraction;
        multiple+=runs>=2;
        if(sym)for(int b=0;b<2;b++)for(int j=0;j<r->trail_length[b];j++){
            const AwTrailNode*a=&map.trail[r->trail[b][j]],*z=&map.trail[map.mountains[1].trail[b][j]];
            assert(a->x+z->x==63&&a->z+z->z==63&&a->q==z->q&&a->profile==z->profile&&a->mode==z->mode);
        }
        digest=(digest^map.hash)*16777619u;
        if((i+1)%8==0)fprintf(stderr,"mountain %d/32\n",sym*16+i+1);
    }
    int distinct=0;for(int i=0;i<32;i++){int seen=0;for(int j=0;j<i;j++)seen|=topologies[i]==topologies[j];distinct+=!seen;}
    assert(distinct>=20&&__builtin_popcount((unsigned)shapes)>=6&&raised>=16&&min_covered>=4&&multiple>=8&&min_large==1000);
    printf("MOUNTAIN_TEST version=%d worlds=32 digest=%08" PRIx32 " topologies=%d profiles=%d graded_edges=%d multiple_tunnels=%d full_regions=%d min_covered=%d min_large_permille=%d stacked_spans=PASS clearance=PASS contradictions=PASS\n",AW_VERSION,digest,distinct,__builtin_popcount((unsigned)shapes),raised,multiple,normal,min_covered,min_large);
}
