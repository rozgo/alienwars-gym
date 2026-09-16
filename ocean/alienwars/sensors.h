#ifndef ALIENWARS_SENSORS_H
#define ALIENWARS_SENSORS_H
#include "sensor_rays.h"
#define AW_SENSOR_VERSION 1
#define AW_SENSOR_UNITS 16
#define AW_SENSOR_TYPES 4
#define AW_SENSOR_RAYS 48
#define AW_SENSOR_SLOT_OBS (6+AW_SENSOR_RAYS*3)
#define AW_SENSOR_OBS (13+AW_SENSOR_TYPES*AW_SENSOR_SLOT_OBS)
#define AW_SENSOR_PI 3.14159265358979323846f
enum {AW_SENSOR_LIDAR,AW_SENSOR_SONAR,AW_SENSOR_RF,AW_SENSOR_CAMERA};
typedef struct {AwSVec position;float yaw,pitch,roll;} AwSensorPose;
typedef struct {
    AwSVec delta,velocity,angle_delta,angular_velocity;float distance;
} AwOdometry;
typedef struct {
    int enabled;float range,period,hfov,vfov;AwSensorPose mount;
} AwSensorConfig;
typedef struct {AwSVec direction;AwSensorHit hit;} AwSensorBeam;
typedef struct {float distance,bearing,elevation,strength;int detected;} AwRFReturn;
typedef struct {
    int valid,count;uint32_t sequence;double stamp,next;
    /* World mount origin and acquisition BODY orientation. Beam directions
     * also include mount rotation; RF bearing is relative to this body yaw. */
    AwSensorPose pose;AwSensorBeam beams[AW_SENSOR_RAYS];AwRFReturn peers[AW_SENSOR_UNITS];
} AwSensorReading;
typedef struct {
    int active,initialized,layer;float radius;AwSensorPose pose,previous;
    /* Optional physical hull. Zero extents preserve legacy spherical fixtures. */
    AwSVec body_center,body_extent;float body_yaw;
    AwOdometry odometry;AwSensorConfig config[AW_SENSOR_TYPES];
    AwSensorReading reading[AW_SENSOR_TYPES];
} AwSensorUnit;
typedef struct {
    AwRayWorld rays;double time;int count;
    AwSensorUnit units[AW_SENSOR_UNITS];
    /* Fixed contiguous policy data; exact global pose is a separate privileged
     * channel, never silently mixed into perceptual/odometry observations. */
    float observations[AW_SENSOR_UNITS][AW_SENSOR_OBS];
    float privileged_pose[AW_SENSOR_UNITS][9];
    uint64_t ray_queries,samples;
} AwSensors;
static float aw_sensor_angle(float x){return atan2f(sinf(x),cosf(x));}
/* Body axes: +x right, +y up, +z forward. Positive yaw turns toward +x;
 * positive pitch raises the nose, positive roll raises the right wing. */
static AwSVec aw_sensor_rotate(AwSVec v,AwSensorPose p){
    float cr=cosf(p.roll),sr=sinf(p.roll),cp=cosf(p.pitch),sp=sinf(p.pitch),cy=cosf(p.yaw),sy=sinf(p.yaw);
    float x=cr*v.x-sr*v.y,y=sr*v.x+cr*v.y,z=v.z;
    float yp=cp*y+sp*z,zp=-sp*y+cp*z;
    return (AwSVec){cy*x+sy*zp,yp,-sy*x+cy*zp};
}
static AwSensorConfig aw_sensor_default(int type,int layer){
    AwSensorConfig c={.enabled=1,.range=24,.period=.2f,.hfov=2*AW_SENSOR_PI,
        .mount={.position={0,layer>=2?0:1,0}}};
    if(type==AW_SENSOR_SONAR){c.range=32;c.period=.5f;c.hfov=AW_SENSOR_PI*.85f;c.vfov=AW_SENSOR_PI*.25f;c.mount.position.y=-.55f;c.mount.pitch=layer==3?0:-.55f;}
    if(type==AW_SENSOR_RF){c.range=64;c.period=.5f;}
    if(type==AW_SENSOR_CAMERA){c.range=36;c.period=.5f;c.hfov=AW_SENSOR_PI*.5f;c.vfov=AW_SENSOR_PI*.34f;c.mount.pitch=layer==2?-.65f:-.12f;}
    c.enabled=type==AW_SENSOR_SONAR?(layer==1||layer==3):type==AW_SENSOR_LIDAR?(layer!=1&&layer!=3):1;return c;
}
static void aw_sensors_init(AwSensors*s,const AwMap*m,int count){
    memset(s,0,sizeof(*s));s->count=aw_clamp(count,0,AW_SENSOR_UNITS);aw_ray_world_init(&s->rays,m);
}
/* Attaching / reconfiguring invalidates only this module. Geometry and RNG
 * remain untouched. The same mount interface is valid on every unit layer. */
