#ifndef ALIENWARS_VEHICLES_H
#define ALIENWARS_VEHICLES_H
#include "patrols.h"
#include "sensor_rays.h"
#include "motion.h"
enum {AW_VEHICLE_GROUND,AW_VEHICLE_BOAT,AW_VEHICLE_QUAD,AW_VEHICLE_WING,AW_VEHICLE_SUB,AW_VEHICLE_FAMILIES};
typedef struct {float width,length,height,speed,reverse,accel,turn,turn_accel,vertical;} AwVehicleSpec;
typedef struct {int family,variant,contact,failed;AwSVec position,velocity;float yaw,yaw_rate,pitch; } AwVehicle;
typedef struct {AwSVec position,velocity;float yaw,width,length,height;int active;} AwBody;
static AwVehicleSpec aw_vehicle_spec(int f,int v){
    if(f==AW_VEHICLE_GROUND){static const AwVehicleSpec a[3]={{.58f,.60f,.85f,3,1.4f,5,1.4f,4,0},{.76f,1.02f,1.0f,2.5f,1,3,1.0f,2,0},{.76f,1.30f,1.15f,2.1f,.8f,2,.65f,1.5f,0}};return a[aw_clamp(v,0,2)];}
    if(f==AW_VEHICLE_BOAT)return (AwVehicleSpec){.4f+.16f*v,1.05f+.48f*v,.9f+.28f*v,2.8f-.4f*v,.6f,1.0f,.55f-.1f*v,.8f,0};
    if(f==AW_VEHICLE_QUAD)return (AwVehicleSpec){.94f,.94f,.65f,2.2f,2.2f,2.8f,1.3f,3,1.6f};
    if(f==AW_VEHICLE_WING)return (AwVehicleSpec){v==1?1.65f:2.45f,v==1?1.25f:1.65f,1.2f,v==1?7.5f:6.5f,v==1?5.0f:4.5f,1.7f,v==1?.32f:.25f,.45f,1.8f};
    return (AwVehicleSpec){.54f+.16f*v,1.20f+.4f*v,1.35f+.4f*v,2.1f-.25f*v,.6f,.8f,.50f-.08f*v,.6f,.75f};
}
static float aw_vehicle_sensor_range(int family,int variant){
    if(family==AW_VEHICLE_GROUND)return (float[]){20,16,26}[aw_clamp(variant,0,2)];
    if(family==AW_VEHICLE_BOAT)return 18+variant*8;
    if(family==AW_VEHICLE_QUAD)return 20;
    if(family==AW_VEHICLE_WING)return variant==1?40:30;
    return 20+variant*10;
}
static const char*aw_vehicle_role(int family,int variant){
    if(family==AW_VEHICLE_GROUND)return (const char*[]){"Scout: agile, narrow access","Rover: steady, broad tracks","Hauler: slow, wide clearance"}[aw_clamp(variant,0,2)];
    if(family==AW_VEHICLE_BOAT)return (const char*[]){"Skiff: fast, shallow draft","Patrol boat: balanced reach","Cutter: deep draft, long range"}[aw_clamp(variant,0,2)];
    if(family==AW_VEHICLE_QUAD)return "Quadcopter: hover, strafe, tight access";
    if(family==AW_VEHICLE_WING)return variant==1?"Recon wing: fast, wide sensor reach":"Transport: larger, wider turns";
    return (const char*[]){"Recon sub: compact, agile","Patrol sub: balanced reach","Heavy sub: slow, long-range sonar"}[aw_clamp(variant,0,2)];
}
static int aw_vehicle_family(int layer,int variant){return layer==0?AW_VEHICLE_GROUND:layer==1?AW_VEHICLE_BOAT:layer==3?AW_VEHICLE_SUB:variant==0?AW_VEHICLE_QUAD:AW_VEHICLE_WING;}
static AwBody aw_vehicle_body(const AwVehicle*v){AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);AwSVec p=v->position;if(v->family==AW_VEHICLE_GROUND)p.y+=s.height*.5f;if(v->family==AW_VEHICLE_BOAT)p.y+=.2f;return (AwBody){p,v->velocity,v->yaw,s.width,s.length,s.height*.5f,1};}
/* Yaw-oriented boxes, shared by collision and local sensing. Width/length are
 * half extents. Pitch is covered by conservative vertical margins in flight. */
