/* Map Lab viewer. The shared map.h also serves the PufferLib environment. */
#include "render.h"
#include "patrol_render.h"
#include "sensor_render.h"
#include "camera_zoom.h"
#include "unit_visibility.h"
#include "unit_lighting.h"
#include "command_fleet.h"
#include <inttypes.h>
#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#define AW_EXPORT EMSCRIPTEN_KEEPALIVE
EM_JS(void,aw_report,(uint32_t seed,uint32_t hash,int valid,int walk,int reached,int length,int decisions,int reductions,int attempts,int resolved,int paused,double milliseconds,int tunnels,int structural,int cut,int layer,float floor,int cost,int shapes,int tour,int depthA,int depthB,int fps,int lakes,int rooms,int baseA,int baseB,int ocean,int mountains,int bridges),{
    if(typeof window !== 'undefined' && window.maplabReport) window.maplabReport({
        seed:seed>>>0,hash:(hash>>>0).toString(16).padStart(8,'0'),valid:!!valid,
        walk,reached,length,decisions,reductions,attempts,resolved,paused:!!paused,milliseconds,
        tunnels,structural,cut:!!cut,layer,floor,cost,shapes,tour,depthA,depthB,fps,lakes,rooms,baseA,baseB,ocean,mountains,bridges
    });
});
#else
#define AW_EXPORT
#define aw_report(...) ((void)0)
#endif

static AwMap world;
static AwScene scene;
static AwPatrols patrols;
static AwCommandFleet fleet;
#ifdef AW_FLECS_EXPLORER
AW_EXPORT const char*aw_explorer_request(const char*method,const char*path){return aw_inspect_request(&fleet.world,method,path);}
#endif
#define sensors fleet.world.sensors
static AwMotion unit_motion[AW_UNITS];
static int sensor_selected=0,sensor_layers=1,sensor_all=0,sensor_xray=1;
static int command_armed=0;
static float command_height=NAN;
static void aw_sensor_update_poses(float dt);
static void aw_publish_sensors(void);
static int patrol_mode=0; /* Live, paused, hidden. */
static AwOptions settings={1,6,6,0,1};
static int cut_mode=1,cut_active=0,follow_scout=0,scout_layer=0,show_tiles=0,scout_tour=0;
static float scout_floor=1;
static int baked_occlusion=1,surface_detail=1;
static int isolate_tunnels=0,show_tunnel_ceilings=0;
static Camera3D camera;
static float yaw=0.75f,pitch=0.9f,zoom=158.0f;
static AwZoom zoom_input;
static void aw_set_zoom(float scale){aw_zoom_reset(&zoom_input,scale);zoom=expf(zoom_input.value);}
AW_EXPORT void aw_camera_zoom(float delta){
    if(!zoom_input.initialized)aw_set_zoom(zoom);
    aw_zoom_push(&zoom_input,delta);
}
static Vector3 focus={64,10,64};
static float revealed=AW_CELLS,animation_time=0,unit_progress=0;
static int show_overlay=0,show_ocean=0,show_path=1,paused=0,unit_paused=0;
static double generation_ms=0;

static void aw_frame_tunnels(void){
    if(!world.cave_count)return;
    Vector3 lo={1000,1000,1000},hi={-1000,-1000,-1000};
    for(int i=0;i<world.cave_count;i++){
        const AwCaveNode*n=&world.cave[i];
        float x=(n->x+0.5f)*AW_UNIT,z=(n->z+0.5f)*AW_UNIT,r=aw_cave_radius(n->profile)*AW_UNIT;
        lo.x=fminf(lo.x,x-r);hi.x=fmaxf(hi.x,x+r);lo.z=fminf(lo.z,z-r);hi.z=fmaxf(hi.z,z+r);
        lo.y=fminf(lo.y,aw_y(n->q/4.0f));hi.y=fmaxf(hi.y,aw_y((n->q+aw_cave_height(n->profile))/4.0f));
    }
    for(int i=0;i<world.trail_count;i++){
        const AwTrailNode*n=&world.trail[i];float x=(n->x+.5f)*AW_UNIT,z=(n->z+.5f)*AW_UNIT,r=aw_trail_radius(n->profile)*AW_UNIT;
        lo.x=fminf(lo.x,x-r);hi.x=fmaxf(hi.x,x+r);lo.z=fminf(lo.z,z-r);hi.z=fmaxf(hi.z,z+r);
        lo.y=fminf(lo.y,aw_y(n->q/4.0f));hi.y=fmaxf(hi.y,aw_y((n->q+aw_trail_height(n->profile))/4.0f));
    }
    focus=Vector3Scale(Vector3Add(lo,hi),0.5f);yaw=0.75f;pitch=0.8f;
    aw_set_zoom(Clamp(Vector3Distance(lo,hi)*1.12f,16,210));
}
static void aw_set_isolation(int value){
    /* Visibility only: both modes share the same camera, including any orbit,
     * pan or zoom performed while isolated. Framing is an explicit action. */
    isolate_tunnels=!!value&&world.cave_count>0;
}