static int aw_sensor_attach(AwSensors*s,int unit,int type,AwSensorConfig c){
    if(unit<0||unit>=s->count||type<0||type>=AW_SENSOR_TYPES)return 0;
    if(!isfinite(c.range)||!isfinite(c.period)||!isfinite(c.hfov)||!isfinite(c.vfov)||
       !isfinite(c.mount.position.x)||!isfinite(c.mount.position.y)||!isfinite(c.mount.position.z)||
       !isfinite(c.mount.yaw)||!isfinite(c.mount.pitch)||!isfinite(c.mount.roll))return 0;
    c.enabled=!!c.enabled;c.range=fminf(128,fmaxf(type==AW_SENSOR_RF?2:.1f,c.range));c.period=fminf(10,fmaxf(1.0f/60,c.period));
    c.hfov=fminf(2*AW_SENSOR_PI,fmaxf(.01f,c.hfov));c.vfov=fminf(AW_SENSOR_PI*.95f,fmaxf(0,c.vfov));
    if(type==AW_SENSOR_CAMERA)c.hfov=fminf(AW_SENSOR_PI*.95f,c.hfov);
    s->units[unit].config[type]=c;memset(&s->units[unit].reading[type],0,sizeof(AwSensorReading));
    s->units[unit].reading[type].next=s->time;return 1;
}
static void aw_sensor_equip(AwSensors*s,int unit,int layer,float radius){
    if(unit<0||unit>=s->count)return;AwSensorUnit*u=&s->units[unit];u->active=1;u->layer=layer;u->radius=radius;
    for(int t=0;t<AW_SENSOR_TYPES;t++)aw_sensor_attach(s,unit,t,aw_sensor_default(t,layer));
}
static void aw_sensor_teleport(AwSensors*s,int unit){
    if(unit<0||unit>=s->count)return;AwSensorUnit*u=&s->units[unit];u->initialized=0;
    memset(&u->odometry,0,sizeof(u->odometry));
    for(int t=0;t<AW_SENSOR_TYPES;t++){u->reading[t].valid=0;u->reading[t].next=s->time;}
}
static AwSensorHit aw_sensor_cast(AwSensors*s,const AwMap*m,int owner,AwSVec o,AwSVec d,float range,int water){
    s->ray_queries++;AwSensorHit hit=aw_ray_terrain(m,&s->rays,o,d,range,water);
    for(int i=0;i<s->count;i++)if(i!=owner&&s->units[i].active){
        const AwSensorUnit*body=&s->units[i];
        if(body->body_extent.x>0){
            float t=aw_ray_box(body->body_center,body->body_extent,body->body_yaw,o,d,range);
            if(t<hit.distance)hit=(AwSensorHit){t,AW_HIT_UNIT,i};continue;
        }
        AwSVec center=s->units[i].pose.position;if(s->units[i].layer==0)center.y+=.5f;
        AwSVec delta=aw_sv_add(o,aw_sv_scale(center,-1));float b=aw_sv_dot(delta,d),c=aw_sv_dot(delta,delta)-s->units[i].radius*s->units[i].radius;
        float discriminant=b*b-c;if(discriminant<0)continue;
        float t=c<=0?0:-b-sqrtf(discriminant);if(t>=0&&t<hit.distance)hit=(AwSensorHit){t,AW_HIT_UNIT,i};
    }return hit;
}
static void aw_sensor_sample(AwSensors*s,const AwMap*m,int id,int type){
    AwSensorUnit*u=&s->units[id];AwSensorConfig*c=&u->config[type];AwSensorReading*r=&u->reading[type];
    r->pose=u->pose;r->pose.position=aw_sv_add(u->pose.position,aw_sensor_rotate(c->mount.position,u->pose));
    r->stamp=s->time;r->sequence++;r->valid=aw_sensor_in_world(r->pose.position)&&aw_ray_density(m,r->pose.position)<=0;
    r->count=type==AW_SENSOR_LIDAR?32:type==AW_SENSOR_SONAR?24:type==AW_SENSOR_CAMERA?48:0;
    if(type==AW_SENSOR_SONAR){
        float bed=aw_ocean_bed_q(m,r->pose.position.x*.5f,r->pose.position.z*.5f)*.75f-1.2f;
        r->valid&=r->pose.position.y<-.12f&&r->pose.position.y>bed; /* A dry underground cave is not water. */
    }
    memset(r->peers,0,sizeof(r->peers));
    if(!r->valid)return;s->samples++;
    if(type==AW_SENSOR_RF){
        for(int j=0;j<s->count;j++)if(j!=id&&s->units[j].active&&s->units[j].config[AW_SENSOR_RF].enabled){
            AwSVec receiver=aw_sv_add(s->units[j].pose.position,aw_sensor_rotate(s->units[j].config[AW_SENSOR_RF].mount.position,s->units[j].pose));
            AwSVec delta=aw_sv_add(receiver,aw_sv_scale(r->pose.position,-1));float distance=aw_sv_length(delta);
            if(distance<.001f||distance>c->range)continue;
            s->ray_queries++;AwSensorHit blocked=aw_ray_terrain(m,&s->rays,r->pose.position,aw_sv_scale(delta,1/distance),distance,0);
            /* Deliberate game model, not electromagnetic simulation: inverse
             * square path loss plus one 18 dB obstruction penalty. */
            float db=-20*log10f(fmaxf(1,distance))-(blocked.kind==AW_HIT_TERRAIN?18:0);
            float budget=20*log10f(c->range);float strength=fmaxf(0,1+db/budget);
            if(strength>0)r->peers[j]=(AwRFReturn){distance,aw_sensor_angle(atan2f(delta.x,delta.z)-u->pose.yaw),atan2f(delta.y,hypotf(delta.x,delta.z)),strength,1};
        }return;
    }
    for(int i=0;i<r->count;i++){
        AwSVec local;
        if(type==AW_SENSOR_CAMERA){
            /* 8x6 pinhole depth image, row-major, with radial range returns. */
            float x=(2*((i%8)+.5f)/8-1)*tanf(c->hfov*.5f),y=(1-2*((i/8)+.5f)/6)*tanf(c->vfov*.5f);
            local=aw_sv_normal((AwSVec){x,y,1});
        }else{
            float angle=c->hfov*i/r->count-c->hfov*.5f;
            /* Three vertical beams at each of eight sonar azimuths. */
            if(type==AW_SENSOR_SONAR)angle=c->hfov*((i/3)/7.0f-.5f);
            float elevation=type==AW_SENSOR_SONAR?c->vfov*((i%3)/2.0f-.5f):0;
            local=(AwSVec){sinf(angle)*cosf(elevation),sinf(elevation),cosf(angle)*cosf(elevation)};
        }
        AwSVec direction=aw_sensor_rotate(aw_sensor_rotate(local,c->mount),u->pose);
        r->beams[i]=(AwSensorBeam){direction,aw_sensor_cast(s,m,id,r->pose.position,direction,c->range,1)};
    }
}
static float aw_sensor_clip(float x){return fminf(1,fmaxf(-1,x));}
static void aw_sensor_pack(AwSensors*s,int id){
    AwSensorUnit*u=&s->units[id];float*out=s->observations[id];memset(out,0,sizeof(s->observations[id]));
    memset(s->privileged_pose[id],0,sizeof(s->privileged_pose[id]));if(!u->active)return;
    AwOdometry*d=&u->odometry;float odom[13]={d->delta.x/4,d->delta.y/4,d->delta.z/4,d->angle_delta.x/AW_SENSOR_PI,d->angle_delta.y/AW_SENSOR_PI,d->angle_delta.z/AW_SENSOR_PI,
        d->velocity.x/16,d->velocity.y/16,d->velocity.z/16,d->angular_velocity.x/8,d->angular_velocity.y/8,d->angular_velocity.z/8,d->distance/1024};
    for(int k=0;k<13;k++)out[k]=aw_sensor_clip(odom[k]);
    float privileged[9]={(u->pose.position.x-64)/96,u->pose.position.y/64,(u->pose.position.z-64)/96,sinf(u->pose.yaw),cosf(u->pose.yaw),sinf(u->pose.pitch),cosf(u->pose.pitch),sinf(u->pose.roll),cosf(u->pose.roll)};
    for(int k=0;k<9;k++)s->privileged_pose[id][k]=aw_sensor_clip(privileged[k]);
    for(int t=0;t<AW_SENSOR_TYPES;t++){
        AwSensorConfig*c=&u->config[t];AwSensorReading*r=&u->reading[t];float*p=out+13+t*AW_SENSOR_SLOT_OBS;
        if(!c->enabled)continue;p[0]=1;p[1]=(float)r->valid;p[2]=r->sequence?fminf(1,(float)(s->time-r->stamp)/c->period):1;
        p[3]=c->range/128;p[4]=c->hfov/(2*AW_SENSOR_PI);p[5]=c->vfov/AW_SENSOR_PI;if(!r->valid)continue;
        if(t==AW_SENSOR_RF){for(int j=0;j<s->count;j++)if(r->peers[j].detected){AwRFReturn*v=&r->peers[j];float*b=p+6+j*6;
            b[0]=1;b[1]=v->distance/c->range;b[2]=sinf(v->bearing);b[3]=cosf(v->bearing);b[4]=sinf(v->elevation);b[5]=v->strength;}}
        else for(int j=0;j<r->count;j++){AwSensorHit h=r->beams[j].hit;p[6+3*j]=h.distance/c->range;p[7+3*j]=h.kind/4.0f;p[8+3*j]=(h.entity+1)/(float)AW_SENSOR_UNITS;}
    }
}
/* Caller supplies all poses before stepping. Fixed dt in training; viewer dt
 * may vary. No catch-up burst: late samples explicitly carry their timestamp.
 * Module phases spread cost across steps after the first complete snapshot. */
