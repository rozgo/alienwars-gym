#include "src/puffercpu.c"
static int allocations,guard;
static void*checked_malloc(size_t n){allocations+=guard;return malloc(n);}
static void*checked_calloc(size_t n,size_t s){allocations+=guard;return calloc(n,s);}
static void*checked_realloc(void*p,size_t n){allocations+=guard;return realloc(p,n);}
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#include "ocean/alienwars_shared/shared_api.c"
#include "ocean/alienwars_shared/alienwars_shared.h"
static float obs[12][OBS_SIZE],actions[12][4],rewards[12],terminals[12];
static unsigned char masks[12][13];
int main(void){
    Dict settings={0};dict_set(&settings,"maps",1);dict_set(&settings,"map_seed",301);dict_set(&settings,"curriculum",0);
    Env env={0};puf_init(&env,&settings);assert(env.num_agents==12);
    for(int i=0;i<12;i++){
        assert(env.agents[i].policy==aw_shared_family(i));
        env.agents[i].observations=obs[i];env.agents[i].actions=actions[i];env.agents[i].rewards=&rewards[i];env.agents[i].terminals=&terminals[i];env.agents[i].action_mask=masks[i];
    }
    guard=1;puf_reset(&env);
    int available=0;
    for(int i=0;i<12;i++){
        assert(rewards[i]==0&&terminals[i]==0);
        if(env.task->world.active[i]){available++;assert(aw_vehicle_clear(&env.task->map->map,&env.task->world.agents[i].vehicle));}
        for(int j=i+1;j<12;j++)if(env.task->world.active[i]&&env.task->world.active[j])assert(!aw_bodies_overlap(aw_vehicle_body(&env.task->world.agents[i].vehicle),aw_vehicle_body(&env.task->world.agents[j].vehicle),0));
        env.task->world.agents[i].limit=2;
        aw_shared_reference(env.task,i,actions[i],0);
    }
    assert(available>=5);puf_step(&env);assert(env.log.n==0);puf_step(&env);assert(env.log.n==available);
    for(int i=0;i<12;i++)assert(terminals[i]==1);
    puf_step(&env);assert(env.task->world.ticks==0&&env.log.n==available);
    for(int i=0;i<12;i++)assert(rewards[i]==0&&terminals[i]==1);
    puf_step(&env);
    int first=-1;for(int i=0;i<12;i++)if(env.task->world.active[i]){assert(terminals[i]==0);first=i;}
    assert(first>=0);actions[first][0]=NAN;actions[first][1]=INFINITY;actions[first][2]=-1;actions[first][3]=1.5f;
    puf_step(&env);assert(env.task->world.agents[first].invalid==4);
    for(int k=0;k<50;k++){
        for(int i=0;i<12;i++)aw_shared_reference(env.task,i,actions[i],1);
        puf_step(&env);
        for(int i=0;i<12;i++){assert(isfinite(rewards[i]));for(int j=0;j<OBS_SIZE;j++)assert(isfinite(obs[i][j])&&fabsf(obs[i][j])<=1);}
    }
    assert(allocations==0);guard=0;puf_close(&env);dict_clear(&settings);
    printf("SHARED_ADAPTER policies=5 agents=12 available=%d terminal_and_reward_reset=PASS independent_state=PASS no_step_reset_allocations=PASS finite_invalid_actions=PASS valid_spawns=PASS\n",available);
}
