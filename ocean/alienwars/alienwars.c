/* Map Lab viewer. The shared map.h also serves the PufferLib environment. */
#include "render.h"
#include <inttypes.h>
#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#define AW_EXPORT EMSCRIPTEN_KEEPALIVE
EM_JS(void,aw_report,(uint32_t seed,uint32_t hash,int valid,int walk,int reached,int length,int decisions,int reductions,int attempts,int resolved,int paused,double milliseconds,int tunnels,int structural,int cut,int layer,float floor,int cost,int shapes,int tour),{
    if(typeof window !== 'undefined' && window.maplabReport) window.maplabReport({
        seed:seed>>>0,hash:(hash>>>0).toString(16).padStart(8,'0'),valid:!!valid,
        walk,reached,length,decisions,reductions,attempts,resolved,paused:!!paused,milliseconds,
        tunnels,structural,cut:!!cut,layer,floor,cost,shapes,tour
    });
});
#else
#define AW_EXPORT
#define aw_report(...) ((void)0)
#endif

static AwMap world;
static AwScene scene;
static AwOptions settings={1,6,6,0,1};
static int cut_mode=1,cut_active=0,follow_scout=0,scout_layer=0,show_tiles=0,scout_tour=0;
static float scout_floor=1;
static Camera3D camera;
static float yaw=0.75f,pitch=0.9f,zoom=158.0f;
static Vector3 focus={64,10,64};
static float revealed=AW_CELLS,animation_time=0,unit_progress=0;
static int show_overlay=0,show_path=1,paused=0,unit_paused=0;
static double generation_ms=0;

static void aw_publish(void) {
    aw_report(world.seed,world.hash,world.valid,world.walk_count,world.reached_count,world.path_length,
        world.decisions,world.reductions,world.attempts,(int)revealed,paused,generation_ms,world.tunnel_count,world.structure_decisions+world.cave_decisions,cut_active,scout_layer,scout_floor,world.path_cost,world.shape_decisions,scout_tour);
}

AW_EXPORT void aw_new(uint32_t seed,int watch) {
    double start=GetTime();
    if(!aw_generate_options(&world,seed,settings)){
        generation_ms=(GetTime()-start)*1000;
        aw_publish();
        return;
    }
    generation_ms=(GetTime()-start)*1000;
    aw_build_scene(&scene,&world);
    revealed=watch?0:AW_CELLS;
    unit_progress=0;paused=0;scout_tour=0;
    aw_publish();
}

AW_EXPORT void aw_config(uint32_t seed,int watch,int symmetry,int a,int b,int biome,int tunnels){
    settings=(AwOptions){symmetry,a,b,biome,tunnels};aw_new(seed,watch);
}

AW_EXPORT void aw_option(int option,int value) {
    if(option==0)show_overlay=!!value;
    if(option==1)show_path=!!value;
    if(option==2)unit_paused=!!value;
    if(option==3)paused=!!value;
    if(option==4)cut_mode=aw_clamp(value,0,2);
    if(option==5)follow_scout=!!value;
    if(option==6)show_tiles=!!value;
    aw_publish();
}

AW_EXPORT void aw_watch(void) {
    revealed=0;unit_progress=0;paused=0;aw_publish();
}

AW_EXPORT void aw_step(void) {
    paused=1;
    revealed=fminf((float)AW_CELLS,revealed+1);
    aw_publish();
}

AW_EXPORT void aw_camera_control(int action) {
    if(action==0){yaw=0.75f;pitch=0.9f;zoom=158;focus=(Vector3){64,10,64};follow_scout=0;}
    if(action==1)zoom=fmaxf(16,zoom*0.84f);
    if(action==2)zoom=fminf(210,zoom/0.84f);
    if(action==3)yaw-=0.22f;
    if(action==4)yaw+=0.22f;
    if(action>=5&&action<=8){
        Vector3 right={cosf(yaw),0,-sinf(yaw)},forward={sinf(yaw),0,cosf(yaw)};
        Vector3 shift=action<7?right:forward;
        float amount=(action==5||action==7)?-3.0f:3.0f;
        focus=Vector3Add(focus,Vector3Scale(shift,amount));
        focus.x=Clamp(focus.x,0,128);focus.z=Clamp(focus.z,0,128);
    }
}

AW_EXPORT void aw_resize(int width,int height) {
    if(width<240||height<200||width>3840||height>2400)return;
    if(IsWindowReady())SetWindowSize(width,height);
}

