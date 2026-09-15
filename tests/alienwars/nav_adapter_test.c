#include "src/puffercpu.c"
#include "ocean/alienwars/alienwars.h"
int main(void){
    Dict settings={0};dict_set(&settings,"maps",1);dict_set(&settings,"map_seed",72);
    dict_set(&settings,"episode_steps",2);dict_set(&settings,"task_kind",0);
    Env env={0};puf_init(&env,&settings);
    float obs[OBS_SIZE],actions[2]={1,1},reward=99,terminal=99;
    env.agents[0].observations=obs;env.agents[0].actions=actions;
    env.agents[0].rewards=&reward;env.agents[0].terminals=&terminal;
    puf_reset(&env);assert(reward==0&&terminal==0);
    puf_step(&env);assert(terminal==0);puf_step(&env);assert(terminal==1);
    assert(obs[OBS_SIZE-1]==1&&env.log.timeout==1&&env.log.n==1);
    puf_step(&env);assert(terminal==0);puf_step(&env);assert(terminal==1&&env.log.n==2);
    for(int j=0;j<OBS_SIZE;j++)assert(isfinite(obs[j]));
    /* Warmed recurrent state must be indistinguishable from a fresh network
     * when an episode terminal is supplied with its first observation. */
    int hidden=8,layers=2,n=hidden*OBS_SIZE+7*hidden+layers*3*hidden*hidden;
    Weights *weights=calloc(1,sizeof(*weights)+n*sizeof(float));
    weights->data=(float*)(weights+1);weights->size=n;
    for(int i=0;i<n;i++)weights->data[i]=sinf(i*.7f)*.02f;
    int sizes[]=ACT_SIZES;PufferNet *a=make_puffernet(weights,1,OBS_SIZE,hidden,layers,sizes,2);
    weights->idx=0;PufferNet *b=make_puffernet(weights,1,OBS_SIZE,hidden,layers,sizes,2);
    terminal=0;for(int i=0;i<7;i++)forward_puffernet(a,obs,actions,NULL,&terminal);
    terminal=1;forward_puffernet(a,obs,actions,NULL,&terminal);forward_puffernet(b,obs,actions,NULL,&terminal);
    assert(!memcmp(a->decoder->output,b->decoder->output,7*sizeof(float)));
    free_puffernet(a);free_puffernet(b);free(weights);puf_close(&env);dict_clear(&settings);
    puts("NAV_ADAPTER terminal_pulse=PASS autoreset=PASS reward_clear=PASS recurrent_reset=PASS");return 0;
}
