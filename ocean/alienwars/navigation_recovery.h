#ifndef ALIENWARS_NAVIGATION_RECOVERY_H
#define ALIENWARS_NAVIGATION_RECOVERY_H
/* Bounded local A* reconnects to the immutable global route. All storage is
 * fixed stack/component memory. Layer-aware support keeps a ground detour on
 * its current bridge/cave level; every accepted edge is body-checked. */
enum {AW_RECOVERY_SIDE=17,AW_RECOVERY_PLANE=17*17,AW_RECOVERY_NODES=17*17*5};
typedef struct {
    const AwMap*map;AwVehicle vehicle;
    int nodes,layers;AwSVec point[AW_RECOVERY_NODES];unsigned char allowed[AW_RECOVERY_NODES];
} AwRecoveryGraph;
static int aw_recovery_edges(void*ctx,int node,AwAStarEdge*out){
    AwRecoveryGraph*g=ctx;int n=0,x=node%17,z=(node/17)%17,y=node/AW_RECOVERY_PLANE;
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++)if(dx||dz){
        int xx=x+dx,zz=z+dz;if(xx<0||zz<0||xx>=17||zz>=17)continue;
        int next=y*AW_RECOVERY_PLANE+zz*17+xx;if(!g->allowed[next])continue;
        if(aw_mission_segment(g->map,&g->vehicle,g->point[node],g->point[next]))out[n++]=(AwAStarEdge){next,aw_sv_length(aw_sv_add(g->point[next],aw_sv_scale(g->point[node],-1)))};
    }
    if(g->layers>1)for(int dy=-1;dy<=1;dy+=2){int yy=y+dy;if(yy<0||yy>=g->layers)continue;
        int next=node+dy*AW_RECOVERY_PLANE;if(g->allowed[next]&&aw_mission_segment(g->map,&g->vehicle,g->point[node],g->point[next]))out[n++]=(AwAStarEdge){next,1.5f};
    }return n;
}
static float aw_recovery_estimate(void*ctx,int a,int b){
    AwRecoveryGraph*g=ctx;return aw_sv_length(aw_sv_add(g->point[a],aw_sv_scale(g->point[b],-1)));
}
static int aw_navigation_replan(AwMissionWorld*w,const AwMap*m,int id){
    AwMissionAgent*a=&w->agents[id];if(a->vehicle.family==AW_VEHICLE_WING)return 0;
    AwRecoveryGraph g={.map=m,.vehicle=a->vehicle};
    g.layers=a->vehicle.family==AW_VEHICLE_QUAD||a->vehicle.family==AW_VEHICLE_SUB?5:1;g.nodes=AW_RECOVERY_PLANE*g.layers;
    AwSVec origin=a->vehicle.position;float q=(origin.y+1.2f)/.75f;
    AwVehicleSpec own=aw_vehicle_spec(a->vehicle.family,a->vehicle.variant);
    for(int n=0;n<g.nodes;n++){
        AwVehicle probe=a->vehicle;probe.position=(AwSVec){origin.x+n%17-8,origin.y+1.5f*(n/AW_RECOVERY_PLANE-g.layers/2),origin.z+(n/17)%17-8};
        if(probe.family==AW_VEHICLE_GROUND){float h=aw_support_q(m,probe.position.x*.5f,probe.position.z*.5f,q);
            if(fabsf(h-q)>3)continue;probe.position.y=h*.75f-1.2f;}
        int clear=1;
        for(int heading=0;heading<4&&clear;heading++){probe.yaw=heading*AW_MOTION_PI*.25f;clear=aw_vehicle_clear(m,&probe);}
        if(!clear)continue;
        for(int j=0;j<w->count&&clear;j++)if(j!=id&&a->tracks[j].valid){
            AwVehicleSpec other=aw_vehicle_spec(w->agents[j].vehicle.family,w->agents[j].vehicle.variant);AwSVec p=a->tracks[j].position;
            if(fabsf(p.y-aw_vehicle_body(&probe).position.y)>(own.height+other.height)*.5f+.2f+a->tracks[j].vertical_uncertainty)continue;
            if(hypotf(p.x-probe.position.x,p.z-probe.position.z)<hypotf(own.width,own.length)+hypotf(other.width,other.length)+.3f+a->tracks[j].horizontal_uncertainty)clear=0;
        }
        g.point[n]=probe.position;g.allowed[n]=clear;
    }
    int first=g.layers/2*AW_RECOVERY_PLANE+8*17+8;g.point[first]=origin;g.allowed[first]=1;
    int heap[AW_RECOVERY_NODES],slot[AW_RECOVERY_NODES],parent[AW_RECOVERY_NODES];
    float cost[AW_RECOVERY_NODES],priority[AW_RECOVERY_NODES];
    AwAStar search={.capacity=AW_RECOVERY_NODES,.heap=heap,.slot=slot,.parent=parent,.cost=cost,.priority=priority};int path[64];
    int previous_count=a->detour_count;a->detour_count=0;
    for(int ahead=8;ahead>=2;ahead-=2){
        AwSVec goal=aw_mission_lookahead(a,(float)ahead,NULL);int last=-1;float best=INFINITY;
        for(int n=0;n<g.nodes;n++)if(g.allowed[n]&&n!=first){
            float d=aw_sv_length(aw_sv_add(g.point[n],aw_sv_scale(goal,-1)));
            if(d<best&&d<1.75f&&aw_mission_segment(m,&g.vehicle,g.point[n],goal)){best=d;last=n;}
        }
        if(last<0)continue;
        int count=aw_astar_path(&search,g.nodes,first,last,&g,aw_recovery_edges,aw_recovery_estimate,path,63);
        if(count<2)continue;
        a->detour_count=0;a->detour_cursor=0;
        for(int k=1;k<count;k++)a->detour[a->detour_count++]=g.point[path[k]];
        a->detour[a->detour_count++]=goal;a->replans++;a->recoveries++;return 1;
    }
    a->detour_count=previous_count;a->replan_failures++;return 0;
}
#endif
