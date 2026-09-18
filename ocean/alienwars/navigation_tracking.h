#ifndef ALIENWARS_NAVIGATION_TRACKING_H
#define ALIENWARS_NAVIGATION_TRACKING_H
/* Track measured RF positions across acquisition timestamps. No simulator
 * neighbor pose/velocity is read. Known module mount offsets and hull profiles
 * are equipment metadata; lost/disabled measurements expire, never become
 * privileged replacement observations. */
static void aw_navigation_tracks(AwMissionWorld*w){
    for(int i=0;i<w->count;i++){
        AwMissionAgent*a=&w->agents[i];a->closing=0;a->clearance=20;
        if(a->control_version<3||!w->active[i])continue;
        const AwSensorUnit*u=&w->sensors.units[i];const AwSensorReading*r=&u->reading[AW_SENSOR_RF];
        for(int j=0;j<w->count;j++)if(j!=i){
            AwNavTrack*t=&a->tracks[j];const AwRFReturn*p=&r->peers[j];
            if(!u->config[AW_SENSOR_RF].enabled||!r->valid||!p->detected){t->valid=0;t->samples=0;continue;}
            if(!t->valid||r->stamp>t->stamp+1e-6){
                float angle=r->pose.yaw+p->bearing,h=p->distance*cosf(p->elevation);
                AwSVec position={r->pose.position.x+sinf(angle)*h,r->pose.position.y+sinf(p->elevation)*p->distance,
                    r->pose.position.z+cosf(angle)*h};
                /* RF default antenna sits vertically above the body pose. Horizontal mount offsets and pitch are not
                 * inferred from hidden neighbor orientation. The track remains an
                 * antenna-based approximation for custom displaced mounts. */
                position.y-=w->sensors.units[j].config[AW_SENSOR_RF].mount.position.y;
                double dt=r->stamp-t->stamp;AwSVec velocity={0};
                if(t->valid&&dt>.01&&dt<2){velocity=aw_sv_scale(aw_sv_add(position,aw_sv_scale(t->position,-1)),1/(float)dt);
                    float speed=aw_sv_length(velocity);if(speed>10)velocity=aw_sv_scale(velocity,10/speed);}
                t->velocity=velocity;t->position=position;t->stamp=r->stamp;t->valid=1;t->samples++;
            }
            if(w->sensors.time-t->stamp>1.5){t->valid=0;continue;}
            AwSVec delta=aw_sv_add(t->position,aw_sv_scale(a->vehicle.position,-1));float distance=aw_sv_length(delta);
            if(distance<a->clearance){a->clearance=distance;
                a->closing=distance>.01f?-aw_sv_dot(delta,aw_sv_add(t->velocity,aw_sv_scale(a->vehicle.velocity,-1)))/distance:0;}
        }
    }
}
#endif
