#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
static int forbid_alloc;
static void *guard_malloc(size_t n){assert(!forbid_alloc);return malloc(n);}
static void *guard_calloc(size_t n,size_t s){assert(!forbid_alloc);return calloc(n,s);}
static void *guard_realloc(void *p,size_t n){assert(!forbid_alloc);return realloc(p,n);}
#define malloc guard_malloc
#define calloc guard_calloc
#define realloc guard_realloc
#include "ocean/alienwars/nav_core.h"
static AwNavWorld fixture;
static AwNavEpisode episode,copy;
static uint32_t world_digest(const AwNavWorld *w){
    uint32_t h=2166136261u;const unsigned char *p=(const unsigned char*)w;
    for(size_t i=0;i<sizeof(*w);i++)h=(h^p[i])*16777619u;return h;
}
static void flat(int q){
    memset(&fixture,0,sizeof(fixture));AwMap *m=&fixture.map;
    memset(m->blend,255,sizeof(m->blend));memset(m->span_first,255,sizeof(m->span_first));
    for(int i=0;i<AW_CELLS;i++)aw_flat(&m->cells[i],q);
    m->spawns[0]=12*64+10;aw_navigation(m);aw_nav_graph(&fixture);
    aw_ray_world_init(&fixture.rays,m);
    assert(aw_nav_add_task(&fixture,12*64+10,12*64+14,AW_NAV_SURFACE));
}
static void finite_obs(const AwNavEpisode *e){for(int i=0;i<AW_NAV_OBS;i++)assert(isfinite(e->observations[i])&&fabsf(e->observations[i])<=1.00001f);}
static void fixtures(void){
    flat(4);aw_nav_reset(&episode,&fixture,0,AW_MOTION_PI*.5f,600);
    copy=episode;forbid_alloc=1;
    for(int i=0;i<60&&!episode.result;i++){
        aw_nav_step(&episode,2,1);aw_nav_step(&copy,2,1);finite_obs(&episode);
        assert(!memcmp(episode.observations,copy.observations,sizeof(episode.observations)));
        assert(episode.unit.motion.yaw_rate<=2.00001f);
    }
    assert(episode.result==AW_NAV_SUCCESS&&episode.ticks<40&&episode.event_reward==1);
    aw_nav_reset(&episode,&fixture,0,0,3);
    assert(episode.ticks==0&&episode.unit.speed==0&&episode.sensors.units[0].odometry.distance==0);
    for(int i=0;i<3;i++)aw_nav_step(&episode,1,1);
    assert(episode.result==AW_NAV_TIMEOUT&&episode.event_reward==0);
    aw_nav_reset(&episode,&fixture,0,0,600);aw_nav_step(&episode,NAN,5);
    assert(episode.invalid_actions==1&&episode.unit.speed==0);finite_obs(&episode);
    forbid_alloc=0;
    /* Wall entry must block rather than tunnel through the shared solid. */
    for(int z=0;z<64;z++)for(int x=13;x<64;x++)aw_flat(&fixture.map.cells[z*64+x],24);
    AwGroundUnit unit={.x=23,.z=25,.q=4,.motion={.yaw=AW_MOTION_PI*.5f,.initialized=1}};
    for(int i=0;i<90;i++)aw_ground_step(&fixture.map,&unit,2,1,1.0f/30);
    assert(unit.contact&&!unit.failed&&unit.x<26);
    /* A step over a cliff fails, and cannot snap to the much lower floor. */
    flat(4);for(int z=0;z<64;z++)for(int x=13;x<64;x++)aw_flat(&fixture.map.cells[z*64+x],0);
    unit=(AwGroundUnit){.x=26.1f,.z=25,.q=4,.speed=3,.motion={.yaw=AW_MOTION_PI*.5f,.initialized=1}};
    aw_ground_step(&fixture.map,&unit,2,1,1.0f/30);assert(unit.failed&&unit.q==4);
    /* An upper deck floor stays separate from the water bed below. */
    flat(4);fixture.map.bridge_count=1;fixture.map.bridges[0]=(AwBridge){8,12,1,0,10,20,20,22};
    assert(aw_bridge_index(&fixture.map));
    unit=(AwGroundUnit){.x=25,.z=25,.q=22,.motion={.yaw=AW_MOTION_PI*.5f,.initialized=1}};
    for(int i=0;i<30;i++)aw_ground_step(&fixture.map,&unit,2,1,1.0f/30);
    assert(!unit.failed&&!unit.contact&&unit.q>20);
    printf("NAV_FIXTURES observations=%d finite=PASS reset=PASS deterministic=PASS actions=PASS success=PASS timeout=PASS wall=PASS cliff=PASS stacked_bridge=PASS no_step_alloc=PASS\n",AW_NAV_OBS);
}
int main(int argc,char **argv){
    fixtures();
    if(argc>1&&strcmp(argv[1],"--fixtures")==0)return 0;
    AwNavWorld *w=calloc(1,sizeof(*w));assert(w);
    assert(aw_nav_world_init(w,72));int counts[3]={0},wins[3]={0},falls=0,contacts=0;
    uint32_t rng=173,digest=2166136261u;
    clock_t began=clock();int steps=0;
    for(int k=0;k<w->task_count;k++){
        AwNavTask *t=&w->tasks[k];counts[t->kind]++;
        aw_nav_reset(&episode,w,k,0,600);uint32_t hash=world_digest(w);
        forbid_alloc=1;
        while(!episode.result){
            float actions[2];aw_nav_baseline(&episode,3,&rng,actions);aw_nav_step(&episode,actions[0],actions[1]);steps++;finite_obs(&episode);
        }
        forbid_alloc=0;assert(world_digest(w)==hash);
        if(episode.result!=AW_NAV_SUCCESS)fprintf(stderr,"NAV_FAILURE task=%d kind=%d start=%d goal=%d pos=%.3f,%.3f,%.3f goalpos=%.3f,%.3f distance=%.3f contacts=%d\n",k,t->kind,t->start,t->goal,episode.unit.x,episode.unit.q,episode.unit.z,w->positions[t->goal].x,w->positions[t->goal].z,episode.distance,episode.contacts);
        wins[t->kind]+=episode.result==AW_NAV_SUCCESS;falls+=episode.result==AW_NAV_FALL;contacts+=episode.contacts;
        digest=(digest^(uint32_t)(episode.result+k*4))*16777619u;
    }
    printf("NAV_WORLD seed=72 tasks=%d surface=%d/%d bridge=%d/%d tunnel=%d/%d falls=%d contacts=%d digest=%08" PRIx32 " steps=%d\n",
        w->task_count,wins[0],counts[0],wins[1],counts[1],wins[2],counts[2],falls,contacts,digest,steps);
    fprintf(stderr,"NAV_BENCH steps=%d cpu_seconds=%.3f\n",steps,(clock()-began)/(double)CLOCKS_PER_SEC);
    assert(counts[0]&&counts[1]&&counts[2]);
    assert(wins[0]>0&&wins[1]>0&&wins[2]>0);
    free(w);return 0;
}
