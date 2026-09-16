#ifndef ALIENWARS_LOCAL_ENV_H
#define ALIENWARS_LOCAL_ENV_H
typedef float obs_t;
#include "pufferenv.h"
#include "local_api.h"
#define OBS_SIZE AW_LOCAL_OBS
#define NUM_ATNS 4
#define ACT_SIZES {3,3,3,3}
#define PUF_STEPS_PER_SEC 10
struct Log {float perf,score,success,contacts,episode_steps,n;};
struct Env {Log log;int num_agents;int tag,boundary_reached;unsigned int rng;Agent agents[1];AwLocalTask*task;int steps,contacts;float score;};
void puf_init(Env*e,Dict*kwargs){e->num_agents=1;e->agents[0].policy=0;e->agents[0].action_mask=NULL;e->task=aw_local_create((int)dict_get(kwargs,"family"),(int)dict_get(kwargs,"maps"),(unsigned)dict_get(kwargs,"map_seed"),e->rng,(int)dict_get(kwargs,"difficulty"));if(!e->task){fprintf(stderr,"Local navigation task preparation failed\n");exit(2);}}
void puf_reset(Env*e){e->steps=e->contacts=0;e->score=0;e->agents[0].rewards[0]=e->agents[0].terminals[0]=0;aw_local_restart_task(e->task,e->agents[0].observations);}
void puf_step(Env*e){Agent*a=&e->agents[0];int done=0,win=0,contact=0;aw_local_tick_task(e->task,a->actions,a->observations,a->rewards,&done,&win,&contact);a->terminals[0]=done;e->steps++;e->contacts+=contact;e->score+=a->rewards[0];if(done){e->log.perf+=win;e->log.success+=win;e->log.score+=e->score;e->log.contacts+=e->contacts;e->log.episode_steps+=e->steps;e->log.n++;aw_local_restart_task(e->task,a->observations);e->steps=e->contacts=0;e->score=0;}}
void puf_log(Log*l,Dict*d){dict_set(d,"perf",l->perf);dict_set(d,"score",l->score);dict_set(d,"success",l->success);dict_set(d,"contacts",l->contacts);dict_set(d,"episode_steps",l->episode_steps);dict_set(d,"n",l->n);}
void puf_render(Env*e){(void)e;} /* Map Lab uses the same vehicle/controller core. */
void puf_close(Env*e){aw_local_destroy(e->task);e->task=NULL;}
#endif
