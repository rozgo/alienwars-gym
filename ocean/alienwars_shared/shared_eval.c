#include "../../src/puffercpu.c"
/* Standalone evaluator includes the core to expose per-mission measurements,
 * without adding simulator internals to the trainer's small C ABI. */
#include "shared_api.c"
int main(int argc,char**argv){
    if(argc!=7){fprintf(stderr,"Usage: shared-eval models|reference|random map_seed maps scenarios curriculum output_mode\n");return 2;}
    int seed=atoi(argv[2]),maps=atoi(argv[3]),scenarios=atoi(argv[4]),curriculum=atoi(argv[5]);
    if(scenarios<1||scenarios>100000)return 2;
    int baseline=!strcmp(argv[1],"reference")?1:!strcmp(argv[1],"random")?2:0;
    AwSharedTask*t=aw_shared_create(maps,seed,9091,curriculum);if(!t)return 2;
    Weights*weights[5]={0};PufferNet*net[12]={0};int sizes[]={4,3,3,3};
    if(!baseline){for(int f=0;f<5;f++){
        char file[2048];snprintf(file,sizeof(file),"%s/mission-%d.bin",argv[1],f);weights[f]=load_weights(file);
        int expected=128*AW_SHARED_OBS+14*128+2*3*128*128;
        if(!weights[f]||weights[f]->size-7!=expected){fprintf(stderr,"Missing or wrong-sized family %d checkpoint\n",f);return 2;}
        for(int j=0;j<expected;j++)if(!isfinite(weights[f]->data[j]))return 2;
    }for(int i=0;i<12;i++){Weights*w=weights[aw_shared_family(i)];w->idx=0;net[i]=make_puffernet(w,1,AW_SHARED_OBS,128,2,sizes,4);}}
    int attempted[5]={0},unavailable[5]={0},wins[5]={0},contacts[5]={0},events[5]={0},blocked[5]={0},steps[5]={0};
    for(int episode=0;episode<scenarios;episode++){
        aw_shared_reset_at(t,(episode/3)%maps,episode%3);float terminal[12];int present[12];
        for(int i=0;i<12;i++){terminal[i]=1;present[i]=t->world.active[i];attempted[aw_shared_family(i)]++;unavailable[aw_shared_family(i)]+=!present[i];}
        while(!t->reset_pending){
            for(int i=0;i<12;i++){
                if(baseline||t->terminal[i])aw_shared_reference(t,i,t->actions[i],baseline==2);
                else {unsigned char mask[13];aw_shared_mask(t,i,mask);forward_puffernet(net[i],t->world.agents[i].observation,t->actions[i],NULL,&terminal[i]);multidiscrete(net[i]->multidiscrete,net[i]->decoder->output,t->actions[i],1,mask);}
            }
            aw_shared_step(t);
            for(int i=0;i<12;i++){terminal[i]=t->terminal[i];if(t->event[i]){
                AwMissionAgent*a=&t->world.agents[i];int f=aw_shared_family(i);
                wins[f]+=a->arrived;contacts[f]+=a->contacts;events[f]+=a->collision_events;blocked[f]+=a->blocked_total;steps[f]+=a->ticks;
            }}
        }
        for(int i=0;i<12;i++){
            AwMissionAgent*a=&t->world.agents[i];
            printf("{\"scenario\":%d,\"map_seed\":%u,\"layout\":%d,\"unit\":%d,\"family\":%d,\"available\":%s,\"arrived\":%d,\"impact\":%d,\"timeout\":%d,\"ticks\":%d,\"contact_decisions\":%d,\"collision_events\":%d,\"blocked_decisions\":%d,\"route_length\":%.4f}\n",
                episode,t->map->map.seed,t->scenario,i,aw_shared_family(i),present[i]?"true":"false",a->arrived,a->vehicle.failed,a->timeout,a->ticks,a->contacts,a->collision_events,a->blocked_total,present[i]?a->route->distance[a->route->count-1]:0);
        }fflush(stdout);
    }
    for(int f=0;f<5;f++)printf("{\"family\":%d,\"attempted\":%d,\"unavailable\":%d,\"arrivals\":%d,\"steps\":%d,\"contact_decisions\":%d,\"collision_events\":%d,\"blocked_decisions\":%d}\n",f,attempted[f],unavailable[f],wins[f],steps[f],contacts[f],events[f],blocked[f]);
    for(int i=0;i<12;i++)if(net[i])free_puffernet(net[i]);for(int f=0;f<5;f++)free(weights[f]);aw_shared_destroy(t);return 0;
}
