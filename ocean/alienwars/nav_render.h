#ifndef ALIENWARS_NAV_RENDER_H
#define ALIENWARS_NAV_RENDER_H
#include "render.h"
#include "sensor_render.h"
#include "camera_zoom.h"
#include "patrol_render.h"
#include "unit_visibility.h"
#include "unit_lighting.h"
typedef struct {
    AwScene scene;
    const AwNavWorld *world;
    AwZoom zoom;
    float yaw,pitch;
    Vector3 target;
    int paused,reset,heat,walk,sensors,reference,cut,follow;
} AwNavView;
static AwNavView nav_view;
#ifdef PLATFORM_WEB
#include <emscripten.h>
EM_JS(void,aw_nav_js_status,(const char *json),{window.awNavStatus?.(UTF8ToString(json));});
EM_JS(void,aw_nav_js_ready,(void),{window.awNavReady?.();});
EMSCRIPTEN_KEEPALIVE void aw_nav_orbit(float dx,float dy){
    nav_view.yaw-=dx*.005f;nav_view.pitch=Clamp(nav_view.pitch+dy*.004f,.25f,1.45f);
}
EMSCRIPTEN_KEEPALIVE void aw_nav_zoom(float delta){aw_zoom_push(&nav_view.zoom,delta);}
EMSCRIPTEN_KEEPALIVE void aw_nav_resize(int width,int height){
    if(IsWindowReady()&&width>=240&&height>=200&&width<=3840&&height<=2400)SetWindowSize(width,height);
}
EMSCRIPTEN_KEEPALIVE void aw_nav_option(int key,int value){
    if(key==0)nav_view.paused=!!value;
    if(key==1)nav_view.reset=1;
    if(key==2)nav_view.heat=!!value;
    if(key==3)nav_view.walk=!!value;
    if(key==4)nav_view.sensors=!!value;
    if(key==5)nav_view.reference=!!value;
    if(key==6)nav_view.cut=!!value;
    if(key==7)nav_view.follow=!!value;
}
static void aw_nav_web_publish(const AwNav *nav){
    const AwNavEpisode *e=&nav->episode;const AwNavView *v=&nav_view;
    char json[4096];int n=snprintf(json,sizeof(json),
        "{\"seed\":%u,\"hash\":\"%08x\",\"kind\":%d,\"ticks\":%d,\"limit\":%d,\"distance\":%.3f,\"reward\":%.5f,\"return\":%.4f,\"progress\":%.5f,\"event\":%.1f,\"contacts\":%d,\"invalid\":%d,\"episodes\":%u,\"successes\":%u,\"last\":%d,\"value\":%.4f,\"mode\":%d,\"throttle\":%d,\"steer\":%d,\"flags\":[%d,0,%d,%d,%d,%d,%d,%d],\"probabilities\":[",
        e->world->map.seed,e->world->map.hash,e->task->kind,e->ticks,e->limit,e->distance,e->reward,e->total_reward,e->progress_reward,e->event_reward,e->contacts,e->invalid_actions,nav->episodes,nav->successes,nav->last.success?1:nav->last.fall?2:3,nav->value,nav->controller,e->throttle,e->steer,v->paused,v->heat,v->walk,v->sensors,v->reference,v->cut,v->follow);
    for(int j=0;j<6;j++)n+=snprintf(json+n,sizeof(json)-n,"%s%.5f",j?",":"",nav->probabilities[j]);
    n+=snprintf(json+n,sizeof(json)-n,"],\"depth\":[");
    const AwSensorReading *depth=&e->sensors.units[0].reading[AW_SENSOR_CAMERA];
    for(int j=0;j<48;j++)n+=snprintf(json+n,sizeof(json)-n,"%s%.4f",j?",":"",depth->valid?depth->beams[j].hit.distance/36:1);
    snprintf(json+n,sizeof(json)-n,"]}");
    aw_nav_js_status(json);
}
#endif
static void aw_nav_manual(float actions[2]){
    actions[0]=1;actions[1]=1;if(!IsWindowReady())return;
    if(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))actions[0]=2;
    else if(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))actions[0]=0;
    if(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT))actions[1]=0;
    else if(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT))actions[1]=2;
}
int aw_nav_view_paused(void){return nav_view.paused;}
int aw_nav_view_reset(void){int reset=nav_view.reset;nav_view.reset=0;return reset;}
void aw_nav_render_close(void){
    if(IsWindowReady()){aw_destroy_scene(&nav_view.scene);aw_art_close();CloseWindow();}
    memset(&nav_view,0,sizeof(nav_view));
}
static void aw_nav_line(Vector3 a,Vector3 b,Color color){a.y+=.12f;b.y+=.12f;DrawLine3D(a,b,color);}
void aw_nav_render(AwNav *nav){
    AwNavView *v=&nav_view;AwNavEpisode *e=&nav->episode;
    if(!IsWindowReady()){
        SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
        InitWindow(1440,900,"AlienWars | Navigation Training");SetTargetFPS(60);
        v->yaw=.75f;v->pitch=.95f;v->cut=1;v->follow=1;v->sensors=1;v->reference=0;
        aw_zoom_reset(&v->zoom,48);
#ifdef PLATFORM_WEB
        int *default_locs=rlGetShaderLocsDefault();
        if(default_locs[SHADER_LOC_VERTEX_NORMAL]<0)default_locs[SHADER_LOC_VERTEX_NORMAL]=RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL;
        aw_nav_js_ready();
#endif
    }
    if(v->world!=e->world){aw_build_scene(&v->scene,&e->world->map);v->world=e->world;}
    if(IsKeyPressed(KEY_SPACE))v->paused=!v->paused;
    if(IsKeyPressed(KEY_N))v->reset=1;
    if(IsKeyPressed(KEY_H))v->heat=!v->heat;
    if(IsKeyPressed(KEY_G))v->walk=!v->walk;
    if(IsKeyPressed(KEY_L))v->sensors=!v->sensors;
    if(IsKeyPressed(KEY_R))v->reference=!v->reference;
    if(IsKeyPressed(KEY_C))v->cut=!v->cut;
    if(IsKeyPressed(KEY_F))v->follow=!v->follow;
    Vector3 p=aw_sensor_v3(aw_nav_position(&e->unit)),g=aw_sensor_v3(e->world->positions[e->task->goal]);
    if(v->follow)v->target=Vector3Lerp(p,g,.25f);
#ifndef PLATFORM_WEB
    Vector2 delta=GetMouseDelta();
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&GetMouseX()<GetScreenWidth()-330){
        v->yaw-=delta.x*.005f;v->pitch=Clamp(v->pitch+delta.y*.004f,.25f,1.45f);
    }
    float wheel=GetMouseWheelMove();if(wheel)aw_zoom_push(&v->zoom,-wheel*.12f);
