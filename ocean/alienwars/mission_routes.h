#ifndef ALIENWARS_MISSION_ROUTES_H
#define ALIENWARS_MISSION_ROUTES_H
#include "vehicles.h"

#define AW_MISSION_POINTS 2048
#define AW_FLIGHT_LABELS 48000
#define AW_FLIGHT_BINS (32*32*10*32*3*3)
typedef struct {
    int count, family, variant;
    AwSVec point[AW_MISSION_POINTS];
    float distance[AW_MISSION_POINTS], heading[AW_MISSION_POINTS];
    AwDrive drive[AW_MISSION_POINTS];
    int node[AW_MISSION_POINTS];
    AwVehicle start;
} AwMissionRoute;
typedef struct {
    AwVehicle vehicle;
    AwDrive drive;
    float cost, priority;
    int parent, bin;
} AwFlightLabel;
typedef struct {
    const AwMap *map;
    AwAStar search;
    unsigned char ground[3][AW_NODES], boat[3][AW_OCEAN_CELLS];
    AwVolumeGraph volume[4]; /* quad, then three submarine hulls */
    AwFlightLabel *labels;
    int *best, *heap, count, queued, expanded;
    int path[AW_SUB_NODES];
} AwMissionPlanner;

static int aw_mission_append(AwMissionRoute*r,AwSVec p,float yaw,AwDrive drive,int node){
    if(r->count>=AW_MISSION_POINTS)return 0;
    int i=r->count++;
    r->point[i]=p;r->heading[i]=yaw;r->drive[i]=drive;r->node[i]=node;
    r->distance[i]=i?r->distance[i-1]+aw_sv_length(aw_sv_add(p,aw_sv_scale(r->point[i-1],-1))):0;
    return 1;
}
static void aw_mission_planner_close(AwMissionPlanner*p){
    aw_astar_close(&p->search);
    for(int i=0;i<4;i++)free(p->volume[i].allowed);
    free(p->labels);free(p->best);free(p->heap);memset(p,0,sizeof(*p));
}
static int aw_mission_planner_init(AwMissionPlanner*p,const AwMap*m){
    memset(p,0,sizeof(*p));p->map=m;
    if(!aw_astar_init(&p->search,AW_SUB_NODES))goto fail;
    p->labels=calloc(AW_FLIGHT_LABELS,sizeof(*p->labels));
    p->best=malloc(AW_FLIGHT_BINS*sizeof(int));p->heap=malloc(AW_FLIGHT_LABELS*sizeof(int));
    if(!p->labels||!p->best||!p->heap)goto fail;
    for(int v=0;v<3;v++){
        for(int c=0;c<AW_CELLS;c++)p->ground[v][c]=m->walkable[c]&&aw_body_fits(m,c%64+.5f,aw_surface_q(m,c,.5f,.5f),c/64+.5f,v!=0);
        for(int c=0;c<m->cave_count;c++){
            const AwCaveNode*n=&m->cave[c];
            p->ground[v][AW_CELLS+c]=aw_body_fits(m,n->x+.5f,n->q,n->z+.5f,v!=0);
        }
        for(int c=0;c<m->span_count;c++)p->ground[v][AW_SPAN_START+c]=!!(m->spans[c].fits&(v?2:1));
        for(int c=0;c<AW_OCEAN_CELLS;c++)p->boat[v][c]=aw_patrol_water_cell(m,c,v);
    }
    for(int i=0;i<4;i++)if(!aw_volume_init(&p->volume[i],m,i!=0,i?i-1:0))goto fail;
    return 1;
fail:
    aw_mission_planner_close(p);return 0;
}
static AwSVec aw_mission_world(AwRoutePoint p){return (AwSVec){p.x*2,p.q*.75f-1.2f,p.z*2};}
static AwRoutePoint aw_mission_grid(AwSVec p){return (AwRoutePoint){p.x*.5f,(p.y+1.2f)/.75f,p.z*.5f};}

/* Labels are immutable. A later better representative for a discretized bin
 * cannot invalidate the continuous parent trajectory already stored by a child. */
