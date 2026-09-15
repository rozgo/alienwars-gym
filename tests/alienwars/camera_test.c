#include <assert.h>
#include <stdio.h>
#include "ocean/alienwars/camera_zoom.h"

static float exercise(int hz,int direction){
    AwZoom z;aw_zoom_reset(&z,direction<0?16:280);
    float bound=direction<0?16:280,peak=bound;
    for(int i=0;i<hz*3;i++){
        if(i<hz&&i%(hz/30)==0)aw_zoom_push(&z,direction*.6f/30);
        float value=aw_zoom_step(&z,1.0f/hz);
        assert(isfinite(value)&&value>=16*expf(-.121f)&&value<=280*expf(.121f));
        if(i==hz-1){peak=value;assert(direction*(peak-bound)>bound*.01f);}
        if(i>hz+hz/2)assert(direction*(value-bound)<=direction*(peak-bound));
    }
    assert(fabsf(expf(z.value)-bound)<.005f);
    return peak;
}
int main(void){
    for(int d=-1;d<=1;d+=2){
        float a=exercise(30,d),b=exercise(60,d),c=exercise(120,d);
        assert(fabsf(a-c)<.001f&&fabsf(b-c)<.001f);
    }
    AwZoom a,b;aw_zoom_reset(&a,158);aw_zoom_reset(&b,158);
    for(int i=0;i<100;i++)aw_zoom_push(&a,-.0015f);
    aw_zoom_push(&b,-.15f);assert(fabsf(a.target-b.target)<.00005f);
    float previous=158;
    for(int i=0;i<120;i++){float value=aw_zoom_step(&a,1.0f/60);assert(value<=previous+.0001f);previous=value;}
    assert(fabsf(previous-158*expf(-.15f))<.01f);
    for(int d=-1;d<=1;d+=2){
        aw_zoom_reset(&a,d<0?16:280);
        for(int i=0;i<10000;i++)aw_zoom_push(&a,d*1000);
        float stretched=a.target;aw_zoom_push(&a,-d*.05f);
        assert(d*(a.target-stretched)<-.049f); /* Immediate reversal, no accumulated pressure debt. */
        aw_zoom_push(&a,NAN);aw_zoom_push(&a,INFINITY);
        assert(isfinite(aw_zoom_step(&a,NAN))&&isfinite(aw_zoom_step(&a,1000)));
    }
    aw_zoom_reset(&a,68);assert(fabsf(aw_zoom_step(&a,1)-68)<.0001f); /* Focus cancels the spring. */
    puts("CAMERA_TEST zoom_bounds=PASS rubber_band/settle=PASS reversal=PASS precise_deltas=PASS reset=PASS finite=PASS 30/60/120Hz=PASS");
}