#endif
    float zoom=aw_zoom_step(&v->zoom,fminf(GetFrameTime(),.05f));
    Camera3D camera={.position=Vector3Add(v->target,(Vector3){cosf(v->yaw)*cosf(v->pitch)*80,sinf(v->pitch)*80,sinf(v->yaw)*cosf(v->pitch)*80}),
        .target=v->target,.up={0,1,0},.fovy=zoom,.projection=CAMERA_ORTHOGRAPHIC};
    aw_prepare_reflection(&v->scene,camera,AW_CELLS);
    BeginDrawing();ClearBackground((Color){21,25,25,255});BeginMode3D(camera);
    aw_draw_scene(&v->scene,AW_CELLS,(float)GetTime(),v->walk,0,camera.position,p,v->cut,0);
    rlDrawRenderBatchActive();rlDisableDepthTest();
    if(v->heat)for(int n=0;n<AW_NODES;n++){
        float d=e->task->distance[n];if(!e->world->allowed[n]||d>48)continue;
        Vector3 a=aw_sensor_v3(e->world->positions[n]);a.y+=.08f;
        Color color=ColorFromHSV(120-fminf(1,d/48)*120,.7f,.85f);color.a=100;
        DrawCube(a,.32f,.03f,.32f,color);
    }
    if(v->reference){
        int n=e->task->start;
        for(int k=0;k<AW_NODES&&n!=e->task->goal;k++){
            int best=n;for(int d=0;d<AW_LINKS;d++){int j=e->world->links[n][d];if(j>=0&&e->task->distance[j]<e->task->distance[best])best=j;}
            if(best==n)break;aw_nav_line(aw_sensor_v3(e->world->positions[n]),aw_sensor_v3(e->world->positions[best]),(Color){180,155,236,180});n=best;
        }
    }
    for(int i=1;i<e->trace_count;i++)aw_nav_line(aw_sensor_v3(e->trace[i-1]),aw_sensor_v3(e->trace[i]),(Color){84,221,208,255});
    Vector3 attempted={e->unit.attempted_x,p.y,e->unit.attempted_z};
    if(e->unit.contact||e->unit.failed)DrawSphereEx(attempted,.22f,6,8,RED);
    DrawCylinder((Vector3){g.x,g.y+.7f,g.z},.05f,.05f,1.4f,8,GOLD);
    aw_sensor_ring(g,.9f,GOLD,0);DrawSphereEx((Vector3){g.x,g.y+1.5f,g.z},.18f,6,8,GOLD);
    rlDrawRenderBatchActive();rlEnableDepthTest();
    for(int pass=0;pass<2;pass++){
        if(pass==0)aw_units_through_begin();else aw_units_lit_begin();
        rlPushMatrix();rlTranslatef(p.x,p.y,p.z);rlRotatef(e->unit.motion.yaw*RAD2DEG,0,1,0);
        rlRotatef(-e->unit.motion.pitch*RAD2DEG,1,0,0);rlScalef(.85f,.85f,.85f);
        aw_patrol_model(AW_PATROL_GROUND,0,(float)GetTime());rlPopMatrix();
        if(pass==0)aw_units_through_end();else aw_units_lit_end();
    }
    if(v->sensors)aw_sensors_draw(&e->sensors,0,1|8,0,1,(float)GetTime(),0,0);
    EndMode3D();