static void aw_sensors_step(AwSensors*s,const AwMap*m,float dt){
    if(!isfinite(dt)||dt<=0||dt>1)return;s->time+=dt;
    for(int id=0;id<s->count;id++){
        AwSensorUnit*u=&s->units[id];if(!u->active){aw_sensor_pack(s,id);continue;}
        AwOdometry*d=&u->odometry;
        if(u->initialized){
            AwSVec delta=aw_sv_add(u->pose.position,aw_sv_scale(u->previous.position,-1));
            /* Dot against previous body axes is the inverse mount rotation. */
            d->delta=(AwSVec){aw_sv_dot(delta,aw_sensor_rotate((AwSVec){1,0,0},u->previous)),aw_sv_dot(delta,aw_sensor_rotate((AwSVec){0,1,0},u->previous)),aw_sv_dot(delta,aw_sensor_rotate((AwSVec){0,0,1},u->previous))};
            d->velocity=aw_sv_scale(d->delta,1/dt);d->distance+=aw_sv_length(delta);
            d->angle_delta=(AwSVec){aw_sensor_angle(u->pose.pitch-u->previous.pitch),aw_sensor_angle(u->pose.yaw-u->previous.yaw),aw_sensor_angle(u->pose.roll-u->previous.roll)};
            d->angular_velocity=aw_sv_scale(d->angle_delta,1/dt);
        }u->previous=u->pose;u->initialized=1;
        for(int t=0;t<AW_SENSOR_TYPES;t++){
            AwSensorConfig*c=&u->config[t];AwSensorReading*r=&u->reading[t];if(!c->enabled)continue;
            if(s->time+1e-7>=r->next){
                int first=!r->sequence;aw_sensor_sample(s,m,id,t);
                if(first)r->next=s->time+c->period*(.25+.75*((id*7+t*3)%17)/17.0);
                else{double intervals=floor((s->time-r->next+1e-7)/c->period)+1;r->next+=fmax(1,intervals)*c->period;}
            }
        }aw_sensor_pack(s,id);
    }
}
#endif
