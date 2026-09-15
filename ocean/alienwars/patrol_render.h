#ifndef ALIENWARS_PATROL_RENDER_H
#define ALIENWARS_PATROL_RENDER_H
#include "patrols.h"
#include "motion.h"
/* Compact procedural silhouettes, scaled to tile-sized actors. Unit rendering
 * stays distinct from the subdued terrain material and its baked lighting. */
static void aw_patrol_model(int layer,int variant,float time){
    Color dark={38,48,52,255},metal={112,131,137,255},accent=layer==AW_PATROL_GROUND?(Color){220,167,77,255}:layer==AW_PATROL_NAVAL?(Color){81,190,205,255}:(Color){199,217,226,255};
    if(layer==AW_PATROL_GROUND){
        float length=variant==1?1.6f:2.25f;
        DrawCube((Vector3){0,.38f,0},1.0f,.42f,length,metal);
        if(variant==1){
            for(int side=-1;side<=1;side+=2)DrawCube((Vector3){side*.60f,.22f,0},.28f,.40f,length+.12f,dark);
            DrawCylinder((Vector3){0,.60f,-.12f},.38f,.42f,.20f,10,accent);
            DrawCylinderEx((Vector3){0,.76f,-.12f},(Vector3){0,.76f,.90f},.09f,.07f,6,dark);
        }else{
            DrawCube((Vector3){0,.76f,.66f},.94f,.48f,.64f,accent);
            DrawCube((Vector3){0,.67f,-.4f},.94f,.44f,1.2f,dark);
            for(int side=-1;side<=1;side+=2)for(int w=-1;w<=1;w++)DrawSphereEx((Vector3){side*.62f,.24f,w*.74f},.25f,5,8,dark);
            DrawCube((Vector3){0,.81f,1.0f},.70f,.16f,.025f,(Color){60,132,149,255});
        }
        for(int side=-1;side<=1;side+=2)DrawCube((Vector3){side*.36f,.48f,length*.51f},.16f,.09f,.035f,(Color){246,230,159,255});
    }else if(layer==AW_PATROL_NAVAL){
        float width=.67f+variant*.30f,length=1.50f+variant*.85f;
        Vector3 bow={0,.04f,length*.62f},a={-width*.5f,.04f,length*.30f},b={width*.5f,.04f,length*.30f};
        DrawCube((Vector3){0,-.02f,-length*.10f},width,.23f,length*.80f,dark);
        Vector3 lowA={a.x,-.19f,a.z},lowB={b.x,-.19f,b.z},tip={0,-.19f,length*.52f};
        DrawTriangle3D(b,a,bow,accent);DrawTriangle3D(lowA,lowB,tip,dark);
        DrawTriangle3D(a,lowA,tip,dark);DrawTriangle3D(a,tip,bow,accent);
        DrawTriangle3D(b,bow,tip,accent);DrawTriangle3D(b,tip,lowB,dark);
        DrawCube((Vector3){0,.15f,-length*.12f},width*.66f,.25f,length*.47f,metal);
        DrawCube((Vector3){0,.32f,-length*.06f},width*.49f,.16f,length*.24f,accent);
        if(variant){DrawCylinderEx((Vector3){0,.38f,-.25f},(Vector3){0,.75f+variant*.16f,-.25f},.026f,.018f,5,metal);}
        if(variant==2)for(int i=-1;i<=1;i++)DrawCube((Vector3){0,.23f,-.65f+i*.27f},width*.7f,.18f,.18f,(Color){84,117,127,255});
        for(int side=-1;side<=1;side+=2)DrawLine3D((Vector3){side*width*.4f,.04f,-length*.55f},(Vector3){side*width*.8f,.04f,-length*1.1f},(Color){153,199,204,100});
    }else{
        if(variant==0){
            DrawSphereEx((Vector3){0,0,0},.32f,6,10,accent);
            for(int x=-1;x<=1;x+=2)for(int z=-1;z<=1;z+=2){
                Vector3 arm={x*.63f,0,z*.63f};DrawCylinderEx((Vector3){0,0,0},arm,.065f,.055f,5,dark);
                DrawCylinder(arm,.25f,.25f,.035f,12,metal);
                Vector3 rotor={arm.x+sinf(time*45)*.28f,.09f,arm.z+cosf(time*45)*.28f};
                DrawLine3D((Vector3){2*arm.x-rotor.x,.09f,2*arm.z-rotor.z},rotor,accent);
            }
        }else{
            float length=variant==1?1.8f:2.6f,width=variant==1?1.5f:2.3f;
            DrawCube((Vector3){0,0,0},.44f,.30f,length,metal);
            DrawSphereEx((Vector3){0,.11f,length*.28f},.25f,5,8,(Color){72,138,163,255});
            for(int side=-1;side<=1;side+=2){
                Vector3 a={side*.2f,.02f,length*.22f},b={side*width,.02f,-length*.22f},c={side*.2f,.02f,-length*.32f};
                DrawTriangle3D(a,b,c,accent);DrawTriangle3D(c,b,a,accent);
                DrawCylinderEx((Vector3){side*width*.66f,-.08f,.30f},(Vector3){side*width*.66f,-.08f,-.60f},.14f,.19f,8,dark);
                DrawSphereEx((Vector3){side*width*.66f,-.08f,-.64f},.095f,4,6,(Color){91,206,229,255});
            }
            DrawCube((Vector3){0,.25f,-length*.42f},.055f,.4f,.4f,accent);
        }
    }
}
static void aw_patrol_draw(const AwMap*m,const AwPatrols*f,const AwMotion*motion,float time){
    for(int i=0;i<AW_PATROLS;i++){
        const AwPatrol*p=&f->units[i];if(p->count<2)continue;
        AwPatrolPoint a=aw_patrol_position(m,p,p->progress);
        Vector3 position={a.x*AW_UNIT,aw_y(a.q/4),a.z*AW_UNIT};
        if(p->layer==AW_PATROL_NAVAL)position.y+=sinf(time*1.7f+i)*.018f;
        if(p->layer==AW_PATROL_AIR){
            float ground=aw_y(aw_patrol_roof(m,a.x,a.z)/4)+.04f;
            DrawCylinder((Vector3){position.x,ground,position.z},.38f,.38f,.005f,12,(Color){20,30,38,60});
        }
        rlPushMatrix();rlTranslatef(position.x,position.y,position.z);
        rlRotatef(motion[i].yaw*RAD2DEG,0,1,0);
        rlRotatef(-motion[i].pitch*RAD2DEG,1,0,0);
        aw_patrol_model(p->layer,p->variant,time);rlPopMatrix();
    }
}
#endif
