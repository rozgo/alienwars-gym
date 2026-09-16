#include "../../src/puffercpu.c"
#include "local_api.h"
int main(int argc,char**argv){
    if(argc<6){fprintf(stderr,"Usage: eval checkpoint|reference|random family map_seed maps episodes [difficulty]\n");return 2;}
    int family=atoi(argv[2]),seed=atoi(argv[3]),maps=atoi(argv[4]),episodes=atoi(argv[5]),difficulty=argc>6?atoi(argv[6]):1;
    if(episodes<1||episodes>100000)return 2;
    AwLocalTask*t=aw_local_create(family,maps,seed,9091,difficulty);if(!t)return 2;
    PufferNet*net=NULL;Weights*w=NULL;int sizes[]={3,3,3,3},baseline=!strcmp(argv[1],"random")?1:!strcmp(argv[1],"reference")?2:0;
    if(!baseline){w=load_weights(argv[1]);if(!w)return 2;int expected=128*AW_LOCAL_OBS+13*128+2*3*128*128;if(w->size-7!=expected){fprintf(stderr,"Wrong checkpoint size\n");return 2;}for(int i=0;i<expected;i++)if(!isfinite(w->data[i])){fprintf(stderr,"Nonfinite checkpoint\n");return 2;}net=make_puffernet(w,1,AW_LOCAL_OBS,128,2,sizes,4);}
    srand(9092);float obs[AW_LOCAL_OBS],actions[4],reward,terminal=1;int successes=0,contact_steps=0,total_steps=0;
    for(int ep=0;ep<episodes;ep++){aw_local_restart_task(t,obs);terminal=1;int done=0,success=0,contact=0,steps=0,contacts=0;
        while(!done){if(net){forward_puffernet(net,obs,actions,NULL,&terminal);multidiscrete(net->multidiscrete,net->decoder->output,actions,1,NULL);}else aw_local_baseline_task(t,actions,baseline);
            aw_local_tick_task(t,actions,obs,&reward,&done,&success,&contact);terminal=done;contacts+=contact;steps++;for(int i=0;i<AW_LOCAL_OBS;i++)assert(isfinite(obs[i]));assert(isfinite(reward));}
        successes+=success;contact_steps+=contacts;total_steps+=steps;printf("{\"episode\":%d,\"success\":%d,\"steps\":%d,\"contacts\":%d}\n",ep,success,steps,contacts);fflush(stdout);}
    printf("{\"family\":%d,\"episodes\":%d,\"successes\":%d,\"steps\":%d,\"contact_steps\":%d}\n",family,episodes,successes,total_steps,contact_steps);
    aw_local_destroy(t);if(net)free_puffernet(net);free(w);return 0;
}