static void aw_publish(void) {
    aw_report(world.seed,world.hash,world.valid,world.walk_count,world.reached_count,world.path_length,
        world.decisions,world.reductions,world.attempts,(int)revealed,paused,generation_ms,world.tunnel_count,world.structure_decisions+world.cave_decisions,cut_active,scout_layer,scout_floor,world.path_cost,world.shape_decisions,scout_tour,world.cave_hubs[0]>=0?world.cave[world.cave_hubs[0]].q:0,world.cave_hubs[1]>=0?world.cave[world.cave_hubs[1]].q:0,GetFPS(),world.lake_count,world.cave_count?2+world.cave_room_count:0,world.spawns[0],world.spawns[1],world.ocean_count,world.mountain_count,world.bridge_count);
    aw_publish_sensors();
}

AW_EXPORT void aw_new(uint32_t seed,int watch) {
    double start=GetTime();
    if(!aw_generate_options(&world,seed,settings)){
        generation_ms=(GetTime()-start)*1000;
        aw_publish();
        return;
    }
    aw_patrol_build(&world,&patrols);aw_command_fleet_init(&fleet,&world,&patrols);memset(unit_motion,0,sizeof(unit_motion));
    generation_ms=(GetTime()-start)*1000;
    aw_build_scene(&scene,&world);aw_set_occlusion(&scene,baked_occlusion);aw_set_detail(&scene,surface_detail);
    if(isolate_tunnels)aw_set_isolation(world.cave_count>0);
    revealed=watch?0:AW_CELLS;
    unit_progress=0;paused=0;scout_tour=0;
    aw_sensor_update_poses(0);
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
    if(option==7)aw_set_isolation(value);
    if(option==8)show_tunnel_ceilings=!!value;
    if(option==9)show_ocean=!!value;
    if(option==10){baked_occlusion=!!value;aw_set_occlusion(&scene,baked_occlusion);}
    if(option==11){surface_detail=!!value;aw_set_detail(&scene,surface_detail);}
    if(option==12)patrol_mode=aw_clamp(value,0,2);
    if(option==13){aw_command_fleet_init(&fleet,&world,&patrols);unit_paused=0;patrol_mode=0;aw_sensor_update_poses(0);}
    aw_publish();
}

AW_EXPORT void aw_watch(void) {
    revealed=0;unit_progress=0;paused=0;aw_command_fleet_scout_route(&fleet,&world,0);aw_sensor_teleport(&sensors,0);unit_motion[0]=(AwMotion){0};aw_publish();
}

AW_EXPORT void aw_step(void) {
    paused=1;
    revealed=fminf((float)AW_CELLS,revealed+1);
    aw_publish();
}

AW_EXPORT void aw_camera_control(int action) {
    if(action==0){follow_scout=0;if(isolate_tunnels)aw_frame_tunnels();else{yaw=0.75f;pitch=0.9f;aw_set_zoom(158);focus=(Vector3){64,10,64};}}
    if(action==9){aw_set_isolation(0);yaw=.75f;pitch=.9f;aw_set_zoom(235);focus=(Vector3){64,0,64};follow_scout=0;}
    if(action==1)aw_camera_zoom(logf(.84f));
    if(action==2)aw_camera_zoom(-logf(.84f));
    if(action==3)yaw-=0.22f;
    if(action==4)yaw+=0.22f;
    if(action>=5&&action<=8){
        Vector3 right={cosf(yaw),0,-sinf(yaw)},forward={sinf(yaw),0,cosf(yaw)};
        Vector3 shift=action<7?right:forward;
        float amount=(action==5||action==7)?-3.0f:3.0f;
        focus=Vector3Add(focus,Vector3Scale(shift,amount));
        focus.x=Clamp(focus.x,-32,160);focus.z=Clamp(focus.z,-32,160);
    }
}

AW_EXPORT void aw_resize(int width,int height) {
    if(width<240||height<200||width>3840||height>2400)return;
    if(IsWindowReady())SetWindowSize(width,height);
}

