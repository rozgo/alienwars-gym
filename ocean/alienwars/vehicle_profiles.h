#ifndef ALIENWARS_VEHICLE_PROFILES_H
#define ALIENWARS_VEHICLE_PROFILES_H
#include "map.h"
enum {AW_VEHICLE_GROUND,AW_VEHICLE_BOAT,AW_VEHICLE_QUAD,AW_VEHICLE_WING,AW_VEHICLE_SUB,AW_VEHICLE_FAMILIES};
typedef struct {float width,length,height,speed,reverse,accel,turn,turn_accel,vertical;} AwVehicleSpec;
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
    if(family==AW_VEHICLE_WING)return variant==1?40:48;
    return 20+variant*10;
}
static const char*aw_vehicle_role(int family,int variant){
    if(family==AW_VEHICLE_GROUND)return (const char*[]){"Scout: agile, narrow access","Rover: traction on rough ground","Hauler: slow, wide clearance"}[aw_clamp(variant,0,2)];
    if(family==AW_VEHICLE_BOAT)return (const char*[]){"Skiff: fast, shallow draft","Patrol boat: balanced reach","Cutter: deep draft, long range"}[aw_clamp(variant,0,2)];
    if(family==AW_VEHICLE_QUAD)return "Quadcopter: hover, strafe, tight access";
    if(family==AW_VEHICLE_WING)return variant==1?"Recon wing: fast, wide sensor reach":"Transport: wide turns, broad sensor reach";
    return (const char*[]){"Recon sub: compact, agile","Patrol sub: balanced reach","Heavy sub: slow, long-range sonar"}[aw_clamp(variant,0,2)];
}
/* Dry-road top speed is traded against rough-ground traction. */
static float aw_ground_traction(int variant,int material){
    static const float traction[3][AW_TILES]={
        {.82f,.76f,.90f,.65f,.70f,.55f,.48f,.45f,.35f,.30f,1},
        {.95f,.90f,.96f,.90f,.95f,.88f,.76f,.85f,.50f,.40f,1},
        {.60f,.52f,.78f,.50f,.55f,.38f,.32f,.28f,.25f,.20f,1}};
    return traction[aw_clamp(variant,0,2)][aw_clamp(material,0,AW_TILES-1)];
}
#endif