static void aw_draw_markers(void) {
    for(int s=0;s<2;s++){
        Vector3 p=aw_center(&world,world.spawns[s]);
        Color accent=s?(Color){249,161,88,255}:(Color){101,225,222,255};
        DrawCylinder((Vector3){p.x,p.y+0.04f,p.z},2.0f,2.0f,0.14f,8,(Color){43,55,57,255});
        DrawCylinderWires((Vector3){p.x,p.y+0.2f,p.z},1.72f,1.72f,0.02f,8,accent);
        for(int i=0;i<4;i++){
            float a=PI*0.25f+i*PI*0.5f;
            Vector3 column={p.x+cosf(a)*1.8f,p.y+0.5f,p.z+sinf(a)*1.8f};
            DrawCube(column,0.28f,1.0f,0.28f,(Color){62,74,72,255});
            column.y+=0.56f;DrawCube(column,0.3f,0.13f,0.3f,accent);
        }
    }

}

static Vector3 aw_unit_position(void) {
    int length=world.path_length;
    if(length<2)return aw_center(&world,world.spawns[0]);
    float cycle=fmodf(unit_progress,(float)(length-1)*2);
    float p=cycle>length-1?(length-1)*2-cycle:cycle;
    int segment=(int)p;
    if(segment>=length-1)segment=length-2;
    scout_layer=world.path[segment]>=AW_CELLS;
    scout_floor=aw_corner_q(&world,world.path[segment],0)/4.0f;
    Vector3 position=Vector3Lerp(aw_center(&world,world.path[segment]),aw_center(&world,world.path[segment+1]),p-segment);
    float gx=position.x/AW_UNIT,gz=position.z/AW_UNIT;
    if(world.path[segment]>=AW_CELLS||world.path[segment+1]>=AW_CELLS){
        float q=aw_lerp(aw_corner_q(&world,world.path[segment],0),aw_corner_q(&world,world.path[segment+1],0),p-segment);
        position.y=aw_y(aw_support_q(&world,gx,gz,q)/4);
    }else{
        int node=(int)gz*AW_SIZE+(int)gx;position.y=aw_ground_y(&world,node,gx-(int)gx,gz-(int)gz);
    }
    return position;
}

static void aw_tour(int entrance){
    if(!world.options.tunnels)return;
    int a=-1,b=-1;for(int i=0;i<world.cave_count;i++)if(world.cave[i].portal>=0){if(a<0)a=i;else b=i;}
    if(a<0||b<0)return;
    uint8_t surface[AW_CELLS];memcpy(surface,world.walkable,AW_CELLS);memset(world.walkable,0,AW_CELLS);
    int valid=aw_find_path(&world,AW_CELLS+a,AW_CELLS+b);memcpy(world.walkable,surface,AW_CELLS);if(!valid)return;
    unit_progress=0;scout_tour=1;
    if(!entrance){int deepest=0;for(int i=0;i<world.path_length;i++){
        int a=world.path[i],b=world.path[deepest],ca=aw_node_cell(&world,a),cb=aw_node_cell(&world,b);
        int qa=aw_corner_q(&world,a,0),qb=aw_corner_q(&world,b,0);
        int da=aw_abs(ca%64-32)+aw_abs(ca/64-32),db=aw_abs(cb%64-32)+aw_abs(cb/64-32);
        if(qa<qb||(qa==qb&&da<db))deepest=i;
    }unit_progress=(float)deepest;}
    unit_paused=1;revealed=AW_CELLS;follow_scout=0;
    focus=aw_unit_position();zoom=entrance?30:38;yaw=entrance?-1.4f:0.9f;pitch=0.85f;aw_publish();
}
AW_EXPORT void aw_inspect_tunnel(void){aw_tour(0);}
AW_EXPORT void aw_inspect_entrance(void){aw_tour(1);}

static void aw_draw_unit(void) {
    Vector3 p=aw_unit_position();
    DrawCylinder((Vector3){p.x,p.y+0.025f,p.z},0.6f,0.6f,0.01f,12,(Color){26,39,38,170});
    p.y+=0.55f+sinf(animation_time*4)*0.05f;
    DrawCube(p,0.9f,0.28f,0.7f,(Color){214,175,82,255});
    DrawCube((Vector3){p.x,p.y+0.2f,p.z},0.36f,0.17f,0.34f,(Color){83,220,230,255});
    for(int i=0;i<4;i++){
        Vector3 leg={p.x+(i&1?0.58f:-0.58f),p.y-0.12f,p.z+(i&2?0.4f:-0.4f)};
        DrawSphereEx(leg,0.17f,4,6,(Color){46,64,69,255});
    }
}

