#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ocean/alienwars/motion.h"

static AwMotion simulate(int layer,int variant,int hz){
    AwMotion m={0};AwMotionTuning tuning=aw_motion_tuning(layer,variant);
    aw_motion_follow(&m,0,0,1,tuning,0);
    for(int step=0;step<24*hz;step++){
        float heading=step<8*hz?AW_MOTION_PI*.5f:step<16*hz?-AW_MOTION_PI*.5f:AW_MOTION_PI-.02f;
        float elevation=step<12*hz?.3f:-.2f,dt=1.0f/hz;AwMotion before=m;
        aw_motion_follow(&m,sinf(heading),tanf(elevation),cosf(heading),tuning,dt);
        assert(isfinite(m.yaw)&&isfinite(m.pitch)&&isfinite(m.yaw_rate)&&isfinite(m.pitch_rate));
        assert(fabsf(aw_motion_angle(m.yaw-before.yaw))<=tuning.yaw_speed*dt+.00001f);
        assert(fabsf(m.pitch-before.pitch)<=tuning.pitch_speed*dt+.00001f);
        assert(fabsf(m.yaw_rate-before.yaw_rate)<=tuning.yaw_accel*dt+.00001f);
        assert(fabsf(m.pitch_rate-before.pitch_rate)<=tuning.pitch_accel*dt+.00001f);
        if(step==hz/4)assert(m.yaw>.01f&&m.yaw<AW_MOTION_PI*.45f); /* Not a snapped 90-degree turn. */
        if(step==8*hz-1)assert(fabsf(aw_motion_angle(m.yaw-heading))<.005f);
    }
    assert(fabsf(aw_motion_angle(m.yaw-(AW_MOTION_PI-.02f)))<.015f&&fabsf(m.pitch+.2f)<.001f);
    return m;
}
int main(void){
    for(int layer=0;layer<3;layer++)for(int variant=0;variant<3;variant++){
        AwMotion a=simulate(layer,variant,30),b=simulate(layer,variant,60),c=simulate(layer,variant,120);
        assert(fabsf(aw_motion_angle(a.yaw-b.yaw))<.0002f&&fabsf(aw_motion_angle(a.yaw-c.yaw))<.0002f);
        assert(fabsf(a.pitch-c.pitch)<.0002f&&fabsf(a.yaw_rate-c.yaw_rate)<.0002f);
    }
    AwMotionTuning t=aw_motion_tuning(0,0);AwMotion m={0};float before=AW_MOTION_PI-.025f,after=-AW_MOTION_PI+.025f;
    aw_motion_follow(&m,sinf(before),0,cosf(before),t,0);
    float travelled=0;
    for(int i=0;i<180;i++){float previous=m.yaw;aw_motion_follow(&m,sinf(after),0,cosf(after),t,1.0f/60);travelled+=fabsf(aw_motion_angle(m.yaw-previous));}
    assert(travelled<.051f&&fabsf(aw_motion_angle(m.yaw-after))<.0001f); /* Cross +/- pi by the short arc. */
    AwMotion paused=m;aw_motion_follow(&m,0,1,1,t,0);assert(!memcmp(&m,&paused,sizeof(m)));
    aw_motion_follow(&m,0,1,1,t,NAN);assert(!memcmp(&m,&paused,sizeof(m)));
    m=(AwMotion){0};aw_motion_follow(&m,1,0,0,t,0);assert(fabsf(m.yaw-AW_MOTION_PI*.5f)<.00001f&&m.yaw_rate==0);
    assert(aw_motion_pace(&m,1,0,0)>.999f&&aw_motion_pace(&m,-1,0,0)<.13f&&aw_motion_pace(&m,-1,0,2)>=.8f);
    puts("MOTION_TEST profiles=9 rate/acceleration_bounds=PASS pitch=PASS reversals=PASS angle_wrap=PASS pause/reset=PASS 30/60/120Hz=PASS turn_pace=PASS");
}