#ifndef PLATFORM_WEB
    int x=GetScreenWidth()-330;DrawRectangle(x,0,330,GetScreenHeight(),(Color){26,29,31,248});x+=18;
    DrawText("ALIENWARS / NAVIGATION",x,20,18,RAYWHITE);
    const char *mode=nav->controller==0?"POLICY":nav->controller==1?"RANDOM BASELINE":nav->controller==2?"GREEDY BASELINE":nav->controller==3?"GRAPH REFERENCE":"MANUAL";
    DrawText(mode,x,49,18,nav->controller==0?GREEN:ORANGE);
    const char *identity=strrchr(nav->label,'/');identity=identity?identity+1:nav->label;
    DrawText(TextFormat("%.35s",identity),18,48,16,LIGHTGRAY);
    DrawText(TextFormat("Map %u / %08x",e->world->map.seed,e->world->map.hash),x,80,16,LIGHTGRAY);
    DrawText(TextFormat("Task: %s",(const char*[]){"surface","bridge","tunnel"}[e->task->kind]),x,105,18,RAYWHITE);
    DrawText(TextFormat("Time %.1f / %.1f s",e->ticks*.1f,e->limit*.1f),x,135,18,LIGHTGRAY);
    DrawText(TextFormat("Distance %.1f u",e->distance),x,160,18,LIGHTGRAY);
    DrawText(TextFormat("Return %+.3f / step %+.3f",e->total_reward,e->reward),x,190,17,RAYWHITE);
    DrawText(TextFormat("Progress %+.4f  event %+.1f",e->progress_reward,e->event_reward),x,216,16,LIGHTGRAY);
    DrawText(TextFormat("Contacts %d / invalid %d",e->contacts,e->invalid_actions),x,240,16,LIGHTGRAY);
    DrawText(TextFormat("Completed %u / success %.1f%%",nav->episodes,nav->episodes?100.0f*nav->successes/nav->episodes:0),x,270,16,GOLD);
    if(nav->episodes)DrawText(TextFormat("Last: %s / %.1f s",nav->last.success?"arrived":nav->last.fall?"support failure":"timeout",nav->last.steps*.1f),x,294,16,LIGHTGRAY);
    DrawText(TextFormat("Value estimate %+.3f",nav->value),x,325,18,RAYWHITE);
    for(int h=0;h<2;h++){
        int y=354+h*48;DrawText(h?"Steer: left / straight / right":"Drive: reverse / stop / forward",x,y,14,LIGHTGRAY);
        for(int j=0;j<3;j++){
            DrawRectangle(x+j*94,y+21,86,10,(Color){54,59,63,255});
            DrawRectangle(x+j*94,y+21,(int)(86*nav->probabilities[h*3+j]),10,(h?e->steer:e->throttle)==j?GOLD:SKYBLUE);
        }
    }
    DrawText("Actor depth / radial range",x,460,16,LIGHTGRAY);
    const AwSensorReading *depth=&e->sensors.units[0].reading[AW_SENSOR_CAMERA];
    for(int i=0;i<48;i++){
        float d=depth->valid?depth->beams[i].hit.distance/36:1;
        unsigned char shade=(unsigned char)(24+210*(1-d));
        DrawRectangle(x+(i%8)*22,485+(i/8)*18,21,17,(Color){shade,shade,shade,255});
    }
    DrawText("SPACE pause   N next episode",x,613,15,LIGHTGRAY);
    DrawText("H reward distance   G walkable",x,637,15,LIGHTGRAY);
    DrawText("L sensors   R reference path",x,661,15,LIGHTGRAY);
    DrawText("C cutaway   F follow   Drag orbit",x,685,15,LIGHTGRAY);
    DrawText("WASD / arrows in manual mode",x,709,15,LIGHTGRAY);
    DrawText("Goal uses ideal localization.",x,750,14,GRAY);
    DrawText("Heatmap / reference are diagnostics.",x,772,14,GRAY);
    DrawText(v->paused?"PAUSED":"10 Hz policy / 30 Hz simulation",18,20,18,v->paused?ORANGE:RAYWHITE);
#endif
    EndDrawing();
#ifdef PLATFORM_WEB
    static double last_report;
    if(GetTime()-last_report>.1){aw_nav_web_publish(nav);last_report=GetTime();}
#endif
}
#endif
