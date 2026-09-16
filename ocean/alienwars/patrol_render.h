#ifndef ALIENWARS_PATROL_RENDER_H
#define ALIENWARS_PATROL_RENDER_H
#include "patrols.h"
#include "motion.h"
/* All models share +Z forward, +X right, +Y up. Bright front lamps/cockpits
 * and rear exhaust/propellers make heading legible at RTS scale. */
static void aw_model_wheel(Vector3 p,float radius,float width){
    DrawCylinderEx((Vector3){p.x-width*.5f,p.y,p.z},(Vector3){p.x+width*.5f,p.y,p.z},radius,radius,12,(Color){29,32,33,255});
    DrawCylinderEx((Vector3){p.x-width*.53f,p.y,p.z},(Vector3){p.x+width*.53f,p.y,p.z},radius*.48f,radius*.48f,10,(Color){120,126,124,255});
}
static void aw_model_wedge(Vector3 center,float width,float length,float low,float high,Color color){
    Vector3 a={center.x-width,center.y+low,center.z+length},b={center.x+width,center.y+low,center.z+length};
    Vector3 c={center.x-width,center.y+high,center.z-length},d={center.x+width,center.y+high,center.z-length};
    DrawTriangle3D(a,c,d,color);DrawTriangle3D(a,d,b,color);DrawTriangle3D(d,c,a,color);DrawTriangle3D(b,d,a,color);
    DrawTriangle3D(a,(Vector3){a.x,center.y,a.z},c,color);DrawTriangle3D(b,d,(Vector3){b.x,center.y,b.z},color);
}
static void aw_patrol_model(int layer,int variant,float time){
    Color dark={37,43,46,255},metal={114,128,129,255},light={159,169,166,255},glass={47,110,129,255},front={175,239,229,255},rear={226,124,79,255};
    Color accent=layer==AW_PATROL_GROUND?(Color){197,161,98,255}:layer==AW_PATROL_SUB?(Color){84,157,159,255}:layer==AW_PATROL_NAVAL?(Color){101,157,168,255}:(Color){172,186,188,255};
    if(layer==AW_PATROL_GROUND){
        if(variant==0)rlScalef(.78f,.73f,1);
        float length=variant==0?1.05f:variant==1?1.8f:2.45f;
        DrawCube((Vector3){0,.36f,0},1.06f,.36f,length,metal);
        aw_model_wedge((Vector3){0,.47f,length*.20f},.47f,length*.25f,.07f,.24f,accent);
        if(variant==1){
            for(int side=-1;side<=1;side+=2){
                DrawCube((Vector3){side*.61f,.27f,0},.27f,.44f,length+.10f,dark);
                for(int j=0;j<5;j++)aw_model_wheel((Vector3){side*.63f,.27f,-.72f+j*.36f},.185f,.29f);
                for(int j=0;j<12;j++)DrawCube((Vector3){side*.61f,.50f,-.83f+j*.15f},.30f,.035f,.07f,metal);
            }
            DrawCylinder((Vector3){0,.64f,-.12f},.36f,.42f,.18f,12,accent);
            DrawCube((Vector3){0,.78f,-.16f},.48f,.13f,.38f,metal);
            DrawCylinderEx((Vector3){0,.77f,.08f},(Vector3){0,.77f,.96f},.065f,.045f,8,dark);
        }else{
            int wheels=variant==0?2:3;
            for(int side=-1;side<=1;side+=2)for(int j=0;j<wheels;j++)aw_model_wheel((Vector3){side*.62f,.25f,(j-(wheels-1)*.5f)*(variant==0?.68f:.78f)},.24f,.22f);
            DrawCube((Vector3){0,.70f,length*.25f},.84f,.38f,length*.30f,accent);
            aw_model_wedge((Vector3){0,.75f,length*.37f},.36f,.13f,0,.22f,glass);
            if(variant==2){DrawCube((Vector3){0,.68f,-.48f},.97f,.55f,1.2f,dark);for(int j=0;j<5;j++)DrawCube((Vector3){0,.97f,-.94f+j*.24f},.98f,.035f,.06f,metal);}
            else{DrawCylinder((Vector3){0,.88f,-.25f},.13f,.13f,.19f,10,dark);DrawSphereEx((Vector3){0,1.04f,-.25f},.12f,6,10,glass);}
        }
        for(int side=-1;side<=1;side+=2){DrawCube((Vector3){side*.36f,.54f,length*.51f},.15f,.10f,.045f,front);DrawCube((Vector3){side*.38f,.46f,-length*.51f},.12f,.075f,.045f,rear);}
        DrawCylinderEx((Vector3){-.39f,.65f,-length*.34f},(Vector3){-.39f,1.13f,-length*.34f},.018f,.009f,5,dark);
    }else if(layer==AW_PATROL_NAVAL){
        float width=.72f+variant*.29f,length=1.75f+variant*.82f;
        Vector3 v[6]={{0,.10f,length*.60f},{width*.48f,.10f,length*.26f},{width*.50f,.10f,-length*.46f},{-width*.50f,.10f,-length*.46f},{-width*.48f,.10f,length*.26f},{0,-.23f,-length*.05f}};
        for(int i=0;i<5;i++){int j=(i+1)%5;DrawTriangle3D(v[5],v[j],v[i],dark);DrawTriangle3D((Vector3){0,.10f,0},v[i],v[j],accent);DrawTriangle3D(v[i],v[5],v[j],metal);}
        DrawCube((Vector3){0,.22f,-length*.08f},width*.65f,.24f,length*.47f,light);
        DrawCube((Vector3){0,.42f,-length*.04f},width*.52f,.22f,length*(variant==0?.13f:.23f),accent);
        if(variant==0)for(int side=-1;side<=1;side+=2)DrawCube((Vector3){side*.16f,.29f,-length*.23f},.16f,.10f,.22f,dark);
        DrawCube((Vector3){0,.44f,length*.08f},width*.45f,.13f,.025f,glass);
        DrawCube((Vector3){0,.57f,-length*.03f},width*.59f,.035f,length*.26f,light);
        if(variant){DrawCylinderEx((Vector3){0,.59f,-.3f},(Vector3){0,1.00f+variant*.17f,-.3f},.028f,.022f,6,metal);DrawCube((Vector3){0,.95f+variant*.17f,-.3f},width*.7f,.05f,.06f,light);}
        for(int side=-1;side<=1;side+=2){
            DrawCylinderEx((Vector3){side*width*.44f,.17f,-length*.4f},(Vector3){side*width*.44f,.17f,length*.27f},.015f,.015f,5,light);
            DrawSphereEx((Vector3){side*width*.4f,.18f,length*.24f},.055f,5,7,front);
            DrawCylinderEx((Vector3){side*.14f,-.17f,-length*.40f},(Vector3){side*.14f,-.17f,-length*.55f},.045f,.045f,6,dark);
        }
        DrawCube((Vector3){0,.14f,-length*.46f},width*.3f,.05f,.035f,rear);
    }else if(layer==AW_PATROL_SUB){
        float radius=.31f+variant*.095f,length=1.95f+variant*.68f;
        DrawCylinderEx((Vector3){0,0,-length*.34f},(Vector3){0,0,length*.34f},radius,radius,20,metal);
        DrawSphereEx((Vector3){0,0,-length*.34f},radius,12,20,metal);
        DrawSphereEx((Vector3){0,0,length*.34f},radius,12,20,dark);
        if(variant==2){
            DrawCube((Vector3){0,radius*.86f,-length*.02f},radius*1.2f,.09f,length*.48f,dark);
            for(int j=0;j<4;j++)DrawCube((Vector3){0,radius+.025f,-length*.23f+j*.22f},radius*.94f,.03f,.13f,light);
        }
        if(variant==1)for(int side=-1;side<=1;side+=2)DrawCylinderEx((Vector3){side*radius*.62f,-.12f,-length*.22f},(Vector3){side*radius*.62f,-.12f,length*.12f},.075f,.075f,8,dark);
        DrawCube((Vector3){0,radius+.12f,-.12f},radius*(variant==0?.62f:.82f),.29f,length*(variant==0?.12f:.20f),accent);
        DrawCylinderEx((Vector3){0,radius+.25f,-.12f},(Vector3){0,radius+.55f,-.12f},.023f,.020f,6,dark);
        DrawCylinderEx((Vector3){0,radius+.55f,-.12f},(Vector3){0,radius+.55f,.02f},.023f,.023f,6,front);
        for(int side=-1;side<=1;side+=2){aw_model_wedge((Vector3){side*radius*.9f,0,length*.03f},radius*.75f,.22f,.015f,.035f,accent);}
        DrawCube((Vector3){0,0,-length*.39f},radius*2.65f,.055f,.28f,accent);
        DrawCube((Vector3){0,0,-length*.39f},.055f,radius*2.3f,.27f,accent);
        DrawCylinderEx((Vector3){0,0,-length*.4f},(Vector3){0,0,-length*.57f},.06f,.06f,8,dark);
        for(int i=0;i<5;i++){float a=time*9+i*2*PI/5;DrawCylinderEx((Vector3){0,0,-length*.56f},(Vector3){cosf(a)*radius*.62f,sinf(a)*radius*.62f,-length*.54f},.025f,.06f,5,light);}
        for(int side=-1;side<=1;side+=2)DrawSphereEx((Vector3){side*radius*.78f,radius*.25f,length*.23f},.045f,5,6,front);
    }else if(variant==0){
        DrawCapsule((Vector3){0,0,-.22f},(Vector3){0,0,.26f},.22f,8,12,metal);
        DrawCube((Vector3){0,.14f,-.05f},.29f,.15f,.40f,dark);
        DrawSphereEx((Vector3){0,-.12f,.36f},.10f,8,12,glass);
        for(int side=-1;side<=1;side+=2)DrawSphereEx((Vector3){side*.18f,.05f,.26f},.045f,5,8,front);
        for(int x=-1;x<=1;x+=2)for(int z=-1;z<=1;z+=2){
            Vector3 arm={x*.62f,0,z*.62f};DrawCylinderEx((Vector3){x*.12f,0,z*.10f},arm,.045f,.036f,7,dark);
            DrawCylinder(arm,.085f,.085f,.09f,10,metal);
            float angle=time*(x==z?40:-40);Vector3 a={arm.x+cosf(angle)*.28f,.07f,arm.z+sinf(angle)*.28f},b={arm.x-cosf(angle)*.28f,.07f,arm.z-sinf(angle)*.28f};
            DrawCylinderEx(a,b,.018f,.018f,6,z==1?light:dark);
        }
        for(int side=-1;side<=1;side+=2){DrawCylinderEx((Vector3){side*.20f,-.10f,-.12f},(Vector3){side*.30f,-.28f,-.12f},.025f,.02f,6,metal);DrawCylinderEx((Vector3){side*.30f,-.28f,-.28f},(Vector3){side*.30f,-.28f,.27f},.02f,.02f,6,metal);}
    }else{
        float length=variant==1?2.25f:3.15f,span=variant==1?1.52f:2.24f,radius=variant==1?.19f:.28f;
        DrawCapsule((Vector3){0,0,-length*.30f},(Vector3){0,0,length*.32f},radius,10,14,metal);
        DrawSphereEx((Vector3){0,.13f,length*.28f},radius*.88f,8,12,glass);
        for(int side=-1;side<=1;side+=2){
            float sweep=variant==1?.14f:.02f;
            Vector3 a={side*.14f,.01f,length*.15f},b={side*span,.01f,-length*sweep},c={side*span*.95f,.01f,-length*(sweep+.10f)},d={side*.14f,.01f,-length*.27f};
            DrawTriangle3D(a,b,c,accent);DrawTriangle3D(a,c,d,accent);DrawTriangle3D(c,b,a,accent);DrawTriangle3D(d,c,a,accent);
            DrawCylinderEx((Vector3){side*span*.55f,-.13f,.20f},(Vector3){side*span*.55f,-.13f,-.68f},radius*.60f,radius*.75f,10,dark);
            DrawSphereEx((Vector3){side*span*.55f,-.13f,-.7f},radius*.40f,6,8,rear);
            DrawSphereEx((Vector3){side*span,.035f,-length*.14f},.055f,5,8,side<0?(Color){226,124,105,255}:(Color){140,219,172,255});
            aw_model_wedge((Vector3){side*.34f,.10f,-length*.34f},.4f,.20f,.01f,.04f,accent);
        }
        DrawTriangle3D((Vector3){0,.08f,-length*.27f},(Vector3){0,.58f,-length*.40f},(Vector3){0,.08f,-length*.48f},accent);
        DrawTriangle3D((Vector3){0,.08f,-length*.48f},(Vector3){0,.58f,-length*.40f},(Vector3){0,.08f,-length*.27f},accent);
        DrawSphereEx((Vector3){0,0,length*.43f},.05f,6,8,front);
    }
}
#endif
