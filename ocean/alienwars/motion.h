#ifndef ALIENWARS_MOTION_H
#define ALIENWARS_MOTION_H
#include <math.h>
/* Scripted route steering. Position stays on the validated support path;
 * body orientation eases toward its tangent with bounded rate/acceleration.
 * The renderer and sensors consume this same state. No rendering dependency. */
#define AW_MOTION_PI 3.14159265358979323846f
typedef struct {float yaw,pitch,yaw_rate,pitch_rate;int initialized;} AwMotion;
typedef struct {float yaw_speed,yaw_accel,pitch_speed,pitch_accel,response;} AwMotionTuning;
static float aw_motion_clamp(float x,float lo,float hi){return fminf(hi,fmaxf(lo,x));}
static float aw_motion_angle(float x){
    float a=fmodf(x+AW_MOTION_PI,2*AW_MOTION_PI);if(a<0)a+=2*AW_MOTION_PI;return a-AW_MOTION_PI;
}
/* Layer IDs match patrols.h: ground 0, naval 1, air 2. */
static AwMotionTuning aw_motion_tuning(int layer,int variant){
    float speed=layer==0?(variant==0?1.8f:variant==1?1.4f:1.05f):layer==1?(1.0f-.18f*variant):(variant==0?1.3f:variant==1?.95f:.75f);
    /* Compress the easing to one third of its original duration. Rates and
     * response scale with time; acceleration scales with time squared. */
    const float tempo=3.0f;
    return (AwMotionTuning){speed*tempo,speed*3*tempo*tempo,.65f*tempo,1.8f*tempo*tempo,4.5f*tempo};
}
static void aw_motion_axis(float*angle,float*rate,float target,float max_rate,float acceleration,float response,float dt){
    float error=aw_motion_angle(target-*angle);
    /* A deterministic side at 180 degrees avoids flickering between left and
     * right when a ping-pong patrol reaches an endpoint. Preserve a turn that
     * is already underway if roundoff straddles the branch cut. */
    if(fabsf(error)>AW_MOTION_PI-.0001f)error=*rate<0?-fabsf(error):fabsf(error);
    float a=aw_motion_clamp(response*response*error-2*response*(*rate),-acceleration,acceleration);
    *rate=aw_motion_clamp(*rate+a*dt,-max_rate,max_rate);
    *angle=aw_motion_angle(*angle+*rate*dt);
}
static void aw_motion_follow(AwMotion*m,float dx,float dy,float dz,AwMotionTuning tuning,float dt){
    float horizontal=hypotf(dx,dz);if(horizontal<.00001f&&fabsf(dy)<.00001f)return;
    float yaw=horizontal>.00001f?atan2f(dx,dz):m->yaw,pitch=atan2f(dy,fmaxf(.00001f,horizontal));
    if(!m->initialized){*m=(AwMotion){.yaw=yaw,.pitch=pitch,.initialized=1};return;}
    if(!isfinite(dt)||dt<=0||dt>1)return;
    /* Bound integration error at slow frame rates; 30/60/120 Hz use the same
     * 120 Hz substeps, with no allocations or wall-clock dependency. */
    int steps=(int)ceilf(dt*120-.00001f);if(steps<1)steps=1;float h=dt/steps;
    for(int i=0;i<steps;i++){
        aw_motion_axis(&m->yaw,&m->yaw_rate,yaw,tuning.yaw_speed,tuning.yaw_accel,tuning.response,h);
        aw_motion_axis(&m->pitch,&m->pitch_rate,pitch,tuning.pitch_speed,tuning.pitch_accel,tuning.response,h);
    }
}
static float aw_motion_pace(const AwMotion*m,float dx,float dz,int layer){
    if(!m->initialized||hypotf(dx,dz)<.00001f)return 1;
    float alignment=fmaxf(0,cosf(aw_motion_angle(atan2f(dx,dz)-m->yaw)));
    /* Ground/boats ease through sharp turns; aircraft keep forward progress. */
    float minimum=layer==2?.8f:.12f;return minimum+(1-minimum)*alignment*alignment;
}
#endif
