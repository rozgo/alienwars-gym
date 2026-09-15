#include <assert.h>
#include <inttypes.h>
#include "ocean/alienwars/volume.h"
static AwMap m;
typedef struct {int v[6],count,orientation;} Edge;
#define TABLE (1<<20)
static Edge edges[TABLE];static int triangles,edge_count;
static int cmp(const int*a,const int*b){for(int i=0;i<3;i++)if(a[i]!=b[i])return a[i]<b[i]?-1:1;return 0;}
static void edge(AwVolumePoint a,AwVolumePoint b){
    int va[3]={(int)lroundf(a.x*10000),(int)lroundf(a.q*10000),(int)lroundf(a.z*10000)};
    int vb[3]={(int)lroundf(b.x*10000),(int)lroundf(b.q*10000),(int)lroundf(b.z*10000)},key[6];
    int sign=cmp(va,vb)<0?1:-1;
    memcpy(key,sign>0?va:vb,3*sizeof(int));memcpy(key+3,sign>0?vb:va,3*sizeof(int));
    uint32_t h=2166136261u;for(int i=0;i<6;i++)h=(h^(uint32_t)key[i])*16777619u;h&=TABLE-1;
    while(edges[h].count&&memcmp(edges[h].v,key,sizeof(key)))h=(h+1)&(TABLE-1);
    if(!edges[h].count){memcpy(edges[h].v,key,sizeof(key));edge_count++;}
    edges[h].count++;edges[h].orientation+=sign;
}
static void triangle(void*ctx,AwVolumePoint a,AwVolumePoint b,AwVolumePoint c){
    (void)ctx;assert(isfinite(a.x)&&isfinite(b.q)&&isfinite(c.z));
    float x=(a.x+b.x+c.x)/3,q=(a.q+b.q+c.q)/3,z=(a.z+b.z+c.z)/3;
    assert(fabsf(aw_density(&m,x,q,z))<0.0002f);
    edge(a,b);edge(b,c);edge(c,a);triangles++;
}
static void line(int x,int z,int q,int dx,int dz,int n){
    int prev=-1;
    for(int i=0;i<n;i++){int node=aw_cave_node(&m,x+i*dx,z+i*dz,q);assert(node>=0);m.cave[node].profile=1;if(prev>=0)assert(aw_cave_link(&m,prev,node));prev=node;}
}
int main(void){
    for(int c=0;c<AW_CELLS;c++)aw_flat(&m.cells[c],20);
    /* Three independent empty spans occupy the same x/z column. One is below
     * datum; the other two are inside a mountain. Rock must separate all three. */
    line(8,12,-6,1,0,9);line(12,8,4,0,1,9);line(8,12,12,1,0,9);assert(aw_cave_index(&m));
    for(int i=0;i<3;i++){float q=(float[]){-6,4,12}[i];assert(aw_density(&m,12.5f,q+1,12.5f)<0);assert(aw_density(&m,12.5f,q-0.5f,12.5f)>0);}
    assert(aw_density(&m,12.5f,1,12.5f)>0);assert(aw_density(&m,12.5f,10,12.5f)>0);assert(aw_density(&m,12.5f,18,12.5f)>0);
    int total_triangles=0,total_edges=0,total_boundary=0;
    for(int breach=0;breach<3;breach++){
        memset(edges,0,sizeof(edges));triangles=edge_count=0;
        for(int c=0;c<AW_CELLS;c++)aw_flat(&m.cells[c],breach==1?14:20);
        if(breach==2){
            /* New arched sweeps: a graded passage meeting an open cutting,
             * with a separate crossing above it and the older caves below. */
            for(int route=0;route<3;route++)for(int j=0;j<9;j++){
                int i=m.trail_count++;
                m.trail[i]=(AwTrailNode){.x=route==2?10:8+j,.z=route==2?8+j:8+route*3,
                    .q=route==2?12:4+(j>2&&j<6?j-2:j>=6?3:0),.profile=route==2?1:3};
                if(j)m.trail_edges[m.trail_edge_count++]=(AwTrailEdge){i-1,i,0,route==1};
            }
            assert(aw_mountain_index(&m));
        }
        for(int z=6;z<19;z++)for(int x=6;x<19;x++)aw_volume_cell(&m,z*64+x,triangle,NULL);
        int boundary=0;
        for(int i=0;i<TABLE;i++)if(edges[i].count){
            Edge*e=&edges[i];int outer=(e->v[0]==60000&&e->v[3]==60000)||(e->v[0]==190000&&e->v[3]==190000)||(e->v[2]==60000&&e->v[5]==60000)||(e->v[2]==190000&&e->v[5]==190000);
            if(outer){assert(e->count==1);boundary++;}
            else if(e->count!=2||e->orientation){fprintf(stderr,"Nonmanifold edge count=%d orientation=%d (%d,%d,%d)-(%d,%d,%d)\n",e->count,e->orientation,e->v[0],e->v[1],e->v[2],e->v[3],e->v[4],e->v[5]);return 1;}
        }
        assert(triangles>10000&&boundary>0);
        total_triangles+=triangles;total_edges+=edge_count;total_boundary+=boundary;
    }

    /* A surface breach is a true absence of rock, not a hidden roof mesh. */
    for(int c=0;c<AW_CELLS;c++)aw_flat(&m.cells[c],14);
    assert(aw_density(&m,12.5f,14,12.5f)<=0);assert(aw_density(&m,12.5f,13,12.5f)<0);
    printf("VOLUME_TEST spans=3 fixtures=3 mountain_joins=PASS triangles=%d edges=%d boundary=%d manifold=PASS\n",total_triangles,total_edges,total_boundary);
}