static void aw_draw_markers(void) {
    if(cut_mode==2||isolate_tunnels)return; /* Surface beacons are outside the underground section. */
    for(int s=0;s<2;s++){
        Vector3 p=aw_center(&world,world.spawns[s]);
        Color accent=s?(Color){249,161,88,255}:(Color){101,225,222,255};
        DrawCylinderWires((Vector3){p.x,p.y+0.24f,p.z},2.2f,2.2f,0.02f,32,accent);
    }
    for(int side=0;side<2;side++)if(world.cave_entrances[side]>=0){
        Vector3 p=aw_center(&world,AW_CELLS+world.cave_entrances[side]);p.y+=0.12f;
        Color accent={187,163,245,255};
        DrawCylinderWires(p,1.1f,1.1f,0.06f,16,accent);
        Vector3 top=p;top.y+=1.8f;DrawLine3D(p,top,accent);DrawSphereEx(top,.13f,4,6,accent);
    }
}

static Vector3 aw_scout_position_at(float progress,int report) {
    int length=world.path_length;
    if(length<2)return aw_center(&world,world.spawns[0]);
    float cycle=fmodf(progress,(float)(length-1)*2);
    float p=cycle>length-1?(length-1)*2-cycle:cycle;
    int segment=(int)p;
    if(segment>=length-1)segment=length-2;
    int layer=world.path[segment]>=AW_CELLS;
    float floor=aw_corner_q(&world,world.path[segment],0)/4.0f;
    Vector3 position=Vector3Lerp(aw_center(&world,world.path[segment]),aw_center(&world,world.path[segment+1]),p-segment);
    float gx=position.x/AW_UNIT,gz=position.z/AW_UNIT;
    if(world.path[segment]>=AW_CELLS||world.path[segment+1]>=AW_CELLS){
        float q=aw_lerp(aw_corner_q(&world,world.path[segment],0),aw_corner_q(&world,world.path[segment+1],0),p-segment);
        position.y=aw_y(aw_support_q(&world,gx,gz,q)/4);
    }else{
        int node=(int)gz*AW_SIZE+(int)gx;position.y=aw_ground_y(&world,node,gx-(int)gx,gz-(int)gz);
    }
    if(world.path[segment]>=AW_SPAN_START){
        float q=(position.y+1.2f)/.75f;layer=0;
        for(float h=q+3;h<aw_height_q(&world,gx,gz)+1;h+=.5f)if(aw_density(&world,gx,h,gz)>0){layer=1;break;}
    }
    if(report){scout_layer=layer;scout_floor=floor;}
    return position;
}

static Vector3 aw_unit_position(void){
    if(!fleet.ready||!fleet.active[0]||fleet.route[0].count<2)return aw_scout_position_at(unit_progress,1);
    AwSVec p=aw_command_fleet_pose(&fleet,0).position;scout_floor=(p.y+1.2f)/3;
    int cursor=aw_clamp(fleet.unit[0].cursor,0,fleet.route[0].count-1),node=fleet.route[0].node[cursor];
    scout_layer=node>=AW_CELLS&&node<AW_SPAN_START;
    if(node>=AW_SPAN_START){float q=(p.y+1.2f)/.75f;for(float h=q+3;h<aw_height_q(&world,p.x*.5f,p.z*.5f)+1;h+=.5f)if(aw_density(&world,p.x*.5f,h,p.z*.5f)>0){scout_layer=1;break;}}
    return (Vector3){p.x,p.y,p.z};
}