static int aw_bodies_overlap(AwBody a,AwBody b,float margin){
    if(!a.active||!b.active||fabsf(a.position.y-b.position.y)>a.height+b.height+margin)return 0;
    float dx=b.position.x-a.position.x,dz=b.position.z-a.position.z;
    float ax[2]={cosf(a.yaw),sinf(a.yaw)},az[2]={-sinf(a.yaw),cosf(a.yaw)},bx[2]={cosf(b.yaw),sinf(b.yaw)},bz[2]={-sinf(b.yaw),cosf(b.yaw)};
    for(int i=0;i<4;i++){float x=i<2?ax[i]:bx[i-2],z=i<2?az[i]:bz[i-2];
        float ra=a.width*fabsf(x*ax[0]+z*az[0])+a.length*fabsf(x*ax[1]+z*az[1]);
        float rb=b.width*fabsf(x*bx[0]+z*bz[0])+b.length*fabsf(x*bx[1]+z*bz[1]);
        if(fabsf(dx*x+dz*z)>ra+rb+margin)return 0;}return 1;
}
static int aw_vehicle_clear(const AwMap*m,AwVehicle*v){
    AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);AwSVec p=v->position;
    if(p.x-s.length< -31||p.z-s.length< -31||p.x+s.length>159||p.z+s.length>159||p.y>60)return 0;
    if(v->family==AW_VEHICLE_GROUND){
        if(p.x<2||p.z<2||p.x>126||p.z>126)return 0;
        float q=(p.y+1.2f)/.75f,h=aw_support_q(m,p.x*.5f,p.z*.5f,q);
        if(h<q-1.05f||h>q+.46f)return 0;
        float bed=aw_ocean_bed_q(m,p.x*.5f,p.z*.5f);
        if(h<1.44f&&bed<1.44f&&fabsf(h-bed)<.5f)return 0;
        if(!aw_body_fits(m,p.x*.5f,h,p.z*.5f,v->variant!=0))return 0;
        v->position.y=h*.75f-1.2f;return 1;
    }
    if(v->family==AW_VEHICLE_BOAT){
        v->position.y=-.12f;
        int cell=((int)floorf(p.z*.5f)+16)*96+(int)floorf(p.x*.5f)+16;
        return cell>=0&&cell<AW_OCEAN_CELLS&&aw_patrol_water_cell(m,cell,v->variant);
    }
    AwBody box=aw_vehicle_body(v);float cy=cosf(v->yaw),sy=sinf(v->yaw);
    for(int z=-1;z<=1;z++)for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
        AwSVec corner={p.x+x*s.width*cy+z*s.length*sy,p.y+y*box.height,p.z-x*s.width*sy+z*s.length*cy};
        if(aw_ray_density(m,corner)>0)return 0;
        if(v->family==AW_VEHICLE_SUB){
            int ox=(int)floorf(corner.x*.5f)+16,oz=(int)floorf(corner.z*.5f)+16;
            if(corner.y>=-.25f||ox<0||oz<0||ox>=96||oz>=96||!m->ocean_connected[oz*96+ox])return 0;
        }else if(corner.y<.3f)return 0;
    }return 1;
}
/* Actions: forward speed, yaw, vertical speed, lateral speed; each 0/1/2.
 * Only the quad uses lateral thrust. Fixed-wing speed is always positive and
 * the physics clamps turn rate / climb, independently of policy behavior. */
static void aw_vehicle_step(const AwMap*m,AwVehicle*v,const int action[4],float dt,const AwBody*bodies,int count,int self){
    if(v->failed||!isfinite(dt)||dt<=0||dt>1.0f/30+.00001f)return;
    AwVehicle old=*v;AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);float cy=cosf(v->yaw),sy=sinf(v->yaw);
    float forward=v->velocity.x*sy+v->velocity.z*cy,lateral=v->velocity.x*cy-v->velocity.z*sy;
    float target=action[0]==0?-s.reverse:action[0]==1?0:s.speed;
    if(v->family==AW_VEHICLE_WING)target=s.reverse+(s.speed-s.reverse)*action[0]*.5f;
    forward+=aw_motion_clamp(target-forward,-s.accel*dt,s.accel*dt);
    if(v->family==AW_VEHICLE_WING)forward=fmaxf(s.reverse,forward);
    float side=v->family==AW_VEHICLE_QUAD?(action[3]-1)*s.speed:0;
    lateral+=aw_motion_clamp(side-lateral,-s.accel*dt,s.accel*dt);if(v->family!=AW_VEHICLE_QUAD)lateral=0;
    float turn=(action[1]-1)*s.turn;
    v->yaw_rate+=aw_motion_clamp(turn-v->yaw_rate,-s.turn_accel*dt,s.turn_accel*dt);
    v->yaw=aw_motion_angle(v->yaw+v->yaw_rate*dt);cy=cosf(v->yaw);sy=sinf(v->yaw);
    float up=(action[2]-1)*s.vertical;
    if(v->family==AW_VEHICLE_WING)up=aw_motion_clamp(up,-forward*.30f,forward*.30f);
    v->velocity.y+=aw_motion_clamp(up-v->velocity.y,-s.accel*dt,s.accel*dt);
    v->velocity.x=sy*forward+cy*lateral;v->velocity.z=cy*forward-sy*lateral;
    v->position=aw_sv_add(v->position,aw_sv_scale(v->velocity,dt));
    v->pitch=atan2f(v->velocity.y,fmaxf(.1f,fabsf(forward)));
    int valid=aw_vehicle_clear(m,v);AwBody body=aw_vehicle_body(v);
    for(int i=0;valid&&i<count;i++)if(i!=self&&aw_bodies_overlap(body,bodies[i],.04f))valid=0;
    if(!valid){v->position=old.position;v->contact=1;
        if(v->family==AW_VEHICLE_WING)v->failed=1;else v->velocity=(AwSVec){0};}
}
#endif
