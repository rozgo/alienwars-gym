/* Explicit checkpoint viewer and bounded, reproducible CPU evaluation.
 * The forward pass is upstream PufferNet, including terminal-state clearing. */
#include "src/puffercpu.c"
#include "alienwars.h"
#include "src/live_log.h"
static int align8(int n){return (n+7)&~7;}
int main(int argc,char **argv){
    const char *model=NULL;int headless=0,episodes=0;
    char *overrides[argc];int count=0;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--headless")){headless=1;continue;}
        if(!strncmp(argv[i],"--episodes=",11)){episodes=atoi(argv[i]+11);continue;}
        if(argv[i][0]!='-'&&strstr(argv[i],".bin")){model=argv[i];continue;}
        overrides[count++]=argv[i];
    }
    Ini ini={0};puf_ini_load_env(&ini,"alienwars",count,overrides);
    Dict *settings=puf_ini_section(&ini,"env",0);
    int mode=(int)dict_get(settings,"controller");
    if(mode==0&&!model){fprintf(stderr,"Policy mode requires an explicit .bin checkpoint. Use --env.controller=4 for manual, 1 random, 2 greedy, or 3 reference.\n");return 2;}
    if(mode!=0&&model){fprintf(stderr,"A checkpoint requires --env.controller=0.\n");return 2;}
    if(headless&&(episodes<1||mode==4)){fprintf(stderr,"Headless evaluation needs --episodes=N and policy or baseline mode.\n");return 2;}
    int hidden=puf_ini_get(&ini,"policy","hidden_size"),layers=puf_ini_get(&ini,"policy","num_layers");
    if(hidden<1||hidden>2048||layers<1||layers>8){fprintf(stderr,"Invalid policy architecture\n");return 2;}
    Weights *weights=NULL;PufferNet *net=NULL;int sizes[]=ACT_SIZES;
    if(model){
        struct stat st;int need=align8(hidden*OBS_SIZE)+align8(7*hidden)+layers*align8(3*hidden*hidden);
        if(stat(model,&st)||st.st_size!=(long)need*4){fprintf(stderr,"Checkpoint missing or architecture mismatch: expected %d float32 values for %d x %d\n",need,hidden,layers);return 2;}
        weights=load_weights(model);if(!weights)return 2;
        for(int i=0;i<need;i++)if(!isfinite(weights->data[i])){fprintf(stderr,"Nonfinite checkpoint\n");free(weights);return 2;}
        net=make_puffernet(weights,1,OBS_SIZE,hidden,layers,sizes,2);
    }
    Env env={0};puf_init(&env,settings);
    float observations[OBS_SIZE]={0},actions[2]={1,1},reward=0,terminal=1;
    env.agents[0].observations=observations;env.agents[0].actions=actions;
    env.agents[0].rewards=&reward;env.agents[0].terminals=&terminal;
    aw_nav_restart(env.nav,observations);
    const char *label=model?model:mode==1?"random":mode==2?"greedy":mode==3?"graph reference":"manual";
    aw_nav_prediction(env.nav,0,NULL,label);
    fputs("{\"type\":\"evaluation\",\"controller\":",stdout);puf_json_string(stdout,label);
    printf(",\"hidden\":%d,\"layers\":%d,\"episode_seed\":%.0f,\"map_seed\":%.0f,\"maps\":%.0f}\n",hidden,layers,dict_get(settings,"episode_seed"),dict_get(settings,"map_seed"),dict_get(settings,"maps"));
    int completed=0,wins[3]={0},totals[3]={0};long steps=0;double accumulator=0,hold=0;
    while(!episodes||completed<episodes){
        if(!headless){
            aw_nav_render(env.nav);puf_web_vsync();if(WindowShouldClose())break;
            if(aw_nav_view_reset()){
                aw_nav_restart(env.nav,observations);terminal=1;hold=0;accumulator=0;
            }
            if(aw_nav_view_paused())continue;
            float dt=fminf(GetFrameTime(),.1f);
            if(hold>0){hold-=dt;if(hold<=0){aw_nav_restart(env.nav,observations);terminal=1;}continue;}
            accumulator+=dt;if(accumulator<.1)continue;accumulator-=.1;
        }
        if(net){
            forward_puffernet(net,observations,actions,NULL,&terminal);
            for(int j=0;j<7;j++)if(!isfinite(net->decoder->output[j])){fprintf(stderr,"Nonfinite policy output\n");return 3;}
            aw_nav_prediction(env.nav,net->decoder->output[6],net->decoder->output,label);
        }
        AwNavResult r=aw_nav_tick(env.nav,actions,observations);steps++;
        terminal=r.success||r.fall||r.timeout;
        for(int j=0;j<OBS_SIZE;j++)assert(isfinite(observations[j]));assert(isfinite(r.reward));
        if(!terminal)continue;
        int kind=(int)r.kind;totals[kind]++;wins[kind]+=(int)r.success;completed++;
        printf("{\"type\":\"episode\",\"episode\":%d,\"kind\":%d,\"map_seed\":%u,\"map_hash\":\"%08x\",\"start\":%u,\"goal\":%u,\"success\":%.0f,\"fall\":%.0f,\"timeout\":%.0f,\"return\":%.7g,\"steps\":%.0f,\"contacts\":%.0f,\"invalid_actions\":%.0f}\n",completed,kind,r.map_seed,r.map_hash,r.start,r.goal,r.success,r.fall,r.timeout,r.total_reward,r.steps,r.contacts,r.invalid_actions);fflush(stdout);
        if(headless)aw_nav_restart(env.nav,observations);else hold=1.5;
    }
    printf("{\"type\":\"summary\",\"episodes\":%d,\"steps\":%ld,\"successes\":[%d,%d,%d],\"counts\":[%d,%d,%d]}\n",completed,steps,wins[0],wins[1],wins[2],totals[0],totals[1],totals[2]);
    puf_close(&env);if(net)free_puffernet(net);free(weights);puf_ini_free(&ini);return 0;
}
