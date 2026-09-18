/* Behavioral trace shared by the pre-port snapshot and the Flecs runtime.
 * Hash explicit numeric fields, never addresses or struct padding. */
#include "ocean/alienwars_shared/shared_api.c"
#include <assert.h>
#include <stdint.h>
static uint32_t trace=2166136261u;
static void integer(int value){trace=(trace^(uint32_t)value)*16777619u;}
static void scalar(float value){assert(isfinite(value));integer((int)roundf(value*100000));}
int main(void){
    AwSharedTask *tasks[2];
    for(int n=0;n<2;n++){tasks[n]=aw_shared_create(1,301,901+n,0);assert(tasks[n]);aw_shared_reset(tasks[n]);}
    clock_t start=clock();int resets=0;
    for(int step=0;step<600;step++)for(int n=0;n<2;n++){
        AwSharedTask*t=tasks[n];
        if(step%113==0){aw_shared_reset(t);resets++;}
        for(int i=0;i<12;i++){
            aw_shared_reference(t,i,t->actions[i],step%19<4);
            if(step%113==0)t->world.agents[i].limit=47+i;
        }
        aw_shared_step(t);
        integer(n);integer(step);integer(t->world.ticks);
        for(int i=0;i<12;i++){
            AwMissionAgent*a=&t->world.agents[i];AwVehicle*v=&a->vehicle;
            integer(t->world.active[i]);integer(t->terminal[i]);integer(t->event[i]);
            integer(a->arrived);integer(a->contacts);integer(a->blocked_total);integer(v->failed);
            scalar(v->position.x);scalar(v->position.y);scalar(v->position.z);
            scalar(v->velocity.x);scalar(v->velocity.y);scalar(v->velocity.z);
            scalar(v->yaw);scalar(v->pitch);scalar(a->reward);
            for(int j=0;j<AW_SHARED_OBS;j++)scalar(a->observation[j]);
        }
    }
    double elapsed=(clock()-start)/(double)CLOCKS_PER_SEC;
    for(int n=0;n<2;n++)aw_shared_destroy(tasks[n]);
    printf("TRACE worlds=2 steps=1200 resets=%d digest=%08x\n",resets,trace);
    fprintf(stderr,"TRACE_TIME seconds=%.6f world_steps_per_second=%.3f\n",elapsed,1200/elapsed);
}