static void aw_update(void) {
    float dt=fminf(GetFrameTime(),0.05f);
    animation_time+=dt;
    if(!paused&&revealed<AW_CELLS)revealed=fminf(AW_CELLS,revealed+dt*320);
    if(revealed>=AW_CELLS&&!unit_paused&&world.path_length>1){
        float cycle=fmodf(unit_progress,(float)(world.path_length-1)*2);
        float p=cycle>world.path_length-1?(world.path_length-1)*2-cycle:cycle;
        int i=aw_clamp((int)p,0,world.path_length-2);
        int cost=aw_move_cost(&world,world.path[i],world.path[i+1]);
        unit_progress+=dt*2.3f*10.0f/fmaxf(10,(float)cost);
    }
    Vector2 mouse=GetMouseDelta();
    if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||(IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&IsKeyDown(KEY_LEFT_SHIFT))){
        Vector3 right={cosf(yaw),0,-sinf(yaw)},forward={sinf(yaw),0,cosf(yaw)};
        focus=Vector3Add(focus,Vector3Scale(right,-mouse.x*zoom/GetScreenHeight()));
        focus=Vector3Add(focus,Vector3Scale(forward,-mouse.y*zoom/GetScreenHeight()));
        focus.x=Clamp(focus.x,0,128);focus.z=Clamp(focus.z,0,128);
    }else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
        yaw-=mouse.x*0.005f;pitch=Clamp(pitch+mouse.y*0.004f,0.42f,1.35f);
    }
    zoom=Clamp(zoom*(1-GetMouseWheelMove()*0.09f),16,210);
    if(IsKeyDown(KEY_Q))yaw-=dt;
    if(IsKeyDown(KEY_E))yaw+=dt;
    if(IsKeyPressed(KEY_HOME))aw_camera_control(0);
#ifndef PLATFORM_WEB
    if(IsKeyPressed(KEY_R))aw_new(aw_hash(world.seed+1),0);
    if(IsKeyPressed(KEY_G))aw_option(0,!show_overlay);
    if(IsKeyPressed(KEY_P))aw_option(1,!show_path);
    if(IsKeyPressed(KEY_SPACE))aw_watch();
#endif
    Vector3 scout=aw_unit_position();
    if(follow_scout)focus=scout;
    camera.target=focus;
    camera.position=(Vector3){focus.x+sinf(yaw)*cosf(pitch)*100,focus.y+sinf(pitch)*100,focus.z+cosf(yaw)*cosf(pitch)*100};
    float aspect=(float)GetScreenWidth()/(float)GetScreenHeight();
    camera.up=(Vector3){0,1,0};camera.fovy=zoom*fmaxf(1.0f,1.35f/aspect);camera.projection=CAMERA_ORTHOGRAPHIC;
    BeginDrawing();
    ClearBackground((Color){7,13,18,255});
    BeginMode3D(camera);
    if(world.valid){
        Vector3 target=scout;target.y+=0.55f;
        Vector3 eye=Vector3Add(target,Vector3Scale(Vector3Normalize(Vector3Subtract(camera.position,camera.target)),190));
        int blocked=aw_occluded(&world,eye.x/AW_UNIT,(eye.y+1.2f)/0.75f,eye.z/AW_UNIT,target.x/AW_UNIT,(target.y+1.2f)/0.75f,target.z/AW_UNIT);
        cut_active=cut_mode==2||(cut_mode==1&&blocked);
        aw_draw_scene(&scene,revealed-1,animation_time,show_overlay,eye,target,cut_active?cut_mode:0);
        if(revealed>=AW_CELLS){
            if(show_tiles){
                for(int c=0;c<AW_CELLS;c++)for(int d=0;d<2;d++)for(int i=0;i<AW_SUBDIV;i++){
                    float a=(float)i/AW_SUBDIV,b=(float)(i+1)/AW_SUBDIV;
                    float ax=d?1:a,az=d?a:1,bx=d?1:b,bz=d?b:1;
                    Vector3 p={(c%AW_SIZE+ax)*AW_UNIT,aw_ground_y(&world,c,ax,az)+0.045f,(c/AW_SIZE+az)*AW_UNIT};
                    Vector3 q={(c%AW_SIZE+bx)*AW_UNIT,aw_ground_y(&world,c,bx,bz)+0.045f,(c/AW_SIZE+bz)*AW_UNIT};
                    Vector3 mid=Vector3Lerp(p,q,0.5f);
                    if(aw_solid(&world,mid.x/AW_UNIT,(mid.y+1.2f)/0.75f-0.12f,mid.z/AW_UNIT))DrawLine3D(p,q,(Color){123,209,196,140});
                }
            }
            aw_draw_markers();
            if(show_path){
                for(int i=1;i<world.path_length;i++){
                    Vector3 a=aw_center(&world,world.path[i-1]),b=aw_center(&world,world.path[i]);
                    a.y+=0.12f;b.y+=0.12f;
                    if(cut_mode==2){float level=target.y+1.5f;
                        if(a.y>level&&b.y>level)continue;
                        if(a.y>level)a=Vector3Lerp(a,b,(a.y-level)/(a.y-b.y));
                        if(b.y>level)b=Vector3Lerp(b,a,(b.y-level)/(b.y-a.y));
                    }
                    Vector3 end=Vector3Lerp(a,b,0.72f);
                    DrawCylinderEx(a,end,0.055f,0.055f,4,(Color){245,201,100,220});
                }
            }
            aw_draw_unit();
        }else{
            /* The wire footprint makes the incomplete terrain readable. */
            for(int z=0;z<=AW_SIZE;z+=4)DrawLine3D((Vector3){0,0.02f,z*AW_UNIT},(Vector3){128,0.02f,z*AW_UNIT},(Color){56,100,108,90});
            for(int x=0;x<=AW_SIZE;x+=4)DrawLine3D((Vector3){x*AW_UNIT,0.02f,0},(Vector3){x*AW_UNIT,0.02f,128},(Color){56,100,108,90});
        }
    }
    EndMode3D();
