#ifndef ALIENWARS_NAVIGATION_CONTROL_H
#define ALIENWARS_NAVIGATION_CONTROL_H
/* Bounded local safety/recovery planner. It simulates the actual actuator model
 * against known static terrain and only sensor-derived dynamic tracks. It is an
 * explicit assist, measured separately from PPO, not a learned safety claim. */
static int aw_navigation_dynamic_clear(const AwMissionWorld*w,int id,const AwVehicle*v,float future){
    const AwMissionAgent*a=&w->agents[id];AwVehicleSpec self=aw_vehicle_spec(v->family,v->variant);
    for(int j=0;j<w->count;j++)if(j!=id&&a->tracks[j].valid){
        const AwNavTrack*t=&a->tracks[j];float age=(float)(w->sensors.time-t->stamp);
        if(age>1.5f)continue;
        AwSVec other=aw_sv_add(t->position,aw_sv_scale(t->velocity,future+fmaxf(0,age)));
        AwVehicleSpec spec=aw_vehicle_spec(w->agents[j].vehicle.family,w->agents[j].vehicle.variant);
        float vertical=(self.height+spec.height)*.5f+.3f;
        if(fabsf(other.y-v->position.y)>vertical)continue;
        float radius=hypotf(self.width,self.length)+hypotf(spec.width,spec.length)+.35f;
        float separation=hypotf(other.x-v->position.x,other.z-v->position.z);
        AwSVec now=aw_sv_add(t->position,aw_sv_scale(t->velocity,fmaxf(0,age)));
        float initial=hypotf(now.x-a->vehicle.position.x,now.z-a->vehicle.position.z);
        /* A conservative measurement envelope may already contain a valid
         * physical spawn. Always permit motion that opens that gap. */
        if(separation<radius&&separation<initial-.01f)return 0;
    }return 1;
}
static int aw_navigation_probe(const AwMissionWorld*w,const AwMap*m,int id,AwDrive d,AwVehicle*out){
    AwVehicle v=w->agents[id].vehicle;v.contact=0;v.contact_kind=0;
    /* Bounded at 6 substeps (15 for forward-only aircraft), using exactly the
     * physical integration rate. This is imminent-contact rejection, not a
     * guarantee against unseen traffic or arbitrary future maneuvers. */
    int moving=v.family==AW_VEHICLE_WING?15:6;
    for(int step=0;step<moving;step++){
        aw_vehicle_drive(m,&v,d,1.0f/30,NULL,0,-1);
        if(v.contact||v.failed||!aw_navigation_dynamic_clear(w,id,&v,(step+1)/30.0f))return 0;
        if(step==moving-1)*out=v;
    }return 1;
}
static AwDrive aw_navigation_control(AwMissionWorld*w,const AwMap*m,int id,AwDrive preferred){
    AwMissionAgent*a=&w->agents[id];AwVehicleSpec s=aw_vehicle_spec(a->vehicle.family,a->vehicle.variant);
    a->yielding=0;a->recovery_phase=0;
    if(a->replan_cooldown>0)a->replan_cooldown--;
    if(a->blocked_ticks>=50&&!a->replan_cooldown){
        a->replan_cooldown=100;
        if(aw_navigation_replan(w,m,id)){float neutral[4]={2,1,1,1};preferred=aw_mission_control(a,neutral);a->recovery_phase=3;}
    }
    /* Keep right for approaching traffic. The convention is relative to the
     * direction of travel, so opposing units choose opposite world-space sides.
     * Acquire early, and retain the side briefly after the peer passes. */
    if(a->avoid_ticks>0)a->avoid_ticks--;
    if(a->vehicle.family!=AW_VEHICLE_WING){
        float nearest=INFINITY;int peer=-1;
        for(int j=0;j<w->count;j++)if(j!=id&&a->tracks[j].valid){
            AwNavTrack*t=&a->tracks[j];AwSVec delta=aw_sv_add(t->position,aw_sv_scale(a->vehicle.position,-1));
            float front=delta.x*sinf(a->vehicle.yaw)+delta.z*cosf(a->vehicle.yaw);
            float side=delta.x*cosf(a->vehicle.yaw)-delta.z*sinf(a->vehicle.yaw);
            AwVehicleSpec other=aw_vehicle_spec(w->agents[j].vehicle.family,w->agents[j].vehicle.variant);
            float radius=s.width+other.width+.6f;
            float distance=aw_sv_length(delta);
            AwSVec relative=aw_sv_add(t->velocity,aw_sv_scale(a->vehicle.velocity,-1));
            if(front>0&&fabsf(side)<radius&&fabsf(delta.y)<(s.height+other.height)*.5f+.25f&&
               distance<s.length+other.length+s.speed*4&&aw_sv_dot(delta,relative)<-.1f&&distance<nearest){nearest=distance;peer=j;}
        }
        if(peer>=0){a->avoid_peer=peer;a->avoid_ticks=25;}
        /* A narrow passage has no hull-clear right-hand passing pocket. The
         * higher stable unit slot gives way, retreats briefly, then replans.
         * Both participants infer the conflict from their own RF history. */
        if(peer>=0&&id>peer){
            AwVehicle side=a->vehicle;side.position.x+=cosf(side.yaw)*(2*s.width+1);
            side.position.z-=sinf(side.yaw)*(2*s.width+1);
            if(side.family==AW_VEHICLE_GROUND)side.position.y=aw_support_q(m,side.position.x*.5f,side.position.z*.5f,(side.position.y+1.2f)/.75f)*.75f-1.2f;
            if(!aw_vehicle_clear(m,&side)){
                preferred.speed=a->yield_streak<20?0:-s.reverse*.5f;preferred.turn=0;preferred.strafe=0;
                AwVehicle result;if(aw_navigation_probe(w,m,id,preferred,&result)){
                    a->yielding=1;a->yield_ticks++;a->recovery_phase=1;a->safety_interventions++;return preferred;
                }
            }
        }
        if(a->avoid_ticks>0){
            AwSVec target=aw_mission_lookahead(a,4,NULL);float yaw=a->route->heading[aw_clamp(a->cursor,1,a->route->count-1)];
            float offset=s.width+1.8f;target.x+=cosf(yaw)*offset;target.z-=sinf(yaw)*offset;
            AwSVec delta=aw_sv_add(target,aw_sv_scale(a->vehicle.position,-1));
            float angle=aw_motion_angle(atan2f(delta.x,delta.z)-a->vehicle.yaw);
            preferred.turn=angle*2-a->vehicle.yaw_rate*.4f;
            preferred.speed=fminf(preferred.speed,s.speed*.65f*fmaxf(0,cosf(angle)));
        }
    }
    AwVehicle finish;
    if(aw_navigation_probe(w,m,id,preferred,&finish))return preferred;
    a->safety_interventions++;
    AwDrive best={0};float score=-INFINITY;int found=0;
    for(int k=0;k<12;k++){
        AwDrive d=preferred;
        if(k<3){d.speed=0;d.strafe=0;d.turn=(k-1)*s.turn*.65f;}
        else if(k<6){d.speed=-s.reverse*.7f;d.strafe=0;d.turn=(k-4)*s.turn*.65f;}
        else if(k<9){d.speed=s.speed*.35f;d.strafe=0;d.turn=(k-7)*s.turn*.65f;}
        else {d.speed=s.speed*.65f;d.turn=(k-10)*s.turn*.35f;}
        if(a->vehicle.family==AW_VEHICLE_WING){d.speed=s.reverse;d.climb=(k%3-1)*s.vertical*.5f;}
        if(!aw_navigation_probe(w,m,id,d,&finish))continue;
        AwMissionAgent projected=*a;projected.vehicle=finish;
        float gain=aw_mission_project(&projected)-a->previous_potential;
        AwSVec target=aw_mission_lookahead(&projected,1,NULL),delta=aw_sv_add(target,aw_sv_scale(finish.position,-1));
        float heading=fabsf(aw_motion_angle(atan2f(delta.x,delta.z)-finish.yaw));
        float value=gain-.25f*heading-.02f*fabsf(d.turn-preferred.turn);
        if(a->blocked_ticks>20&&a->recovery_ticks<15&&k>=3&&k<6)value+=.1f;
        if(value>score){score=value;best=d;found=1;}
    }
    if(!found){best=(AwDrive){0};if(a->vehicle.family==AW_VEHICLE_WING)best=preferred;
        a->yielding=1;a->yield_ticks++;a->recovery_phase=1;
    }else if(best.speed<-.01f){a->recovery_ticks++;a->recoveries+=a->recovery_ticks==1;a->recovery_phase=2;}
    else a->recovery_ticks=0;
    return best;
}
#endif
