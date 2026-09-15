#include <assert.h>
#include "ocean/alienwars/occlusion.h"
static AwMap m;
static AwOcclusion a;
static void flat(int q){for(int c=0;c<AW_CELLS;c++)aw_flat(&m.cells[c],q);}
static void passage(int q){
    int prev=-1;
    for(int x=8;x<=16;x++){
        int n=aw_cave_node(&m,x,12,q);assert(n>=0);m.cave[n].profile=1;
        if(prev>=0)assert(aw_cave_link(&m,prev,n));prev=n;
    }
}
int main(void){
    flat(4);aw_ao_init(&a,&m);
    float open=aw_ao_horizon(&a,64,1.8f,64,0,1,0);assert(open<.015f);
    aw_ao_contact_add(&a,64,1.8f,64,1,1,1,.8f);
    float contact=aw_ao_contact(&a,64,1.8f,64);
    assert(contact>.79f&&aw_ao_contact(&a,65,1.8f,64)==0);
    assert(aw_ao_contact(&a,64,4.8f,64)==0&&aw_ao_contact(&a,64,-1.2f,64)==0);
    assert(aw_ao_contact(&a,64.4f,1.8f,64)>aw_ao_contact(&a,64.8f,1.8f,64));
    /* Accessibility changes, while source map/nav storage is never mutated. */
    static AwMap before;memcpy(&before,&m,sizeof(m));
    float baked=aw_ao_sample(&a,64,1.8f,64,0,1,0);
    assert(baked>=0&&baked<.22f&&aw_ao_sample(&a,64,1.8f,64,0,1,0)==baked);
    assert(!memcmp(&m,&before,sizeof(m)));aw_ao_free(&a);

    /* A nearby cliff shades the foot, not its exposed top; far-field falloff. */
    for(int z=0;z<64;z++)for(int x=0;x<64;x++)aw_flat(&m.cells[z*64+x],x<32?16:4);
    aw_ao_init(&a,&m);
    float foot=aw_ao_horizon(&a,65,1.8f,64,0,1,0),top=aw_ao_horizon(&a,63,10.8f,64,0,1,0);
    float far=aw_ao_horizon(&a,90,1.8f,64,0,1,0);
    assert(foot>open+.10f&&top<.02f&&far<.02f);aw_ao_free(&a);

    flat(24);passage(-6);passage(6);assert(aw_cave_index(&m));aw_ao_init(&a,&m);
    /* Cache interpolation must agree with the mesher/collision field, also
     * for signed depths, surfaces, arbitrary tetrahedra and map boundaries. */
    for(int i=0;i<3000;i++){
        float x=7+(aw_hash(i)%11000)*.001f,z=10+(aw_hash(i+4000)%5000)*.001f,q=-8+(aw_hash(i+8000)%34000)*.001f;
        float cached=aw_ao_density(&a,x*2,q*.75f-1.2f,z*2),exact=aw_density(&m,x,q,z);
        assert(fabsf(cached-exact)<.0001f);
    }
    assert(aw_ao_density(&a,-.001f,0,10)<0&&aw_ao_density(&a,128,0,10)<0);
    float deep=aw_ao_cavity(&a,25,-5.7f,25,0,1,0);
    float upper=aw_ao_cavity(&a,25,3.3f,25,0,1,0);
    float ceiling=aw_ao_cavity(&a,25,6.65f,25,0,-1,0);
    assert(deep>.2f&&upper>.2f&&ceiling>.15f&&fabsf(deep-upper)<.02f);
    /* Rotating the receiver and normal by 180 degrees preserves the bake. */
    float left=aw_ao_cavity(&a,24.2f,3.3f,25,0,1,0),right=aw_ao_cavity(&a,25.8f,3.3f,25,0,1,0);
    assert(fabsf(left-right)<.015f);aw_ao_free(&a);
    flat(8);aw_ao_init(&a,&m);
    float breach=aw_ao_cavity(&a,25,3.3f,25,0,1,0);
    assert(breach<upper-.1f);
    aw_ao_free(&a);
    printf("OCCLUSION_TEST open/foot/top, contacts/floor separation, density parity=3000, stacked caves/ceiling/breach, symmetry/cache PASS\n");
}