#ifndef PLATFORM_WEB
    DrawText(TextFormat("ALIENWARS / MAP LAB    SEED %u    %08x",world.seed,world.hash),24,22,20,(Color){223,233,229,255});
    DrawText("Drag: orbit   Right drag: pan   Wheel: zoom   R: new seed   Space: assembly   G: walkability   P: path",24,GetScreenHeight()-30,16,(Color){159,182,183,255});
#endif
    EndDrawing();
    static double last_report=0;
    if(GetTime()-last_report>0.12){aw_publish();last_report=GetTime();}
}

int main(int argc,char **argv) {
    uint32_t seed=73;
    int headless=0,watch=0;
    for(int i=1;i<argc;i++){
        if(strncmp(argv[i],"--seed=",7)==0)seed=(uint32_t)strtoul(argv[i]+7,NULL,10);
        if(strcmp(argv[i],"--headless")==0)headless=1;
        if(strcmp(argv[i],"--watch")==0)watch=1;
        if(strncmp(argv[i],"--symmetry=",11)==0)settings.symmetry=atoi(argv[i]+11);
        if(strncmp(argv[i],"--floor-a=",10)==0)settings.floors_a=atoi(argv[i]+10);
        if(strncmp(argv[i],"--floor-b=",10)==0)settings.floors_b=atoi(argv[i]+10);
        if(strncmp(argv[i],"--biome=",8)==0)settings.biome=atoi(argv[i]+8);
        if(strncmp(argv[i],"--tunnels=",10)==0)settings.tunnels=atoi(argv[i]+10);
    }
    if(headless){
        if(!aw_generate_options(&world,seed,settings))return 1;
        printf("MAP seed=%" PRIu32 " version=%d hash=%08" PRIx32 " walk=%d reached=%d path=%d decisions=%d attempts=%d structure=%d tunnels=%d cost=%d shapes=%d\n",
            seed,AW_VERSION,world.hash,world.walk_count,world.reached_count,world.path_length,world.decisions,world.attempts,world.structure_decisions,world.tunnel_count,world.path_cost,world.shape_decisions);
        return 0;
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_RESIZABLE);
    InitWindow(1280,800,"AlienWars / Map Lab");
    SetTargetFPS(60);
    aw_new(seed,watch);
#ifdef PLATFORM_WEB
    emscripten_set_main_loop(aw_update,0,1);
#else
    while(!WindowShouldClose())aw_update();
    aw_destroy_scene(&scene);CloseWindow();
#endif
    return 0;
}
