#ifndef ALIENWARS_SHARED_ENV_H
#define ALIENWARS_SHARED_ENV_H
typedef float obs_t;
#include "pufferenv.h"
#include "shared_api.h"
#define OBS_SIZE AW_SHARED_OBS
#define NUM_ATNS 4
#define ACT_SIZES {4,3,3,3}
#define PUF_STEPS_PER_SEC 10
struct Log {float perf,score,contacts,blocked,family_wins[5],family_episodes[5],n;};
struct Env {
    Log log;int num_agents,tag,boundary_reached;unsigned rng;
    Agent agents[AW_SHARED_AGENTS];AwSharedTask*task;float score[AW_SHARED_AGENTS];
};
void puf_init(Env*e,Dict*kwargs){
    e->num_agents=AW_SHARED_AGENTS;
    for(int i=0;i<e->num_agents;i++){e->agents[i].policy=aw_shared_family(i);e->agents[i].action_mask=NULL;}
    e->task=aw_shared_create((int)dict_get(kwargs,"maps"),(unsigned)dict_get(kwargs,"map_seed"),e->rng,(int)dict_get(kwargs,"curriculum"));
    if(!e->task){fprintf(stderr,"Shared mission preparation failed\n");exit(2);}
}
void puf_reset(Env*e){
    aw_shared_reset(e->task);memset(e->score,0,sizeof(e->score));
    for(int i=0;i<e->num_agents;i++){int done,event,win,contacts,blocked;float reward;
        aw_shared_read(e->task,i,e->agents[i].observations,&reward,&done,&event,&win,&contacts,&blocked);
        e->agents[i].rewards[0]=0;e->agents[i].terminals[0]=0;aw_shared_mask(e->task,i,e->agents[i].action_mask);}
}
void puf_step(Env*e){
    for(int i=0;i<e->num_agents;i++)aw_shared_action(e->task,i,e->agents[i].actions);
    aw_shared_step(e->task);
    for(int i=0;i<e->num_agents;i++){
        Agent*a=&e->agents[i];int done,event,win,contacts,blocked;
        aw_shared_read(e->task,i,a->observations,a->rewards,&done,&event,&win,&contacts,&blocked);
        a->terminals[0]=done;aw_shared_mask(e->task,i,a->action_mask);e->score[i]+=a->rewards[0];
        if(event){int family=aw_shared_family(i);e->log.perf+=win;e->log.score+=e->score[i];e->score[i]=0;
            e->log.contacts+=contacts;e->log.blocked+=blocked;e->log.family_wins[family]+=win;e->log.family_episodes[family]++;e->log.n++;}
    }
}
void puf_log(Log*l,Dict*d){
    dict_set(d,"perf",l->perf);dict_set(d,"score",l->score);dict_set(d,"contacts",l->contacts);dict_set(d,"blocked",l->blocked);
    for(int f=0;f<5;f++){char key[64];snprintf(key,sizeof(key),"family_%d_success",f);dict_set(d,key,l->family_episodes[f]>0?l->family_wins[f]/l->family_episodes[f]:0);}
    dict_set(d,"n",l->n);
}
void puf_render(Env*e){(void)e;}
void puf_close(Env*e){aw_shared_destroy(e->task);e->task=NULL;}
#endif
