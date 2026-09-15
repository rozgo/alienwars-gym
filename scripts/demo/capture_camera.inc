/* Capture-only camera controls. Included after the actual viewer source.
 * Quintic easing is evaluated every rendered frame, avoiding integer mouse
 * deltas. These controls never modify simulation or sensor measurements. */
static double demo_start;
static float demo_duration, demo_yaw, demo_pitch, demo_dx, demo_dy;
static Vector3 demo_focus;
static int demo_follow;

AW_EXPORT void aw_demo_orbit(float seconds,float dx,float dy){
    demo_start=GetTime();demo_duration=seconds;
    demo_yaw=yaw;demo_pitch=pitch;demo_dx=dx;demo_dy=dy;
}
AW_EXPORT void aw_demo_frame(float angle,float elevation,float scale,float x,float y,float z){
    demo_duration=0;follow_scout=0;demo_follow=0;
    yaw=angle;pitch=elevation;aw_set_zoom(scale);focus=(Vector3){x,y,z};
}
static void aw_demo_camera(float dt){
    if(demo_duration>0){
        float t=Clamp((GetTime()-demo_start)/demo_duration,0,1);
        float s=t*t*t*(t*(t*6-15)+10);
        yaw=demo_yaw-demo_dx*.005f*s;
        pitch=Clamp(demo_pitch+demo_dy*.004f*s,.42f,1.35f);
        if(t>=1)demo_duration=0;
    }
    if(follow_scout&&demo_follow&&Vector3Distance(demo_focus,focus)<30)
        focus=Vector3Lerp(demo_focus,focus,1-expf(-dt*7));
    demo_focus=focus;demo_follow=follow_scout;
}
