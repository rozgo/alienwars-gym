#include "local_api.h"
#include "../alienwars/local_navigation.h"
#include <time.h>
typedef struct {AwMap map;AwRayWorld rays;AwLocalRoute routes[AW_UNITS];int count;} AwLocalWorld;
typedef struct AwLocalBank {int family,maps,refs;unsigned seed;AwLocalWorld*worlds;struct AwLocalBank*next;} AwLocalBank;
struct AwLocalTask {AwLocalBank*bank;AwLocalWorld*world;AwLocalEpisode episode;unsigned rng,action_rng;int difficulty;AwBody peers[4];AwSVec center[4];float phase[4];};
static AwLocalBank*banks;
static unsigned aw_local_rng(unsigned*s){unsigned x=*s?*s:173;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x;}
static float aw_local_rand(unsigned*s){return (aw_local_rng(s)&65535)/65535.0f;}
AwLocalTask*aw_local_create(int family,int maps,unsigned seed,unsigned instance,int difficulty){
    if(family<0||family>=5||maps<1||maps>32)return NULL;AwLocalBank*b=banks;
    while(b&&(b->family!=family||b->maps!=maps||b->seed!=seed))b=b->next;
    if(!b){b=calloc(1,sizeof(*b));if(!b)return NULL;b->family=family;b->maps=maps;b->seed=seed;b->worlds=calloc(maps,sizeof(*b->worlds));if(!b->worlds){free(b);return NULL;}
        clock_t timer=clock();for(int i=0;i<maps;i++){AwLocalWorld*w=&b->worlds[i];AwPatrols*patrol=calloc(1,sizeof(*patrol));
            if(!patrol||!aw_generate_options(&w->map,seed+i,(AwOptions){i%2,1+(i*3)%10,1+(i*5)%10,AW_TEMPERATE+i%AW_BIOMES,1})||!aw_patrol_build(&w->map,patrol)){free(patrol);free(b->worlds);free(b);return NULL;}
            aw_ray_world_init(&w->rays,&w->map);
            if(family==0){AwPatrol scout={0};if(aw_patrol_scout(&w->map,&scout,w->map.spawns[0],w->map.spawns[1]))aw_local_route(&w->map,&scout,&w->routes[w->count++]);}
            for(int j=0;j<AW_PATROLS;j++)if(patrol->units[j].count>2&&aw_vehicle_family(patrol->units[j].layer,patrol->units[j].variant)==family)aw_local_route(&w->map,&patrol->units[j],&w->routes[w->count++]);
            free(patrol);if(!w->count){free(b->worlds);free(b);return NULL;}
            fprintf(stderr,"LOCAL_MAP family=%d seed=%u hash=%08x routes=%d\n",family,w->map.seed,w->map.hash,w->count);
        }fprintf(stderr,"LOCAL_BANK family=%d worlds=%d prep_seconds=%.3f\n",family,maps,(clock()-timer)/(double)CLOCKS_PER_SEC);b->next=banks;banks=b;}
    AwLocalTask*t=calloc(1,sizeof(*t));if(!t)return NULL;t->bank=b;b->refs++;t->rng=aw_hash(instance^seed^173);t->action_rng=aw_hash(instance^3917);t->difficulty=difficulty;return t;
}
void aw_local_destroy(AwLocalTask*t){if(!t)return;AwLocalBank*b=t->bank;if(!--b->refs){AwLocalBank**p=&banks;while(*p!=b)p=&(*p)->next;*p=b->next;free(b->worlds);free(b);}free(t);}
static void aw_local_traffic(AwLocalTask*t){
    float time=t->episode.ticks*.1f;
    for(int i=0;i<4;i++)if(t->peers[i].active){
        float speed=i==0?0:.8f+.25f*i;float phase=t->phase[i]+time*speed*.2f;
        AwBody old=t->peers[i];
        t->peers[i].position=t->center[i];t->peers[i].position.x+=sinf(phase)*3;
        t->peers[i].position.z+=cosf(phase)*2;t->peers[i].velocity=(AwSVec){cosf(phase)*speed*.6f,0,-sinf(phase)*speed*.4f};
        if(i==0)t->peers[i].velocity=(AwSVec){0};
        /* Moving targets are constrained to the same navigable medium. */
        AwVehicle probe=t->episode.vehicle;probe.position=t->peers[i].position;if(!aw_vehicle_clear(&t->world->map,&probe)){t->peers[i].position=t->center[i];t->peers[i].velocity=(AwSVec){0};}else t->peers[i].position=probe.position;
        if(probe.family==AW_VEHICLE_GROUND)t->peers[i].position.y+=t->peers[i].height;
        if(aw_bodies_overlap(aw_vehicle_body(&t->episode.vehicle),t->peers[i],.05f)){
            t->peers[i]=old;t->peers[i].velocity=(AwSVec){0};
        }
    }
}
void aw_local_restart_task(AwLocalTask*t,float*obs){
    t->world=&t->bank->worlds[aw_local_rng(&t->rng)%t->bank->maps];AwLocalWorld*w=t->world;const AwLocalRoute*r=&w->routes[aw_local_rng(&t->rng)%w->count];
    int start=aw_local_rng(&t->rng)%(r->count-2),end=start+1;
    float length=r->family==AW_VEHICLE_WING?18+aw_local_rand(&t->rng)*35:8+aw_local_rand(&t->rng)*20;
    while(end<r->count-1&&r->distance[end]-r->distance[start]<length)end++;
    AwSVec d=aw_sv_add(r->point[start+1],aw_sv_scale(r->point[start],-1));float yaw=atan2f(d.x,d.z);
    yaw+=(aw_local_rand(&t->rng)-.5f)*(r->family==AW_VEHICLE_WING?.25f:3.0f);
    aw_local_reset(&t->episode,r,start,end,yaw,500);
    memset(t->peers,0,sizeof(t->peers));
    if(t->difficulty)for(int i=0;i<4;i++){
        int n=start+1+(int)(aw_local_rand(&t->rng)*(end-start));if(n>end)n=end;
        AwSVec p=r->point[n];float a=aw_local_rand(&t->rng)*6.283185f;
        p.x+=sinf(a)*(2+i);p.z+=cosf(a)*(2+i);
        AwVehicle probe=t->episode.vehicle;probe.position=p;if(!aw_vehicle_clear(&w->map,&probe))continue;
        t->center[i]=probe.position;t->phase[i]=aw_local_rand(&t->rng)*6.283185f;
        t->peers[i]=(AwBody){.position=probe.position,.width=.35f+aw_local_rand(&t->rng)*.5f,.length=.7f,.height=.5f,.active=1};
        if(aw_bodies_overlap(aw_vehicle_body(&t->episode.vehicle),t->peers[i],2))t->peers[i].active=0;
    }
    aw_local_traffic(t);
    for(int i=0;i<4;i++)if(aw_bodies_overlap(aw_vehicle_body(&t->episode.vehicle),t->peers[i],.1f))t->peers[i].active=0;
    aw_local_observe(&t->episode,&w->map,&w->rays,t->peers,4,-1,.1f);memcpy(obs,t->episode.observation,sizeof(t->episode.observation));
}
void aw_local_tick_task(AwLocalTask*t,const float*actions,float*obs,float*reward,int*terminal,int*success,int*contacts){
    aw_local_traffic(t);aw_local_step(&t->episode,&t->world->map,actions,t->peers,4,-1);
    aw_local_observe(&t->episode,&t->world->map,&t->world->rays,t->peers,4,-1,.1f);
    memcpy(obs,t->episode.observation,sizeof(t->episode.observation));*reward=t->episode.reward;*success=t->episode.success;*contacts=t->episode.vehicle.contact;*terminal=t->episode.success||t->episode.timeout||t->episode.vehicle.failed;
}
void aw_local_baseline_task(AwLocalTask*t,float*actions,int mode){if(mode==1)for(int i=0;i<4;i++)actions[i]=aw_local_rng(&t->action_rng)%3;else aw_local_reference(&t->episode,actions);}
