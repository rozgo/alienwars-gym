#ifndef ALIENWARS_FLEET_H
#define ALIENWARS_FLEET_H
#include "local_navigation.h"
#include "../../src/puffercpu.c"
/* One recurrent state per vehicle; weights are immutable and shared by family. */
typedef struct {
    AwLocalRoute route[AW_UNITS];AwLocalEpisode unit[AW_UNITS];AwBody body[AW_UNITS];
    AwVehicle previous[AW_UNITS];PufferNet*net[AW_UNITS];Weights*weights[AW_VEHICLE_FAMILIES];AwRayWorld rays;
    float accumulator,terminal[AW_UNITS];int ready,trained,active[AW_UNITS],arrivals[AW_UNITS];
} AwFleet;
static void aw_fleet_close(AwFleet*f){
    for(int i=0;i<AW_UNITS;i++)if(f->net[i])free_puffernet(f->net[i]);
    for(int i=0;i<AW_VEHICLE_FAMILIES;i++)free(f->weights[i]);memset(f,0,sizeof(*f));
}
static int aw_fleet_local_end(const AwLocalRoute*r,int start){
    int end=aw_clamp(start+1,1,r->count-1);float length=r->family==AW_VEHICLE_WING?40:22;
    while(end<r->count-1&&r->distance[end]-r->distance[start]<length)end++;return end;
}
static void aw_fleet_scout_route(AwFleet*f,const AwMap*m,int start){
    AwPatrol p={.layer=0,.variant=0,.count=m->path_length};memcpy(p.route,m->path,p.count*sizeof(int));if(p.count>=2&&start==0&&m->path[0]==m->spawns[0]&&m->path[m->path_length-1]==m->spawns[1])aw_patrol_scout(m,&p,m->spawns[0],m->spawns[1]);
    aw_local_route(m,&p,&f->route[0]);
    AwLocalRoute*r=&f->route[0];if(r->count<2){f->active[0]=0;return;}
    start=aw_clamp(start,0,r->count-2);AwSVec d=aw_sv_add(r->point[start+1],aw_sv_scale(r->point[start],-1));
    aw_local_reset(&f->unit[0],r,start,aw_fleet_local_end(r,start),atan2f(d.x,d.z),500);f->active[0]=1;f->terminal[0]=1;f->body[0]=aw_vehicle_body(&f->unit[0].vehicle);f->previous[0]=f->unit[0].vehicle;
}
static void aw_fleet_init(AwFleet*f,const AwMap*m,const AwPatrols*p){
    aw_fleet_close(f);aw_ray_world_init(&f->rays,m);aw_fleet_scout_route(f,m,0);
    int sizes[]={3,3,3,3};f->trained=1;
    for(int k=0;k<AW_VEHICLE_FAMILIES;k++){
        char file[128];snprintf(file,sizeof(file),"resources/alienwars/local-%d.bin",k);f->weights[k]=load_weights(file);
        if(!f->weights[k]){f->trained=0;continue;}
        int expected=128*AW_LOCAL_INPUTS+13*128+2*3*128*128;
        if(f->weights[k]->size-7!=expected){free(f->weights[k]);f->weights[k]=NULL;f->trained=0;continue;}
        for(int j=0;j<expected;j++)if(!isfinite(f->weights[k]->data[j])){free(f->weights[k]);f->weights[k]=NULL;f->trained=0;break;}
    }
    for(int i=0;i<AW_UNITS;i++){
        if(i){aw_local_route(m,&p->units[i-1],&f->route[i]);AwLocalRoute*r=&f->route[i];if(r->count<2)continue;
            int start=(i*19)%(r->count-1);AwSVec d=aw_sv_add(r->point[start+1],aw_sv_scale(r->point[start],-1));
            aw_local_reset(&f->unit[i],r,start,aw_fleet_local_end(r,start),atan2f(d.x,d.z),500);f->active[i]=1;}
        if(!f->active[i])continue;
        AwVehicle*v=&f->unit[i].vehicle;f->body[i]=aw_vehicle_body(v);f->previous[i]=*v;f->terminal[i]=1;
        Weights*w=f->weights[v->family];if(w){w->idx=0;f->net[i]=make_puffernet(w,1,AW_LOCAL_INPUTS,128,2,sizes,4);}
    }f->ready=1;
}
static void aw_fleet_next(AwFleet*f,int i){
    AwLocalEpisode*e=&f->unit[i];AwLocalRoute*r=&f->route[i];
    if(e->end<r->count-1){
        int start=e->end;e->cursor=start+1;e->end=aw_fleet_local_end(r,start);
        e->success=e->timeout=e->ticks=0;e->last_remaining=aw_local_remaining(e);f->terminal[i]=1;return;
    }
    /* Ground and surface routes return along their traversable corridor.
     * Volume routes already contain a closing leg. No pose teleport at arrival. */
    if(r->family==AW_VEHICLE_GROUND||r->family==AW_VEHICLE_BOAT){
        for(int a=0,b=r->count-1;a<b;a++,b--){AwSVec t=r->point[a];r->point[a]=r->point[b];r->point[b]=t;int n=r->node[a];r->node[a]=r->node[b];r->node[b]=n;}
        r->distance[0]=0;for(int j=1;j<r->count;j++)r->distance[j]=r->distance[j-1]+aw_sv_length(aw_sv_add(r->point[j],aw_sv_scale(r->point[j-1],-1)));
    }
    e->cursor=1;e->end=aw_fleet_local_end(r,0);e->success=e->timeout=e->ticks=0;e->last_remaining=aw_local_remaining(e);f->terminal[i]=1;f->arrivals[i]++;
}
static AwVehicle aw_fleet_pose(const AwFleet*f,int i){
    AwVehicle pose=f->unit[i].vehicle;const AwVehicle*old=&f->previous[i];float t=fminf(1,fmaxf(0,f->accumulator/.1f));
    pose.position=aw_sv_add(old->position,aw_sv_scale(aw_sv_add(pose.position,aw_sv_scale(old->position,-1)),t));
    pose.yaw=aw_motion_angle(old->yaw+aw_motion_angle(pose.yaw-old->yaw)*t);pose.pitch=aw_lerp(old->pitch,pose.pitch,t);return pose;
}
static void aw_fleet_step(AwFleet*f,const AwMap*m,float dt,int scout_live,int others_live){
    if(!f->ready)return;f->accumulator+=dt;
    while(f->accumulator>=.1f){f->accumulator-=.1f;
        for(int i=0;i<AW_UNITS;i++){f->previous[i]=f->unit[i].vehicle;if(!f->active[i]||!(i?others_live:scout_live))continue;
            AwLocalEpisode*e=&f->unit[i];float actions[4];
            if(e->success)aw_fleet_next(f,i);
            if(e->timeout){e->ticks=e->timeout=0;f->terminal[i]=1;}
            aw_local_observe(e,m,&f->rays,f->body,AW_UNITS,i,.1f);
            if(f->net[i]){forward_puffernet(f->net[i],e->observation,actions,NULL,&f->terminal[i]);multidiscrete(f->net[i]->multidiscrete,f->net[i]->decoder->output,actions,1,NULL);}
            else aw_local_reference(e,actions);
            f->terminal[i]=0;aw_local_step(e,m,actions,f->body,AW_UNITS,i);f->body[i]=aw_vehicle_body(&e->vehicle);
        }
    }
}
#endif
