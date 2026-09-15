#include "nav_api.h"
#include "nav_core.h"
#include <time.h>
_Static_assert(AW_NAV_INPUTS==AW_NAV_OBS,"Native adapter observation metadata must match the shared task");
typedef struct AwNavBank {
    AwNavConfig config;
    int refs,choices,world_index[32*AW_NAV_TASKS],task_index[32*AW_NAV_TASKS];
    AwNavWorld *worlds;
    struct AwNavBank *next;
} AwNavBank;
struct AwNav {
    AwNavBank *bank;
    AwNavEpisode episode;
    uint32_t rng,controller_rng;
    int controller;
    AwNavResult last;
    unsigned episodes,successes;
    float value,probabilities[6];
    char label[256];
};
static AwNavBank *aw_nav_banks;

AwNav *aw_nav_create(AwNavConfig config,uint32_t instance) {
    if(config.maps<1||config.maps>32||config.kind< -1||config.kind>2||config.limit<1||config.limit>600||
        config.controller<0||config.controller>4)return NULL;
    /* PufferLib creates/closes its CPU environments serially. Bank contents
     * are immutable once workers start; no global state is used in step. */
    AwNavBank *bank=aw_nav_banks;
    while(bank&&(bank->config.map_seed!=config.map_seed||bank->config.maps!=config.maps||
          bank->config.kind!=config.kind||bank->config.limit!=config.limit))bank=bank->next;
    if(!bank){
        bank=calloc(1,sizeof(*bank));if(!bank)return NULL;
        bank->config=config;bank->worlds=calloc(config.maps,sizeof(*bank->worlds));
        if(!bank->worlds){free(bank);return NULL;}
        clock_t start=clock();int counts[3]={0};
        for(int i=0;i<config.maps;i++){
            if(!aw_nav_world_init(&bank->worlds[i],config.map_seed+(uint32_t)i)){
                free(bank->worlds);free(bank);return NULL;
            }
            AwNavWorld *w=&bank->worlds[i];
            for(int j=0;j<w->task_count;j++)if(config.kind<0||config.kind==w->tasks[j].kind){
                int k=bank->choices++;bank->world_index[k]=i;bank->task_index[k]=j;counts[w->tasks[j].kind]++;
            }
            fprintf(stderr,"NAV_MAP seed=%u hash=%08x surface=%d bridge=%d tunnel=%d\n",w->map.seed,w->map.hash,w->counts[0],w->counts[1],w->counts[2]);
        }
        if(!bank->choices){free(bank->worlds);free(bank);return NULL;}
        fprintf(stderr,"NAV_BANK worlds=%d tasks=%d surface=%d bridge=%d tunnel=%d bytes=%zu init_cpu_seconds=%.3f\n",
            config.maps,bank->choices,counts[0],counts[1],counts[2],sizeof(*bank)+config.maps*sizeof(AwNavWorld),(clock()-start)/(double)CLOCKS_PER_SEC);
        bank->next=aw_nav_banks;aw_nav_banks=bank;
    }
    AwNav *nav=calloc(1,sizeof(*nav));if(!nav){
        if(!bank->refs){aw_nav_banks=bank->next;free(bank->worlds);free(bank);}
        return NULL;
    }
    nav->bank=bank;bank->refs++;nav->controller=config.controller;
    nav->rng=aw_hash(config.episode_seed^(instance*0x9e3779b9u));
    nav->controller_rng=aw_hash(nav->rng^0x173897u);
    return nav;
}
void aw_nav_delete(AwNav *nav) {
    if(!nav)return;AwNavBank *bank=nav->bank;
    if(!--bank->refs){
        AwNavBank **p=&aw_nav_banks;while(*p!=bank)p=&(*p)->next;*p=bank->next;
        free(bank->worlds);free(bank);
    }
    free(nav);
}
void aw_nav_restart(AwNav *nav,float *observations) {
    int choice=aw_nav_random(&nav->rng)%nav->bank->choices;
    float yaw=(aw_nav_random(&nav->rng)%65536)/65536.0f*(2*AW_MOTION_PI)-AW_MOTION_PI;
    aw_nav_reset(&nav->episode,&nav->bank->worlds[nav->bank->world_index[choice]],
        nav->bank->task_index[choice],yaw,nav->bank->config.limit);
    memcpy(observations,nav->episode.observations,sizeof(nav->episode.observations));
}
static void aw_nav_manual(float actions[2]);
AwNavResult aw_nav_tick(AwNav *nav,const float *actions,float *observations) {
    float chosen[2]={actions[0],actions[1]};
    if(nav->controller>=1&&nav->controller<=3)aw_nav_baseline(&nav->episode,nav->controller,&nav->controller_rng,chosen);
    if(nav->controller==4)aw_nav_manual(chosen);
    aw_nav_step(&nav->episode,chosen[0],chosen[1]);
    AwNavEpisode *e=&nav->episode;
    AwNavResult r={e->reward,e->total_reward,e->result==AW_NAV_SUCCESS,e->result==AW_NAV_FALL,
        e->result==AW_NAV_TIMEOUT,e->contacts,e->ticks,e->task->kind,e->invalid_actions};
    r.map_seed=e->world->map.seed;r.map_hash=e->world->map.hash;r.start=e->task->start;r.goal=e->task->goal;
    if(e->result){nav->last=r;nav->episodes++;nav->successes+=r.success;}
    memcpy(observations,e->observations,sizeof(e->observations));return r;
}
void aw_nav_controller(AwNav *nav,int mode){nav->controller=mode;}
void aw_nav_prediction(AwNav *nav,float value,const float *logits,const char *label){
    nav->value=value;
    if(label)snprintf(nav->label,sizeof(nav->label),"%s",label);
    if(logits)for(int h=0;h<2;h++){
        float peak=fmaxf(logits[h*3],fmaxf(logits[h*3+1],logits[h*3+2])),sum=0;
        for(int j=0;j<3;j++){nav->probabilities[h*3+j]=expf(logits[h*3+j]-peak);sum+=nav->probabilities[h*3+j];}
        for(int j=0;j<3;j++)nav->probabilities[h*3+j]/=sum;
    }
}
#ifdef AW_NAV_HEADLESS
static void aw_nav_manual(float actions[2]){actions[0]=actions[1]=1;}
void aw_nav_render(AwNav *nav){(void)nav;}
void aw_nav_render_close(void){}
int aw_nav_view_paused(void){return 0;}
int aw_nav_view_reset(void){return 0;}
#else
#include "nav_render.h"
#endif