static void aw_sensor_update_poses(float dt){
    for(int i=0;i<AW_UNITS;i++){
        AwVehicle pose=aw_command_fleet_pose(&fleet,i);const AwVehicle*v=&pose;
        unit_motion[i].yaw=v->yaw;unit_motion[i].pitch=v->pitch;
    }(void)dt;
}
#ifdef PLATFORM_WEB
EM_JS(void,aw_sensor_report,(int unit,const float* values),{
    if(typeof window!=='undefined'&&window.maplabSensors)window.maplabSensors(unit,Array.from(HEAPF32.subarray(values>>2,(values>>2)+129)));
});
#endif
#ifdef PLATFORM_WEB
EM_JS(void,aw_command_fleet_report,(int trained,int selected,const char* role,const float* values),{
    if(window.maplabFleet)window.maplabFleet({trained:!!trained,selected,role:UTF8ToString(role),values:Array.from(HEAPF32.subarray(values>>2,(values>>2)+26))});
});
#endif
static void aw_publish_sensors(void){
#ifdef PLATFORM_WEB
    if(!world.valid||!sensors.count)return;const AwSensorUnit*u=&sensors.units[sensor_selected];
    float values[129]={u->pose.position.x,u->pose.position.y,u->pose.position.z,u->pose.yaw*RAD2DEG,u->pose.pitch*RAD2DEG,u->pose.roll*RAD2DEG,
        aw_sv_length(u->odometry.velocity),u->odometry.distance,(float)sensors.time};
    for(int t=0;t<4;t++){
        const AwSensorReading*r=&u->reading[t];const AwSensorConfig*c=&u->config[t];int hits=0;
        if(t==AW_SENSOR_RF){for(int j=0;j<sensors.count;j++)hits+=r->peers[j].detected;}
        else for(int j=0;j<r->count;j++)hits+=r->beams[j].hit.kind!=AW_HIT_NONE&&r->beams[j].hit.kind!=AW_HIT_BOUNDARY;
        float*v=values+9+t*6;v[0]=c->enabled;v[1]=r->valid;v[2]=c->range;v[3]=1/c->period;v[4]=(float)(sensors.time-r->stamp);v[5]=hits;
    }
    const AwSensorReading*r=&u->reading[AW_SENSOR_CAMERA];
    if(r->valid)for(int i=0;i<48;i++){values[33+2*i]=r->beams[i].hit.distance/u->config[AW_SENSOR_CAMERA].range;values[34+2*i]=r->beams[i].hit.kind;}
    aw_sensor_report(sensor_selected,values);
    const AwVehicle*v=&fleet.unit[sensor_selected].vehicle;AwVehicleSpec spec=aw_vehicle_spec(v->family,v->variant);
    AwSVec goal=fleet.destination[sensor_selected];
    float profile[26]={spec.speed,spec.turn,spec.width*2,spec.length*2,aw_vehicle_sensor_range(v->family,v->variant),v->failed,v->contact,(float)fleet.arrivals[sensor_selected],(float)fleet.unit[sensor_selected].cursor,(float)fleet.route[sensor_selected].count,
        fleet.status[sensor_selected],fleet.selection,fleet.unit[sensor_selected].remaining,fleet.command_result,fleet.world.ticks*.1f,goal.x,goal.y,goal.z,v->family,fleet.active[sensor_selected],fleet.command_requested,command_armed,fleet.total_contacts[sensor_selected],fleet.total_blocked[sensor_selected],fleet.total_collisions[sensor_selected],fleet.unit[sensor_selected].ticks};
    aw_command_fleet_report(fleet.trained,sensor_selected,fleet.active[sensor_selected]?aw_vehicle_role(v->family,v->variant):"Unavailable: no body-clear route",profile);
#endif
}
AW_EXPORT void aw_sensor_control(int action,int value){
    if(action==0){sensor_selected=aw_clamp(value,0,AW_UNITS-1);fleet.selection=1<<sensor_selected;command_height=NAN;}
    if(action==1)sensor_layers=value&15;
    if(action==2)sensor_all=!!value;
    if(action==3)sensor_xray=!!value;
    if(action==4&&sensors.count){
        aw_sensor_update_poses(0);follow_scout=0;aw_set_isolation(0);
        focus=aw_sensor_v3(sensors.units[sensor_selected].pose.position);aw_set_zoom(68);pitch=.82f;
    }
    if(action>=5&&action<=8&&sensors.count){
        int t=action-5;AwSensorConfig c=sensors.units[sensor_selected].config[t];c.enabled=!!value;aw_sensor_attach(&sensors,sensor_selected,t,c);aw_mission_observe(&fleet.world);
    }aw_publish_sensors();
}

