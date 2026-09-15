#ifndef ALIENWARS_ENV_H
#define ALIENWARS_ENV_H
typedef float obs_t;
#include "pufferenv.h"
#include "nav_api.h"
#define OBS_SIZE AW_NAV_INPUTS
#define NUM_ATNS 2
#define ACT_SIZES {3,3}
#define PUF_STEPS_PER_SEC 10
struct Log {
    float perf,score,success,fall,timeout,contacts,episode_steps;
    float surface_success,bridge_success,tunnel_success;
    float surface_episodes,bridge_episodes,tunnel_episodes,invalid_actions,n;
};
struct Env {
    Log log;
    int num_agents;
    unsigned int rng;
    Agent agents[1];
    int tag,boundary_reached;
    AwNav *nav;
};
static double aw_env_option(Dict *kwargs,const char *key,double fallback) {
    DictItem *item=dict_find(kwargs,key);return item?item->value:fallback;
}
void puf_init(Env *env,Dict *kwargs) {
    env->num_agents=1;env->agents[0].policy=0;env->agents[0].action_mask=NULL;
    AwNavConfig config={0};
    config.map_seed=(uint32_t)aw_env_option(kwargs,"map_seed",71);
    config.episode_seed=(uint32_t)aw_env_option(kwargs,"episode_seed",173);
    config.maps=(int)aw_env_option(kwargs,"maps",4);
    config.kind=(int)aw_env_option(kwargs,"task_kind",-1);
    config.limit=(int)aw_env_option(kwargs,"episode_steps",600);
    config.controller=(int)aw_env_option(kwargs,"controller",0);
    env->nav=aw_nav_create(config,env->rng);
    if(!env->nav){fprintf(stderr,"AlienWars navigation bank could not satisfy task settings\n");exit(2);}
}
void puf_reset(Env *env) {
    env->agents[0].rewards[0]=0;env->agents[0].terminals[0]=0;
    aw_nav_restart(env->nav,env->agents[0].observations);
}
void puf_step(Env *env) {
    Agent *agent=&env->agents[0];agent->rewards[0]=0;agent->terminals[0]=0;
    AwNavResult r=aw_nav_tick(env->nav,agent->actions,agent->observations);
    agent->rewards[0]=r.reward;
    if(r.success||r.fall||r.timeout){
        env->log.perf+=r.success;env->log.score+=r.total_reward;
        env->log.success+=r.success;env->log.fall+=r.fall;env->log.timeout+=r.timeout;
        env->log.contacts+=r.contacts;env->log.episode_steps+=r.steps;
        env->log.invalid_actions+=r.invalid_actions;
        if(r.kind==0){env->log.surface_success+=r.success;env->log.surface_episodes++;}
        if(r.kind==1){env->log.bridge_success+=r.success;env->log.bridge_episodes++;}
        if(r.kind==2){env->log.tunnel_success+=r.success;env->log.tunnel_episodes++;}
        env->log.n++;agent->terminals[0]=1;
        aw_nav_restart(env->nav,agent->observations);
    }
}
void puf_log(Log *log,Dict *out) {
#define AW_LOG(field) dict_set(out,#field,log->field)
    AW_LOG(perf);AW_LOG(score);AW_LOG(success);AW_LOG(fall);AW_LOG(timeout);
    AW_LOG(contacts);AW_LOG(episode_steps);AW_LOG(invalid_actions);
    AW_LOG(surface_episodes);AW_LOG(bridge_episodes);AW_LOG(tunnel_episodes);AW_LOG(n);
    dict_set(out,"surface_success",log->surface_episodes?log->surface_success/log->surface_episodes:0);
    dict_set(out,"bridge_success",log->bridge_episodes?log->bridge_success/log->bridge_episodes:0);
    dict_set(out,"tunnel_success",log->tunnel_episodes?log->tunnel_success/log->tunnel_episodes:0);
#undef AW_LOG
}
void puf_render(Env *env){aw_nav_render(env->nav);}
void puf_close(Env *env){aw_nav_delete(env->nav);env->nav=NULL;aw_nav_render_close();}
#endif
