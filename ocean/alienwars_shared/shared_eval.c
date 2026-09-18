#include "../../src/puffercpu.c"
#include "shared_api.c"
int main(int argc,char**argv){
    if(argc!=7){fprintf(stderr,"Usage: shared-eval models|reference|random map_seed maps scenarios curriculum json\n");return 2;}
    int seed=atoi(argv[2]),maps=atoi(argv[3]),scenarios=atoi(argv[4]),curriculum=atoi(argv[5]);
    int per_map=getenv("AW_EVAL_SCENARIOS_PER_MAP")?atoi(getenv("AW_EVAL_SCENARIOS_PER_MAP")):AW_SHARED_SCENARIOS;
    if(scenarios<1||scenarios>100000||per_map<1||per_map>AW_SHARED_SCENARIOS)return 2;
    int baseline=!strcmp(argv[1],"reference")?1:!strcmp(argv[1],"random")?2:0;
    int version=getenv("AW_EVAL_NAV_VERSION")?atoi(getenv("AW_EVAL_NAV_VERSION")):3,assist=!getenv("AW_EVAL_NO_ASSIST");
    AwSharedTask*t=aw_shared_create(maps,seed,9091,curriculum);if(!t)return 2;
    Weights*weights[5]={0};PufferNet*net[12]={0};int sizes[]={4,3,3,3};
    if(!baseline){for(int f=0;f<5;f++){
        char file[2048];snprintf(file,sizeof(file),"%s/mission-%d.bin",argv[1],f);weights[f]=load_weights(file);
        if(!weights[f]||weights[f]->size-7!=AW_FROZEN_FLOATS)return 2;
        for(int j=0;j<AW_FROZEN_FLOATS;j++)if(!isfinite(weights[f]->data[j]))return 2;
    }for(int i=0;i<12;i++){Weights*w=weights[aw_shared_family(i)];w->idx=0;net[i]=make_puffernet(w,1,AW_SHARED_OBS,128,2,sizes,4);}}
    int attempted[5]={0},unavailable[5]={0},wins[5]={0},contacts[5]={0},events[5]={0},blocked[5]={0},steps[5]={0},clean[5]={0},terrain[5]={0},units[5]={0},stalls[5]={0},deadlocks[5]={0},yields[5]={0},replans[5]={0},interventions[5]={0},renewals[5]={0};
    for(int episode=0;episode<scenarios;episode++){
        aw_shared_reset_at(t,(episode/per_map)%maps,episode%per_map);float terminal[12];
        for(int i=0;i<12;i++){
            AwMissionAgent*a=&t->world.agents[i];a->control_version=version;a->assist_enabled=assist&&version>=3;
            if(t->world.active[i])a->previous_potential=aw_mission_project(a);
            terminal[i]=1;attempted[aw_shared_family(i)]++;unavailable[aw_shared_family(i)]+=!t->world.active[i];
            if(!t->world.active[i])printf("{\"scenario\":%d,\"map_seed\":%u,\"unit\":%d,\"family\":%d,\"available\":false}\n",episode,t->map->map.seed,i,aw_shared_family(i));
        }aw_mission_observe(&t->world);
        while(!t->reset_pending){
            for(int i=0;i<12;i++){
                if(baseline||!t->world.active[i]||(t->terminal[i]&&!t->pending_renew[i]))aw_shared_reference(t,i,t->actions[i],baseline==2);
                else {unsigned char mask[13];aw_shared_mask(t,i,mask);forward_puffernet(net[i],t->world.agents[i].observation,t->actions[i],NULL,&terminal[i]);multidiscrete(net[i]->multidiscrete,net[i]->decoder->output,t->actions[i],1,mask);}
            }
            aw_shared_step(t);
            for(int i=0;i<12;i++){terminal[i]=t->terminal[i];if(t->event[i]){
                AwSharedOutcome o=t->outcome[i];int f=aw_shared_family(i);
                wins[f]+=o.arrived;clean[f]+=o.arrived&&!o.contacts;terrain[f]+=o.terrain;units[f]+=o.units;stalls[f]+=o.stall>=100;contacts[f]+=o.contacts;events[f]+=o.events;blocked[f]+=o.blocked;steps[f]+=o.ticks;
                deadlocks[f]+=o.deadlocks;yields[f]+=o.yields;replans[f]+=o.replans;interventions[f]+=o.interventions;
                if(t->pending_renew[i]){attempted[f]++;renewals[f]++;}
                printf("{\"scenario\":%d,\"map_seed\":%u,\"layout\":%d,\"unit\":%d,\"family\":%d,\"available\":true,\"arrived\":%d,\"impact\":%d,\"timeout\":%d,\"ticks\":%d,\"contact_decisions\":%d,\"collision_events\":%d,\"blocked_decisions\":%d,\"route_length\":%.4f,\"terrain_contacts\":%d,\"unit_contacts\":%d,\"max_blocked_ticks\":%d,\"remaining\":%.4f,\"deadlock_events\":%d,\"yield_decisions\":%d,\"replans\":%d,\"assist_decisions\":%d}\n",episode,t->map->map.seed,t->scenario,i,f,o.arrived,o.impact,o.timeout,o.ticks,o.contacts,o.events,o.blocked,o.length,o.terrain,o.units,o.stall,o.remaining,o.deadlocks,o.yields,o.replans,o.interventions);
            }}
        }fflush(stdout);
    }
    for(int f=0;f<5;f++)printf("{\"family\":%d,\"attempted\":%d,\"unavailable\":%d,\"arrivals\":%d,\"steps\":%d,\"contact_decisions\":%d,\"collision_events\":%d,\"blocked_decisions\":%d,\"collision_free_arrivals\":%d,\"terrain_contacts\":%d,\"unit_contacts\":%d,\"stalled_10s\":%d,\"deadlock_events\":%d,\"yield_decisions\":%d,\"replans\":%d,\"assist_decisions\":%d,\"repeated_requests\":%d}\n",f,attempted[f],unavailable[f],wins[f],steps[f],contacts[f],events[f],blocked[f],clean[f],terrain[f],units[f],stalls[f],deadlocks[f],yields[f],replans[f],interventions[f],renewals[f]);
    for(int i=0;i<12;i++)if(net[i])free_puffernet(net[i]);for(int f=0;f<5;f++)free(weights[f]);aw_shared_destroy(t);return 0;
}