AW_EXPORT int aw_move_to(float x,float y,float z,int mask){
    if(!fleet.ready||!isfinite(x)||!isfinite(y)||!isfinite(z))return 0;
    int count=0,ordinal=0,accepted=0;mask&=(1<<AW_UNITS)-1;
    for(int i=0;i<AW_UNITS;i++)if(mask&(1<<i))count++;
    for(int i=0;i<AW_UNITS;i++)if(mask&(1<<i)){
        AwSVec target={x+(ordinal++-(count-1)*.5f)*4.5f,y,z};
        const AwVehicle*v=&fleet.unit[i].vehicle;
        int selected_family=fleet.unit[sensor_selected].vehicle.family;
        if(v->family!=selected_family&&(v->family==AW_VEHICLE_QUAD||v->family==AW_VEHICLE_WING||v->family==AW_VEHICLE_SUB))target.y=v->position.y;
        if(v->family==AW_VEHICLE_BOAT)target.y=-.12f;
        if(v->family==AW_VEHICLE_GROUND){float q=(y+1.2f)/.75f;if(selected_family!=AW_VEHICLE_GROUND&&target.x>=0&&target.z>=0&&target.x<128&&target.z<128){int cell=(int)(target.z*.5f)*64+(int)(target.x*.5f);q=aw_surface_q(&world,cell,fmodf(target.x*.5f,1),fmodf(target.z*.5f,1));}target.y=aw_support_q(&world,target.x*.5f,target.z*.5f,q)*.75f-1.2f;}
        accepted+=aw_command_fleet_destination(&fleet,&world,i,target,0);
    }
    fleet.command_requested=count;fleet.command_result=accepted;command_armed=0;aw_publish_sensors();return accepted;
}
AW_EXPORT void aw_command_control(int action,float value){
    if(!fleet.ready)return;
    if(action==0){int family=fleet.unit[sensor_selected].vehicle.family;fleet.selection=0;
        for(int i=0;i<AW_UNITS;i++)if(fleet.active[i]&&fleet.unit[i].vehicle.family==family)fleet.selection|=1<<i;
    }
    if(action==1)command_armed=!!value;
    if(action==2)command_height=value;
    if(action==3)fleet.selection=1<<sensor_selected;
    aw_publish_sensors();
}
static void aw_command_pointer(Vector2 point,int move,int add){
    if(!fleet.ready||point.x<0||point.y<0||point.x>=GetScreenWidth()||point.y>=GetScreenHeight())return;
    if(!move){
        int selected=-1;float closest=24;
        for(int i=0;i<AW_UNITS;i++)if(fleet.active[i]){
            Vector2 screen=GetWorldToScreen(aw_sensor_v3(aw_command_fleet_pose(&fleet,i).position),camera);
            float distance=Vector2Distance(screen,point);if(distance<closest){closest=distance;selected=i;}
        }
        if(selected>=0){sensor_selected=selected;fleet.selection=add?(fleet.selection^(1<<selected)):1<<selected;if(!fleet.selection)fleet.selection=1<<selected;command_height=NAN;aw_publish_sensors();}return;
    }
    Ray ray=GetScreenToWorldRay(point,camera);const AwVehicle*v=&fleet.unit[sensor_selected].vehicle;
    AwSVec origin={ray.position.x,ray.position.y,ray.position.z},direction={ray.direction.x,ray.direction.y,ray.direction.z},target;
    if(v->family!=AW_VEHICLE_GROUND){
        float height=v->family==AW_VEHICLE_BOAT?-.12f:isfinite(command_height)?command_height:v->position.y;
        if(fabsf(direction.y)<.00001f)return;float distance=(height-origin.y)/direction.y;if(distance<0)return;
        target=aw_sv_add(origin,aw_sv_scale(direction,distance));
    }else if(isolate_tunnels){
        float closest=28;int found=0;
        for(int n=AW_CELLS;n<AW_NODES;n++)if(fleet.planner.ground[v->variant][n]){
            AwPatrolGraph graph={&world,AW_PATROL_GROUND,v->variant,fleet.planner.ground[v->variant]};
            AwSVec p=aw_mission_world(aw_patrol_node(&graph,n));Vector2 screen=GetWorldToScreen(aw_sensor_v3(p),camera);
            float distance=Vector2Distance(screen,point);if(distance<closest){closest=distance;target=p;found=1;}
        }if(!found)return;
    }else{
        AwSensorHit hit=aw_ray_terrain(&world,&sensors.rays,origin,direction,1000,1);
        if(hit.kind!=AW_HIT_TERRAIN&&hit.kind!=AW_HIT_WATER)return;
        target=aw_sv_add(origin,aw_sv_scale(direction,hit.distance));
    }
    aw_move_to(target.x,target.y,target.z,fleet.selection);
}

AW_EXPORT void aw_command_click(float x,float y,int button,int add){
    aw_command_pointer((Vector2){x,y},button==2||command_armed,add);
}

