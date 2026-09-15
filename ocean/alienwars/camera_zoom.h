#ifndef ALIENWARS_CAMERA_ZOOM_H
#define ALIENWARS_CAMERA_ZOOM_H
#include <math.h>

/* Logarithmic orthographic scale: wheel magnitudes compose consistently,
 * even when a trackpad sends many small events between rendered frames. */
typedef struct {float target,value,idle;int initialized;} AwZoom;
static float aw_zoom_clamp(float x,float lo,float hi){return fminf(hi,fmaxf(lo,x));}
static void aw_zoom_reset(AwZoom*z,float scale){
    float value=logf(aw_zoom_clamp(scale,16,280));
    *z=(AwZoom){.target=value,.value=value,.initialized=1};
}
static void aw_zoom_push(AwZoom*z,float delta){
    if(!isfinite(delta)||delta==0)return;
    const float lo=logf(16),hi=logf(280),stretch=.12f;
    delta=aw_zoom_clamp(delta,-.7f,.7f);
    /* Apply inward motion immediately; only outward motion meets resistance.
     * Limit stored pressure so reversing never has a long dead zone. */
    float boundary=delta<0?lo:hi;
    float room=delta<0?fminf(0,boundary-z->target):fmaxf(0,boundary-z->target);
    float inside=copysignf(fminf(fabsf(delta),fabsf(room)),delta);
    z->target+=inside;delta-=inside;
    float pressure=stretch+fabsf(z->target-boundary);
    z->target+=copysignf(sqrtf(pressure*pressure+.5f*stretch*fabsf(delta))-pressure,delta);
    z->target=aw_zoom_clamp(z->target,lo-stretch,hi+stretch);z->idle=0;
}
static float aw_zoom_step(AwZoom*z,float dt){
    if(!isfinite(dt)||dt<=0)return expf(z->value);
    dt=fminf(dt,.1f);int steps=(int)ceilf(dt*120-.00001f);if(steps<1)steps=1;
    float h=dt/steps,lo=logf(16),hi=logf(280);
    for(int i=0;i<steps;i++){
        z->idle=fminf(z->idle+h,1);
        if(z->idle>.075f){
            float limit=aw_zoom_clamp(z->target,lo,hi);
            z->target=limit+(z->target-limit)*expf(-18*h);
            if(fabsf(z->target-limit)<.00001f)z->target=limit;
        }
        z->value+=(z->target-z->value)*(1-expf(-24*h));
        if(fabsf(z->target-z->value)<.00001f)z->value=z->target;
    }
    return expf(z->value);
}
#endif
