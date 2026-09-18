/* Ownership, independent worlds, stable slots and zero-allocation reset/tick. */
#include "ocean/alienwars/missions.h"
#include <stddef.h>
#include <stdio.h>
#include <time.h>

typedef union { max_align_t alignment; struct {size_t bytes;} info; } Allocation;
static size_t live_bytes,peak_bytes,allocation_calls;
static void* tracked_malloc(ecs_size_t n){
    assert(n>=0);Allocation*p=malloc(sizeof(*p)+(size_t)n);assert(p);
    p->info.bytes=(size_t)n;live_bytes+=(size_t)n;allocation_calls++;
    if(live_bytes>peak_bytes)peak_bytes=live_bytes;return p+1;
}
static void tracked_free(void*ptr){
    if(!ptr)return;Allocation*p=(Allocation*)ptr-1;live_bytes-=p->info.bytes;free(p);
}
static void* tracked_calloc(ecs_size_t n){void*p=tracked_malloc(n);memset(p,0,(size_t)n);return p;}
static void* tracked_realloc(void*ptr,ecs_size_t n){
    if(!ptr)return tracked_malloc(n);if(!n){tracked_free(ptr);return NULL;}
    size_t old=((Allocation*)ptr-1)->info.bytes;void*out=tracked_malloc(n);
    memcpy(out,ptr,old<(size_t)n?old:(size_t)n);tracked_free(ptr);return out;
}
static char* tracked_strdup(const char*str){if(!str)return NULL;size_t n=strlen(str)+1;char*out=tracked_malloc((ecs_size_t)n);memcpy(out,str,n);return out;}
static AwMap map;
static AwMissionWorld worlds[24];

int main(void){
    ecs_os_set_api_defaults();ecs_os_api_t api=ecs_os_get_api();
    api.malloc_=tracked_malloc;api.calloc_=tracked_calloc;api.realloc_=tracked_realloc;
    api.free_=tracked_free;api.strdup_=tracked_strdup;ecs_os_set_api(&api);
    /* No map generation cost in this storage/reset fixture. Active movement
     * and perception are covered by the shared trace and adapter tests. */
    for(int i=0;i<AW_CELLS;i++)aw_flat(&map.cells[i],4);
    for(int n=0;n<24;n++){
        AwMissionWorld*w=&worlds[n];assert(aw_mission_world_init(w));aw_mission_world_reset(w,&map,12);
        for(int i=0;i<16;i++){
            assert(ecs_is_alive(w->ecs,w->entities[i]));
            assert(ecs_get_id(w->ecs,w->entities[i],w->mission_id)==&w->agents[i]);
            assert(ecs_get_id(w->ecs,w->entities[i],w->sensor_id)==&w->sensors.units[i]);
            assert(ecs_get_id(w->ecs,w->entities[i],w->active_id)==&w->active[i]);
            assert(ecs_get_id(w->ecs,w->entities[i],w->paused_id)==&w->paused[i]);
        }
        if(n){assert(w->agents!=worlds[n-1].agents);assert(w->perception!=worlds[n-1].perception);}
        w->agents[0].total=(float)(n+1);w->perception[0].odometry.distance=(float)n;
    }
    for(int n=0;n<24;n++)assert(worlds[n].agents[0].total==n+1);
    size_t allocated=live_bytes,allocations=allocation_calls;clock_t start=clock();
    float actions[16][4]={{0}};
    for(int round=0;round<40;round++)for(int n=0;n<24;n++){
        AwMissionWorld*w=&worlds[n];AwMissionAgent*agents=w->agents;AwSensorUnit*sensors=w->perception;
        ecs_entity_t entity=w->entities[0];aw_mission_world_reset(w,&map,12);
        assert(agents==w->agents&&sensors==w->sensors.units&&entity==w->entities[0]);
        assert(w->agents[0].total==0&&w->sensors.units[0].odometry.distance==0&&w->ticks==0);
        aw_mission_tick(w,&map,actions);assert(w->ticks==1);
    }
    assert(allocation_calls==allocations);double elapsed=(clock()-start)/(double)CLOCKS_PER_SEC;
    for(int n=0;n<24;n++){aw_mission_world_close(&worlds[n]);aw_mission_world_close(&worlds[n]);}
    assert(live_bytes==0);
    puts("FLECS_WORLD worlds=24 slots=16 component_ownership=PASS independent_worlds=PASS stable_slots=PASS no_step_reset_allocations=PASS teardown=PASS");
    fprintf(stderr,"FLECS_MEMORY world_struct=%zu heap_per_world=%zu peak_heap=%zu reset_tick_seconds=%.6f\n",
        sizeof(AwMissionWorld),allocated/24,peak_bytes,elapsed);
}