static void aw_tour(int entrance){
    if(!world.options.tunnels)return;
    int a=world.cave_entrances[0],b=world.cave_entrances[1];
    if(a<0||b<0)return;
    uint8_t surface[AW_CELLS];memcpy(surface,world.walkable,AW_CELLS);memset(world.walkable,0,AW_CELLS);
    int valid=aw_find_path(&world,AW_CELLS+a,AW_CELLS+b);memcpy(world.walkable,surface,AW_CELLS);if(!valid)return;
    unit_progress=0;scout_tour=1;aw_sensor_teleport(&sensors,0);unit_motion[0]=(AwMotion){0};
    if(!entrance){int deepest=0;for(int i=0;i<world.path_length;i++){
        int a=world.path[i],b=world.path[deepest],ca=aw_node_cell(&world,a),cb=aw_node_cell(&world,b);
        int qa=aw_corner_q(&world,a,0),qb=aw_corner_q(&world,b,0);
        int da=aw_abs(ca%64-32)+aw_abs(ca/64-32),db=aw_abs(cb%64-32)+aw_abs(cb/64-32);
        if(qa<qb||(qa==qb&&da<db))deepest=i;
    }unit_progress=(float)deepest;}
    aw_command_fleet_scout_route(&fleet,&world,(int)unit_progress);unit_paused=1;revealed=AW_CELLS;follow_scout=0;
    focus=aw_unit_position();aw_set_zoom(entrance?30:38);yaw=entrance?-1.4f:0.9f;pitch=0.85f;aw_publish();
}
AW_EXPORT void aw_inspect_tunnel(void){aw_tour(0);}
AW_EXPORT void aw_inspect_entrance(void){aw_tour(1);}
AW_EXPORT void aw_inspect_mountain(int bypass){
    if(!world.valid||!world.mountain_count)return;
    int branch=!!bypass;const AwMountain*r=&world.mountains[0];
    world.path_length=0;world.path_cost=0;
    for(int i=0;i<r->trail_length[branch];i++){
        int node=AW_SPAN_START+world.trail_span[r->trail[branch][i]];
        if(world.path_length&&node==world.path[world.path_length-1])continue;
        if(world.path_length)world.path_cost+=aw_move_cost(&world,world.path[world.path_length-1],node);
        world.path[world.path_length++]=node;
    }
    aw_sensor_teleport(&sensors,0);unit_motion[0]=(AwMotion){0};aw_set_isolation(0);scout_tour=2+branch;unit_progress=0;aw_command_fleet_scout_route(&fleet,&world,0);unit_paused=0;show_path=1;revealed=AW_CELLS;follow_scout=0;
    focus=(Vector3){(r->x+2*r->step+.5f)*AW_UNIT,aw_y(2),(r->z+2*r->step+.5f)*AW_UNIT};
    aw_set_zoom(4*r->step*AW_UNIT+22);pitch=.8f;yaw=.75f+r->rotation*1.5707963f;aw_publish();
}

AW_EXPORT void aw_inspect_bridge(void){
    static int next=0;if(!world.valid||!world.bridge_count)return;
    const AwBridge*b=&world.bridges[next++%world.bridge_count];
    world.path_length=world.path_cost=0;
    for(int u=0;u<=b->length;u++){
        int c=(b->z+b->dz*u)*64+b->x+b->dx*u;
        int n=AW_SPAN_START+aw_span_find(&world,c,aw_bridge_q(b,u));
        if(world.path_length)world.path_cost+=aw_move_cost(&world,world.path[world.path_length-1],n);
        world.path[world.path_length++]=n;
    }
    aw_sensor_teleport(&sensors,0);unit_motion[0]=(AwMotion){0};aw_set_isolation(0);scout_tour=4;unit_progress=0;aw_command_fleet_scout_route(&fleet,&world,0);unit_paused=0;show_path=1;revealed=AW_CELLS;follow_scout=0;
    focus=aw_bridge_point(b,b->length*.5f,0,0);aw_set_zoom(b->length*AW_UNIT+16);pitch=.70f;yaw=b->dx?.8f:2.3f;aw_publish();
}

AW_EXPORT int aw_inspect_patrol(void){
    static unsigned next=0;if(!world.valid||!patrols.count)return -1;
    int i=next++%AW_PATROLS;const AwPatrol*p=&patrols.units[i];if(p->count<2)return -1;
    AwSVec point=fleet.unit[i+1].vehicle.position;
    aw_set_isolation(0);follow_scout=0;focus=(Vector3){point.x,point.y,point.z};
    sensor_selected=i+1;aw_set_zoom(p->layer==AW_PATROL_AIR?45:24);pitch=.8f;yaw=.75f;aw_publish();return i;
}

static void aw_draw_vehicle(int i){
    if(!fleet.active[i])return;AwVehicle pose=aw_command_fleet_pose(&fleet,i);const AwVehicle*v=&pose;
    rlPushMatrix();rlTranslatef(v->position.x,v->position.y,v->position.z);rlRotatef(v->yaw*RAD2DEG,0,1,0);rlRotatef(-v->pitch*RAD2DEG,1,0,0);
    if(v->family==AW_VEHICLE_WING)rlRotatef(-v->yaw_rate*42,0,0,1);
    aw_patrol_model(i?patrols.units[i-1].layer:0,v->variant,animation_time);rlPopMatrix();
}
static void aw_draw_unit(void){aw_draw_vehicle(0);}
static void aw_draw_fleet(void){for(int i=1;i<AW_UNITS;i++)aw_draw_vehicle(i);}

