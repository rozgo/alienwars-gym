#include "src/puffercpu.c"
static int allocations,guard;
static void*checked_malloc(size_t n){allocations+=guard;return malloc(n);}
static void*checked_calloc(size_t n,size_t s){allocations+=guard;return calloc(n,s);}
static void*checked_realloc(void*p,size_t n){allocations+=guard;return realloc(p,n);}
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#include "ocean/alienwars_local/local_api.c"
#include "ocean/alienwars_local/alienwars_local.h"
int main(void){
    Dict settings={0};dict_set(&settings,"family",2);dict_set(&settings,"maps",1);dict_set(&settings,"map_seed",73);dict_set(&settings,"difficulty",1);
    Env env={0};puf_init(&env,&settings);
    float obs[OBS_SIZE],actions[4]={1,1,1,1},reward=99,terminal=99;
    env.agents[0].observations=obs;env.agents[0].actions=actions;env.agents[0].rewards=&reward;env.agents[0].terminals=&terminal;
    guard=1;puf_reset(&env);assert(reward==0&&terminal==0);env.task->episode.limit=2;
    puf_step(&env);assert(terminal==0);puf_step(&env);assert(terminal==1&&env.log.n==1);
    assert(env.task->episode.ticks==0&&env.steps==0);
    puf_step(&env);assert(terminal==0);
    actions[0]=NAN;actions[1]=INFINITY;actions[2]=-3;actions[3]=1.5f;puf_step(&env);
    assert(env.task->episode.invalid==4);
    for(int i=0;i<500;i++){aw_local_baseline_task(env.task,actions,1);puf_step(&env);assert(isfinite(reward));for(int j=0;j<OBS_SIZE;j++)assert(isfinite(obs[j])&&fabsf(obs[j])<=1);}
    for(int i=0;i<8;i++){puf_reset(&env);assert(aw_vehicle_clear(&env.task->world->map,&env.task->episode.vehicle));for(int j=0;j<4;j++)assert(!aw_bodies_overlap(aw_vehicle_body(&env.task->episode.vehicle),env.task->peers[j],0));}
    assert(allocations==0);guard=0;
    int hidden=8,layers=2,n=hidden*OBS_SIZE+13*hidden+layers*3*hidden*hidden;
    Weights*w=calloc(1,sizeof(*w)+n*sizeof(float));w->data=(float*)(w+1);w->size=n;for(int i=0;i<n;i++)w->data[i]=sinf(i*.7f)*.02f;
    int sizes[]=ACT_SIZES;PufferNet*a=make_puffernet(w,1,OBS_SIZE,hidden,layers,sizes,4);w->idx=0;PufferNet*b=make_puffernet(w,1,OBS_SIZE,hidden,layers,sizes,4);
    terminal=0;for(int i=0;i<7;i++)forward_puffernet(a,obs,actions,NULL,&terminal);terminal=1;forward_puffernet(a,obs,actions,NULL,&terminal);forward_puffernet(b,obs,actions,NULL,&terminal);
    assert(!memcmp(a->decoder->output,b->decoder->output,13*sizeof(float)));
    free_puffernet(a);free_puffernet(b);free(w);puf_close(&env);dict_clear(&settings);
    puts("LOCAL_ADAPTER terminal_pulse=PASS autoreset=PASS reward_clear=PASS recurrent_reset=PASS zero_step_reset_allocations=PASS finite_invalid_actions=PASS valid_spawn=PASS");return 0;
}
