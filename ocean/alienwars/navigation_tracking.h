#ifndef ALIENWARS_NAVIGATION_TRACKING_H
#define ALIENWARS_NAVIGATION_TRACKING_H
/* Sensor-only track fusion. RF localizes the antenna; range beams localize a
 * visible hull surface. Hull metadata bounds the unobserved center offset. We
 * never read a neighbor's true pose, heading or velocity. */
static AwSVec aw_navigation_body_center(AwSVec p,int family,int variant){
    if(family==AW_VEHICLE_GROUND)p.y+=aw_vehicle_spec(family,variant).height*.5f;
    if(family==AW_VEHICLE_BOAT)p.y+=.2f;return p;
}
static void aw_navigation_tracks(AwMissionWorld*w){
    for(int i=0;i<w->count;i++){
        AwMissionAgent*a=&w->agents[i];a->closing=0;a->clearance=20;
        if(a->control_version<3||!w->active[i])continue;
        const AwSensorUnit*u=&w->sensors.units[i];const AwSensorReading*rf=&u->reading[AW_SENSOR_RF];
        AwSVec points[AW_SENSOR_UNITS]={{0}};int counts[AW_SENSOR_UNITS]={0};double stamps[AW_SENSOR_UNITS]={0};int source[AW_SENSOR_UNITS]={0};
        for(int type=0;type<4;type++)if(type!=AW_SENSOR_RF&&u->config[type].enabled){
            const AwSensorReading*r=&u->reading[type];if(!r->valid||w->sensors.time-r->stamp>1.5)continue;
            for(int k=0;k<r->count;k++){
                const AwSensorBeam*b=&r->beams[k];int j=b->hit.entity;
                if(b->hit.kind!=AW_HIT_UNIT||j<0||j>=w->count||j==i)continue;
                if(r->stamp>stamps[j]+1e-6){points[j]=(AwSVec){0};counts[j]=0;stamps[j]=r->stamp;source[j]=type+1;}
                if(fabs(r->stamp-stamps[j])>1e-6)continue;
                points[j]=aw_sv_add(points[j],aw_sv_add(r->pose.position,aw_sv_scale(b->direction,b->hit.distance)));counts[j]++;
            }
        }
        for(int j=0;j<w->count;j++)if(j!=i){
            AwNavTrack*t=&a->tracks[j];const AwRFReturn*p=&rf->peers[j];
            AwVehicleSpec spec=aw_vehicle_spec(w->agents[j].vehicle.family,w->agents[j].vehicle.variant);
            AwSVec position={0};double stamp=0;int provider=0;float horizontal=0,vertical=0;
            if(u->config[AW_SENSOR_RF].enabled&&rf->valid&&p->detected&&w->sensors.time-rf->stamp<=1.5){
                float angle=rf->pose.yaw+p->bearing,h=p->distance*cosf(p->elevation);
                position=(AwSVec){rf->pose.position.x+sinf(angle)*h,rf->pose.position.y+sinf(p->elevation)*p->distance,rf->pose.position.z+cosf(angle)*h};
                AwSVec mount=w->sensors.units[j].config[AW_SENSOR_RF].mount.position;
                position.y-=mount.y;
                position=aw_navigation_body_center(position,w->agents[j].vehicle.family,w->agents[j].vehicle.variant);
                /* Rotation of an off-center mount is unknown. A bound covers
                 * any yaw/pitch rather than consulting hidden orientation. */
                int family=w->agents[j].vehicle.family;
                horizontal=family==AW_VEHICLE_GROUND||family==AW_VEHICLE_BOAT?hypotf(mount.x,mount.z):aw_sv_length(mount);
                vertical=family==AW_VEHICLE_GROUND||family==AW_VEHICLE_BOAT?0:2*aw_sv_length(mount);
                stamp=rf->stamp;provider=AW_SENSOR_RF+1;
            }else if(counts[j]){
                position=aw_sv_scale(points[j],1.0f/counts[j]);stamp=stamps[j];provider=source[j];
                horizontal=hypotf(spec.width,spec.length);vertical=spec.height*.5f;
            }
            if(!provider){t->valid=0;t->samples=0;continue;}
            if(!t->valid||stamp>t->stamp+1e-6||provider!=t->source){
                double dt=stamp-t->stamp;AwSVec velocity={0};
                if(t->valid&&provider==t->source&&dt>.01&&dt<2){velocity=aw_sv_scale(aw_sv_add(position,aw_sv_scale(t->position,-1)),1/(float)dt);
                    float speed=aw_sv_length(velocity);if(speed>10)velocity=aw_sv_scale(velocity,10/speed);}
                t->velocity=velocity;t->position=position;t->stamp=stamp;t->valid=1;t->samples++;t->source=provider;
                t->horizontal_uncertainty=horizontal;t->vertical_uncertainty=vertical;
            }
            AwSVec center=aw_navigation_body_center(a->vehicle.position,a->vehicle.family,a->vehicle.variant);
            AwSVec delta=aw_sv_add(t->position,aw_sv_scale(center,-1));float distance=aw_sv_length(delta);
            if(distance<a->clearance){a->clearance=distance;
                a->closing=distance>.01f?-aw_sv_dot(delta,aw_sv_add(t->velocity,aw_sv_scale(a->vehicle.velocity,-1)))/distance:0;}
        }
    }
}
#endif