static void aw_update(void) {
    float dt=fminf(GetFrameTime(),0.05f);
    animation_time+=dt;
    if(!paused&&revealed<AW_CELLS)revealed=fminf(AW_CELLS,revealed+dt*320);
    if(revealed>=AW_CELLS)aw_command_fleet_step(&fleet,&world,dt,!unit_paused,patrol_mode==0);
    Vector2 mouse=GetMouseDelta();
    int orbit_modifier=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
    if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||(IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&!orbit_modifier)){
        Vector3 right={cosf(yaw),0,-sinf(yaw)},forward={sinf(yaw),0,cosf(yaw)};
        focus=Vector3Add(focus,Vector3Scale(right,-mouse.x*zoom/GetScreenHeight()));
        focus=Vector3Add(focus,Vector3Scale(forward,-mouse.y*zoom/GetScreenHeight()));
        focus.x=Clamp(focus.x,-32,160);focus.z=Clamp(focus.z,-32,160);
    }else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
        yaw-=mouse.x*0.005f;pitch=Clamp(pitch+mouse.y*0.004f,0.42f,1.35f);
    }
#ifndef PLATFORM_WEB
    aw_camera_zoom(-GetMouseWheelMove()*.095f);
#endif
    if(!zoom_input.initialized)aw_set_zoom(zoom);
    zoom=aw_zoom_step(&zoom_input,dt);
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
    if(world.valid&&revealed>=AW_CELLS){aw_sensor_update_poses(dt);}
    if(follow_scout&&fleet.active[sensor_selected])focus=aw_sensor_v3(aw_command_fleet_pose(&fleet,sensor_selected).position);
    camera.target=focus;
    camera.position=(Vector3){focus.x+sinf(yaw)*cosf(pitch)*400,focus.y+sinf(pitch)*400,focus.z+cosf(yaw)*cosf(pitch)*400};
    float aspect=(float)GetScreenWidth()/(float)GetScreenHeight();
    camera.up=(Vector3){0,1,0};camera.fovy=zoom*fmaxf(1.0f,1.35f/aspect);camera.projection=CAMERA_ORTHOGRAPHIC;
    if(world.valid&&!isolate_tunnels)aw_prepare_reflection(&scene,camera,revealed-1);
    BeginDrawing();
    ClearBackground((Color){7,13,18,255});
    BeginMode3D(camera);
    if(world.valid){
        Vector3 target=scout;target.y+=0.55f;
        Vector3 eye=Vector3Add(target,Vector3Scale(Vector3Normalize(Vector3Subtract(camera.position,camera.target)),190));
        int blocked=aw_occluded(&world,eye.x/AW_UNIT,(eye.y+1.2f)/0.75f,eye.z/AW_UNIT,target.x/AW_UNIT,(target.y+1.2f)/0.75f,target.z/AW_UNIT);
        cut_active=!isolate_tunnels&&(cut_mode==2||(cut_mode==1&&blocked));
        aw_draw_scene(&scene,revealed-1,animation_time,show_overlay,show_ocean,eye,target,cut_active?cut_mode:0,isolate_tunnels?(show_tunnel_ceilings?2:1):0);
        if(revealed>=AW_CELLS||isolate_tunnels){
            if(show_tiles&&!isolate_tunnels){
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
            if(show_path&&isolate_tunnels){
                for(int i=0;i<world.trail_edge_count;i++){
                    const AwTrailEdge*e=&world.trail_edges[i];
                    Vector3 a=aw_center(&world,AW_SPAN_START+world.trail_span[e->a]),b=aw_center(&world,AW_SPAN_START+world.trail_span[e->b]);
                    a.y+=.12f;b.y+=.12f;DrawCylinderEx(a,Vector3Lerp(a,b,.72f),.055f,.055f,4,e->branch?(Color){119,204,180,220}:(Color){245,201,100,220});
                }
                for(int i=0;i<world.cave_edge_count;i++){
                    Vector3 a=aw_center(&world,AW_CELLS+world.cave_edges[i].a),b=aw_center(&world,AW_CELLS+world.cave_edges[i].b);
                    a.y+=0.12f;b.y+=0.12f;DrawCylinderEx(a,Vector3Lerp(a,b,0.72f),0.055f,0.055f,4,(Color){245,201,100,220});
                }
                for(int i=0;i<world.cave_count;i++)if(world.cave[i].portal>=0){
                    Vector3 p=aw_center(&world,AW_CELLS+i);p.y+=0.15f;
                    DrawCylinderWires(p,0.6f,0.6f,0.04f,12,(Color){101,225,222,255});
                }
            }
            if(show_path&&!isolate_tunnels){
                rlDrawRenderBatchActive();rlDisableDepthTest();rlDisableDepthMask();
                for(int unit=0;unit<AW_UNITS;unit++)if(fleet.active[unit]&&(fleet.selection&(1<<unit))){
                    const AwMissionRoute*r=&fleet.route[unit];
                    for(int i=1;i<r->count;i++){
                        Vector3 a=aw_sensor_v3(r->point[i-1]),b=aw_sensor_v3(r->point[i]);
                        a.y+=.12f;b.y+=.12f;
                        DrawCylinderEx(a,Vector3Lerp(a,b,.72f),.055f,.055f,4,(Color){245,201,100,150});
                    }
                }
                rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableDepthTest();
            }
            aw_units_through_begin();
            if(!isolate_tunnels||scout_layer)aw_draw_unit();
            if(!isolate_tunnels&&patrol_mode!=2)aw_draw_fleet();
            aw_units_through_end();
            aw_units_lit_begin();
            if(!isolate_tunnels||scout_layer)aw_draw_unit();
            if(!isolate_tunnels&&patrol_mode!=2)aw_draw_fleet();
            aw_units_lit_end();
            rlDrawRenderBatchActive();rlDisableDepthTest();rlDisableDepthMask();
            for(int i=0;i<AW_UNITS;i++)if(fleet.active[i]&&(fleet.selection&(1<<i))){
                Vector3 p=aw_sensor_v3(aw_command_fleet_pose(&fleet,i).position);p.y+=.08f;
                DrawCylinderWires(p,1.5f,1.5f,.04f,24,(Color){118,217,195,255});
                Vector3 goal=aw_sensor_v3(fleet.destination[i]);goal.y+=.1f;
                DrawCylinderWires(goal,1,1,.08f,16,(Color){235,197,111,255});
            }
            rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableDepthTest();
            aw_sensors_draw(&sensors,sensor_selected,sensor_layers,sensor_all,sensor_xray,animation_time,isolate_tunnels,patrol_mode==2);
        }else{
            /* The wire footprint makes the incomplete terrain readable. */
            for(int z=0;z<=AW_SIZE;z+=4)DrawLine3D((Vector3){0,0.02f,z*AW_UNIT},(Vector3){128,0.02f,z*AW_UNIT},(Color){56,100,108,90});
            for(int x=0;x<=AW_SIZE;x+=4)DrawLine3D((Vector3){x*AW_UNIT,0.02f,0},(Vector3){x*AW_UNIT,0.02f,128},(Color){56,100,108,90});
        }
    }
    EndMode3D();
#ifndef PLATFORM_WEB
    DrawText(TextFormat("ALIENWARS / MAP LAB    SEED %u    %08x",world.seed,world.hash),24,22,20,(Color){223,233,229,255});
    DrawText("Drag: pan   Shift-drag: orbit   Wheel: zoom   R: new seed   Space: assembly   G: walkability   P: path",24,GetScreenHeight()-30,16,(Color){159,182,183,255});
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
        printf("MAP seed=%" PRIu32 " version=%d hash=%08" PRIx32 " walk=%d reached=%d path=%d decisions=%d attempts=%d structure=%d tunnels=%d cost=%d shapes=%d entrance_ne=%d entrance_sw=%d depth_ne=%d depth_sw=%d lakes=%d rooms=%d base_a=%d base_b=%d landforms=%d plans=%d ocean=%d bridges=%d\n",
            seed,AW_VERSION,world.hash,world.walk_count,world.reached_count,world.path_length,world.decisions,world.attempts,world.structure_decisions,world.tunnel_count,world.path_cost,world.shape_decisions,world.cave_entrances[0]>=0?world.cave[world.cave_entrances[0]].portal:-1,world.cave_entrances[1]>=0?world.cave[world.cave_entrances[1]].portal:-1,world.cave_hubs[0]>=0?world.cave[world.cave_hubs[0]].q:0,world.cave_hubs[1]>=0?world.cave[world.cave_hubs[1]].q:0,world.lake_count,world.cave_count?2+world.cave_room_count:0,world.spawns[0],world.spawns[1],world.landform_count,world.layout_attempts,world.ocean_count,world.bridge_count);
        return 0;
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_RESIZABLE);
    InitWindow(1280,800,"AlienWars / Map Lab");
#ifdef PLATFORM_WEB
    /* Raylib 5.5's non-VAO batch path binds normals unconditionally even when
     * its default shader omits them. Keep that unused attribute at slot 2,
     * rather than submitting -1 to WebGL for every scout/guide draw. */
    int *default_locs=rlGetShaderLocsDefault();
    if(default_locs[SHADER_LOC_VERTEX_NORMAL]<0)default_locs[SHADER_LOC_VERTEX_NORMAL]=RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL;
#endif
    SetTargetFPS(60);
    aw_new(seed,watch);
#ifdef PLATFORM_WEB
    emscripten_set_main_loop(aw_update,0,1);
#else
    while(!WindowShouldClose())aw_update();
    aw_command_fleet_close(&fleet);aw_destroy_scene(&scene);CloseWindow();
#endif
    return 0;
}
