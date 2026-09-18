#include <assert.h>
#include <stdio.h>
#include "ocean/alienwars/map.h"
static AwMap map,original;
static void vertex(int x,int z,int q){
    for(int c=0;c<AW_CELLS;c++)for(int k=0;k<4;k++)
        if(aw_corner_vertex(c,k)==z*AW_VERT+x)map.cells[c].q[k]=q;
}
static void seam(float x,float z,int axis){
    float a=aw_ocean_bed_q(&map,x,z),b=aw_height_q(&map,x,z);
    assert(fabsf(a-b)<.00002f);
    float e=.0001f;
    assert(fabsf(aw_ocean_bed_q(&map,x+(axis==0?e:0),z+(axis==1?e:0))-
                 aw_ocean_bed_q(&map,x-(axis==0?e:0),z-(axis==1?e:0)))<.001f);
}
int main(void){
    /* A point coast gives exact 3-4-5 distance equality. A Manhattan or
     * square-domain shelf fails this even if a screenshot looks plausible. */
    vertex(32,32,4);map.rng=177;aw_bathymetry_build(&map);assert(map.rng==177);
    assert(aw_ocean_bed_q(&map,37,32)==aw_ocean_bed_q(&map,35,36));
    assert(aw_ocean_bed_q(&map,37,32)>aw_ocean_bed_q(&map,40,32));
    assert(aw_ocean_bed_q(&map,0,0)==-12); /* Old square interior was flat q=0. */
    for(int i=0;i<AW_OCEAN_VERT*AW_OCEAN_VERT;i++)assert(map.shelf_drop[i]==map.shelf_drop[AW_OCEAN_VERT*AW_OCEAN_VERT-1-i]);
    /* An oval with an enclosed lake and low wet beach sockets. */
    memset(&map,0,sizeof(map));memset(map.blend,255,sizeof(map.blend));
    for(int z=20;z<=44;z++)for(int x=18;x<=46;x++){
        float r=(x-32)*(x-32)/100.0f+(z-32)*(z-32)/36.0f;
        if(r<=1.35f)vertex(x,z,r<=1?4:1);
    }
    for(int z=30;z<=34;z++)for(int x=30;x<=34;x++)vertex(x,z,0);
    original=map;aw_bathymetry_build(&map);aw_ocean_build(&map);
    assert(aw_ocean_bed_q(&map,32,32)==0);
    assert(!map.ocean_connected[(32+16)*96+32+16]);
    assert(aw_ocean_bed_q(&map,47,32)>aw_ocean_bed_q(&map,32,47));
    for(int z=20;z<44;z++)for(int x=18;x<46;x++)for(int k=0;k<4;k++){
        float px=x+(k&1)*.5f,pz=z+(k>>1)*.5f,q=aw_height_q(&original,px,pz);
        if(q>=1)assert(q==aw_height_q(&map,px,pz));
    }
    /* Put coast close to a logical edge so the seam crosses a sloped shelf. */
    memset(&map,0,sizeof(map));vertex(4,32,4);aw_bathymetry_build(&map);
    for(int j=0;j<=AW_SIZE*AW_SUBDIV;j++){
        float t=(float)j/AW_SUBDIV;seam(0,t,0);seam(64,t,0);seam(t,0,1);seam(t,64,1);
    }
    for(int j=0;j<100;j++){
        float x=1.1f+j*.021f,z=28.1f+j*.017f,bed=aw_ocean_bed_q(&map,x,z);
        assert(fabsf(aw_density(&map,x,bed,z))<.0001f);
        assert(aw_density(&map,x,bed-.1f,z)>0&&aw_density(&map,x,bed+.1f,z)<0);
        /* Outer ocean triangles use the same diagonal and exact vertex beds. */
        x=-.8f+j*.001f;z=29.2f+j*.004f;int ix=(int)floorf(x),iz=(int)floorf(z);
        float u=x-ix,v=z-iz,a=aw_ocean_bed_q(&map,ix,iz),b=aw_ocean_bed_q(&map,ix+1,iz),c=aw_ocean_bed_q(&map,ix+1,iz+1),d=aw_ocean_bed_q(&map,ix,iz+1);
        float mesh=u>=v?a+(b-a)*u+(c-b)*v:a+(c-d)*u+(d-a)*v;
        assert(fabsf(mesh-aw_ocean_bed_q(&map,x,z))<.00002f);
    }
    printf("BATHYMETRY_TEST coast_distance=PASS ellipse=PASS lakes=PASS dry_shore=PASS symmetry=PASS boundary_seams=PASS mesh_density=PASS rng_unchanged=PASS\n");
}
