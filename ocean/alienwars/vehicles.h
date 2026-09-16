#ifndef ALIENWARS_VEHICLES_H
#define ALIENWARS_VEHICLES_H
#include "patrols.h"
#include "sensor_rays.h"
#include "motion.h"
typedef struct {int family,variant,contact,failed;AwSVec position,velocity;float yaw,yaw_rate,pitch; } AwVehicle;
typedef struct {AwSVec position,velocity;float yaw,width,length,height;int active;} AwBody;
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
typedef struct {float speed,turn,climb,strafe;} AwDrive;
/* Continuous actuator setpoints. Both route primitives and the learned local
 * controller go through these exact acceleration / turn limits. */
static void aw_vehicle_drive(const AwMap*m,AwVehicle*v,AwDrive drive,float dt,const AwBody*bodies,int count,int self){
    if(v->failed||!isfinite(dt)||dt<=0||dt>1.0f/30+.00001f)return;
    AwVehicle old=*v;AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);float cy=cosf(v->yaw),sy=sinf(v->yaw);
    float forward=v->velocity.x*sy+v->velocity.z*cy,lateral=v->velocity.x*cy-v->velocity.z*sy;
    float target=aw_motion_clamp(drive.speed,-s.reverse,s.speed);
    if(v->family==AW_VEHICLE_GROUND){int c=aw_clamp((int)(v->position.z*.5f),0,63)*64+aw_clamp((int)(v->position.x*.5f),0,63);target*=aw_ground_traction(v->variant,m->cells[c].material);}
    if(v->family==AW_VEHICLE_WING)target=fmaxf(s.reverse,target);
    forward+=aw_motion_clamp(target-forward,-s.accel*dt,s.accel*dt);
    if(v->family==AW_VEHICLE_WING)forward=fmaxf(s.reverse,forward);
    float side=v->family==AW_VEHICLE_QUAD?aw_motion_clamp(drive.strafe,-s.speed,s.speed):0;
    lateral+=aw_motion_clamp(side-lateral,-s.accel*dt,s.accel*dt);if(v->family!=AW_VEHICLE_QUAD)lateral=0;
    float turn=aw_motion_clamp(drive.turn,-s.turn,s.turn);
    v->yaw_rate+=aw_motion_clamp(turn-v->yaw_rate,-s.turn_accel*dt,s.turn_accel*dt);
    v->yaw=aw_motion_angle(v->yaw+v->yaw_rate*dt);cy=cosf(v->yaw);sy=sinf(v->yaw);
    float up=aw_motion_clamp(drive.climb,-s.vertical,s.vertical);
    if(v->family==AW_VEHICLE_WING)up=aw_motion_clamp(up,-forward*.30f,forward*.30f);
    v->velocity.y+=aw_motion_clamp(up-v->velocity.y,-s.accel*dt,s.accel*dt);
    v->velocity.x=sy*forward+cy*lateral;v->velocity.z=cy*forward-sy*lateral;
    v->position=aw_sv_add(v->position,aw_sv_scale(v->velocity,dt));
    v->pitch=atan2f(v->velocity.y,fmaxf(.1f,fabsf(forward)));
    int valid=aw_vehicle_clear(m,v);AwBody body=aw_vehicle_body(v);
    for(int i=0;valid&&i<count;i++)if(i!=self&&aw_bodies_overlap(body,bodies[i],.04f))valid=0;
    if(!valid){v->position=old.position;v->yaw=old.yaw;v->pitch=old.pitch;v->yaw_rate=0;v->contact=1;
        if(v->family==AW_VEHICLE_WING)v->failed=1;else v->velocity=(AwSVec){0};}
}
static void aw_vehicle_step(const AwMap*m,AwVehicle*v,const int action[4],float dt,const AwBody*bodies,int count,int self){
    AwVehicleSpec s=aw_vehicle_spec(v->family,v->variant);
    float speed=action[0]==0?-s.reverse:action[0]==1?0:s.speed;
    if(v->family==AW_VEHICLE_WING)speed=s.reverse+(s.speed-s.reverse)*action[0]*.5f;
    aw_vehicle_drive(m,v,(AwDrive){speed,(action[1]-1)*s.turn,(action[2]-1)*s.vertical,(action[3]-1)*s.speed},dt,bodies,count,self);
}
/* Synchronous body resolution: every proposal sees the same old world. A
 * rejected proposal can block another vehicle's proposal, so propagate until
 * stable. No vehicle gets priority from its index in the array. */
static void aw_vehicles_step(const AwMap*m,AwVehicle*v,const AwDrive*drive,const unsigned char*active,int count,float dt){
    AwVehicle before[16];unsigned char blocked[16]={0};
    if(count<0||count>16)return;
    for(int i=0;i<count;i++){before[i]=v[i];if(active[i])aw_vehicle_drive(m,&v[i],drive[i],dt,NULL,0,-1);}
    for(int pass=0;pass<count;pass++){
        int changed=0;
        for(int i=0;i<count;i++)if(active[i])for(int j=i+1;j<count;j++)if(active[j]){
            AwBody a=aw_vehicle_body(&v[i]),b=aw_vehicle_body(&v[j]);
            if(!aw_bodies_overlap(a,b,.04f))continue;
            if(!blocked[i]){blocked[i]=1;changed=1;}if(!blocked[j]){blocked[j]=1;changed=1;}
        }
        for(int i=0;i<count;i++)if(blocked[i]){v[i]=before[i];v[i].contact=1;v[i].velocity=(AwSVec){0};v[i].yaw_rate=0;if(v[i].family==AW_VEHICLE_WING)v[i].failed=1;}
        if(!changed)break;
    }
}
#endif