static int aw_flight_bin(const AwVehicle*v){
    int x=(int)floorf((v->position.x+32)/6),z=(int)floorf((v->position.z+32)/6);
    int y=(int)floorf(v->position.y/6),h=(int)floorf((aw_motion_angle(v->yaw)+AW_MOTION_PI)*32/(2*AW_MOTION_PI))&31;
    if(x<0||z<0||x>=32||z>=32||y<0||y>=10)return -1;
    float turn=aw_vehicle_spec(v->family,v->variant).turn;
    int rate=v->yaw_rate<-.25f*turn?0:v->yaw_rate>.25f*turn?2:1;
    int climb=v->velocity.y<-.4f?0:v->velocity.y>.4f?2:1;
    return (((((y*32+z)*32+x)*32+h)*3+rate)*3+climb);
}
static int aw_flight_less(const AwMissionPlanner*p,int a,int b){
    return p->labels[a].priority<p->labels[b].priority||(p->labels[a].priority==p->labels[b].priority&&a<b);
}
static int aw_flight_push(AwMissionPlanner*p,AwVehicle v,AwDrive drive,int parent,float cost,AwSVec goal){
    int bin=aw_flight_bin(&v);if(bin<0||p->count>=AW_FLIGHT_LABELS)return 0;
    /* Weighted A* deliberately favors latency over optimality. Returned paths
     * are still replay-validated; exhausted requests remain unavailable. */
    float estimate=1.8f*aw_sv_length(aw_sv_add(goal,aw_sv_scale(v.position,-1)))/aw_vehicle_spec(v.family,v.variant).reverse;
    int previous=p->best[bin];
    if(previous>=0&&(p->labels[previous].cost<cost-.0001f||
        (p->labels[previous].cost<=cost+.0001f&&p->labels[previous].priority<=cost+estimate)))return 0;
    int id=p->count++;p->best[bin]=id;
    p->labels[id]=(AwFlightLabel){v,drive,cost,cost+estimate,parent,bin};
    int at=p->queued++;while(at){int up=(at-1)/2;if(!aw_flight_less(p,id,p->heap[up]))break;p->heap[at]=p->heap[up];at=up;}p->heap[at]=id;
    return 1;
}
static int aw_flight_pop(AwMissionPlanner*p){
    int id=p->heap[0],last=p->heap[--p->queued],at=0;
    if(p->queued){while(at*2+1<p->queued){int child=at*2+1;if(child+1<p->queued&&aw_flight_less(p,p->heap[child+1],p->heap[child]))child++;
        if(!aw_flight_less(p,p->heap[child],last))break;p->heap[at]=p->heap[child];at=child;}p->heap[at]=last;}return id;
}
/* Every primitive is replayed through the real 30 Hz dynamics. The search
 * retains yaw-rate state; it cannot turn an aircraft instantly at a grid node.
 * Discretization and a label cap make this a bounded feasible search, not a
 * completeness or optimality claim. */
static int aw_mission_flight(AwMissionPlanner*p,const AwVehicle*start,AwSVec goal,AwMissionRoute*r){
    memset(r,0,sizeof(*r));r->family=start->family;r->variant=start->variant;r->start=*start;
    p->count=p->queued=p->expanded=0;
    for(int i=0;i<AW_FLIGHT_BINS;i++)p->best[i]=-1;
    AwVehicle check=*start,target=*start;target.position=goal;
    if(!aw_vehicle_clear(p->map,&check)||!aw_vehicle_clear(p->map,&target))return 0;
    aw_flight_push(p,*start,(AwDrive){0},-1,0,goal);
    int found=-1;AwVehicleSpec spec=aw_vehicle_spec(start->family,start->variant);
    while(p->queued&&p->count<AW_FLIGHT_LABELS){
        int id=aw_flight_pop(p);AwFlightLabel label=p->labels[id];
        if(p->best[label.bin]!=id)continue;p->expanded++;
        AwSVec delta=aw_sv_add(goal,aw_sv_scale(label.vehicle.position,-1));
        if(hypotf(delta.x,delta.z)<4&&fabsf(delta.y)<2){found=id;break;}
        for(int up=-1;up<=1;up++)for(int turn=-1;turn<=1;turn++){
            AwVehicle v=label.vehicle;AwDrive drive={spec.reverse,turn*spec.turn*.8f,up*spec.reverse*.18f,0};
            for(int t=0;t<45&&!v.failed;t++)aw_vehicle_drive(p->map,&v,drive,1.0f/30,NULL,0,-1);
            if(!v.failed)aw_flight_push(p,v,drive,id,label.cost+1.5f,goal);
        }
    }
    if(found<0)return 0;
    int n=0;for(int id=found;id>=0;id=p->labels[id].parent){if(n>=AW_SUB_NODES)return 0;p->path[n++]=id;}
    AwVehicle v=*start;aw_mission_append(r,v.position,v.yaw,(AwDrive){spec.reverse,0,0,0},-1);
    for(int i=n-2;i>=0;i--){AwDrive drive=p->labels[p->path[i]].drive;
        for(int t=0;t<45;t++){aw_vehicle_drive(p->map,&v,drive,1.0f/30,NULL,0,-1);
            if(v.failed||((t%3)==2&&!aw_mission_append(r,v.position,v.yaw,drive,-1))){r->count=0;return 0;}}
    }
    return r->count>1;
}

static int aw_mission_segment(const AwMap*m,const AwVehicle*vehicle,AwSVec a,AwSVec b){
    AwSVec d=aw_sv_add(b,aw_sv_scale(a,-1));float distance=aw_sv_length(d);
    int count=(int)ceilf(distance/.25f);if(count<1)count=1;
    AwVehicle probe=*vehicle;probe.yaw=atan2f(d.x,d.z);
    for(int i=0;i<=count;i++){
        probe.position=aw_sv_add(a,aw_sv_scale(d,(float)i/count));float y=probe.position.y;
        if(!aw_vehicle_clear(m,&probe))return 0;
        if(vehicle->family==AW_VEHICLE_GROUND&&fabsf(probe.position.y-y)>.23f)return 0;
    }return 1;
}
/* Generic graph routes keep stacked ground levels distinct and snap only to
 * nearby, physically connected endpoints. Requests on the wrong medium fail. */
static int aw_mission_plan(AwMissionPlanner*p,const AwVehicle*start,AwSVec goal,AwMissionRoute*r){
    if(!isfinite(goal.x)||!isfinite(goal.y)||!isfinite(goal.z)||start->family<0||start->family>=5||start->variant<0||start->variant>2){r->count=0;return 0;}
    if(start->family==AW_VEHICLE_WING)return aw_mission_flight(p,start,goal,r);
    memset(r,0,sizeof(*r));r->family=start->family;r->variant=start->variant;r->start=*start;
    AwVehicle target=*start;target.position=goal;
    if(!aw_vehicle_clear(p->map,&target))return 0;goal=target.position;
    int first=-1,last=-1,nodes=0,n=0;float da=INFINITY,db=INFINITY;
    AwPatrolGraph graph={p->map,start->family==AW_VEHICLE_BOAT?AW_PATROL_NAVAL:AW_PATROL_GROUND,start->variant,NULL};
    AwVolumeGraph*volume=NULL;
    if(start->family==AW_VEHICLE_GROUND||start->family==AW_VEHICLE_BOAT){
        nodes=start->family==AW_VEHICLE_GROUND?AW_NODES:AW_OCEAN_CELLS;
        graph.allowed=start->family==AW_VEHICLE_GROUND?p->ground[start->variant]:p->boat[start->variant];
    }else{volume=&p->volume[start->family==AW_VEHICLE_QUAD?0:1+start->variant];nodes=volume->nodes;}
    for(int i=0;i<nodes;i++){
        if(!(volume?volume->allowed[i]:graph.allowed[i]))continue;
        AwSVec point=aw_mission_world(volume?aw_volume_position(volume,i):aw_patrol_node(&graph,i));
        float a=aw_sv_length(aw_sv_add(point,aw_sv_scale(start->position,-1))),b=aw_sv_length(aw_sv_add(point,aw_sv_scale(goal,-1)));
        if(a<da&&a<=(volume?12:4)&&aw_mission_segment(p->map,start,start->position,point)){da=a;first=i;}
        if(b<db&&b<=(volume?12:4)&&aw_mission_segment(p->map,start,point,goal)){db=b;last=i;}
    }
    if(first<0||last<0||da>(volume?12:4)||db>(volume?12:4))return 0;
    if(volume)n=aw_astar_path(&p->search,nodes,first,last,volume,aw_volume_edges,aw_volume_estimate,p->path,AW_MISSION_POINTS-2);
    else n=aw_astar_path(&p->search,nodes,first,last,&graph,aw_patrol_edges,aw_patrol_estimate,p->path,AW_MISSION_POINTS-2);
    if(!n)return 0;
    aw_mission_append(r,start->position,start->yaw,(AwDrive){0},first);
    for(int i=0;i<n;i++){
        AwSVec point=aw_mission_world(volume?aw_volume_position(volume,p->path[i]):aw_patrol_node(&graph,p->path[i]));
        if(aw_sv_length(aw_sv_add(point,aw_sv_scale(r->point[r->count-1],-1)))<.02f)continue;
        if(!aw_mission_segment(p->map,start,r->point[r->count-1],point)){r->count=0;return 0;}
        aw_mission_append(r,point,0,(AwDrive){0},volume?-1:p->path[i]);
    }
    if(aw_sv_length(aw_sv_add(goal,aw_sv_scale(r->point[r->count-1],-1)))>.02f)aw_mission_append(r,goal,0,(AwDrive){0},last);
    for(int i=1;i<r->count;i++){AwSVec d=aw_sv_add(r->point[i],aw_sv_scale(r->point[i-1],-1));r->heading[i]=atan2f(d.x,d.z);}
    return r->count>1;
}
#endif
