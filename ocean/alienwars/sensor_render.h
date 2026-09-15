#ifndef ALIENWARS_SENSOR_RENDER_H
#define ALIENWARS_SENSOR_RENDER_H
#include "sensors.h"
/* Presentation consumes cached returns only. It cannot trigger a query, alter
 * a mount, move a unit, or change authoritative sensor timing. */
static Vector3 aw_sensor_v3(AwSVec v){return (Vector3){v.x,v.y,v.z};}
static Color aw_sensor_color(int type,float alpha){
    static const Color colors[4]={{104,230,180,255},{88,184,245,255},{192,158,247,255},{242,194,107,255}};
    Color c=colors[type];c.a=(unsigned char)(255*fminf(1,fmaxf(0,alpha)));return c;
}
static Vector3 aw_sensor_endpoint(const AwSensorReading*r,int i,float range){
    return aw_sensor_v3(aw_sv_add(r->pose.position,aw_sv_scale(r->beams[i].direction,range)));
}
static void aw_sensor_ring(Vector3 center,float radius,Color color,int dashed){
    for(int i=0;i<96;i++){
        if(dashed&&i%3==2)continue;float a=i*(2*PI/96),b=(i+1)*(2*PI/96);
        DrawLine3D((Vector3){center.x+sinf(a)*radius,center.y,center.z+cosf(a)*radius},
                   (Vector3){center.x+sinf(b)*radius,center.y,center.z+cosf(b)*radius},color);
    }
}
static void aw_sensor_draw_module(const AwSensors*s,int id,int type,float time,float opacity,int selected){
    const AwSensorUnit*u=&s->units[id];const AwSensorConfig*c=&u->config[type];const AwSensorReading*r=&u->reading[type];
    if(!u->active||!c->enabled||!r->valid)return;
    Vector3 origin=aw_sensor_v3(r->pose.position);
    if(type==AW_SENSOR_RF){
        aw_sensor_ring(origin,c->range,aw_sensor_color(type,.30f*opacity),1);
        float pulse=fmodf(time*.18f,1);aw_sensor_ring(origin,c->range*pulse,aw_sensor_color(type,.28f*(1-pulse)*opacity),0);
        for(int j=0;j<s->count;j++)if(r->peers[j].detected){
            /* Cached RF bearing/range forms the link; do not reveal
             * a peer's live position after the last radio sample. */
            float a=r->peers[j].bearing+r->pose.yaw,range=r->peers[j].distance,e=r->peers[j].elevation;
            Vector3 end={origin.x+sinf(a)*cosf(e)*range,origin.y+sinf(e)*range,origin.z+cosf(a)*cosf(e)*range};
            for(int k=0;k<12;k+=2)DrawLine3D(Vector3Lerp(origin,end,k/12.0f),Vector3Lerp(origin,end,(k+1)/12.0f),aw_sensor_color(type,opacity*(.35f+.5f*r->peers[j].strength)));
            aw_sensor_ring(end,.65f,aw_sensor_color(type,.9f*opacity),0);
        }return;
    }
    if(type==AW_SENSOR_LIDAR){
        /* Dashed outer ring is instrument range, not guaranteed visibility. */
        for(int i=0;i<32;i++){
            Vector3 a=aw_sensor_endpoint(r,i,c->range),b=aw_sensor_endpoint(r,(i+1)%32,c->range);
            DrawLine3D(a,Vector3Lerp(a,b,.55f),aw_sensor_color(type,.28f*opacity));
            Vector3 ea=aw_sensor_endpoint(r,i,r->beams[i].hit.distance),eb=aw_sensor_endpoint(r,(i+1)%32,r->beams[(i+1)%32].hit.distance);
            if(fabsf(r->beams[i].hit.distance-r->beams[(i+1)%32].hit.distance)<c->range*.22f){
                DrawTriangle3D(origin,ea,eb,aw_sensor_color(type,.035f*opacity));DrawLine3D(ea,eb,aw_sensor_color(type,.42f*opacity));
            }
        }
    }
    if(type==AW_SENSOR_SONAR){
        for(int i=0;i<7;i++)for(int j=0;j<2;j++){
            int a=i*3+j,b=(i+1)*3+j;Vector3 p=aw_sensor_endpoint(r,a,r->beams[a].hit.distance),q=aw_sensor_endpoint(r,b,r->beams[b].hit.distance);
            DrawLine3D(p,q,aw_sensor_color(type,.38f*opacity));
            if(j==1)DrawTriangle3D(origin,p,q,aw_sensor_color(type,.05f*opacity));
        }
        for(int i=0;i<24;i+=3)DrawLine3D(aw_sensor_endpoint(r,i,c->range),aw_sensor_endpoint(r,i+2,c->range),aw_sensor_color(type,.22f*opacity));
    }
    if(type==AW_SENSOR_CAMERA){
        int corners[4]={0,7,47,40};
        for(int k=0;k<4;k++){
            Vector3 a=aw_sensor_endpoint(r,corners[k],c->range),b=aw_sensor_endpoint(r,corners[(k+1)%4],c->range);
            DrawLine3D(origin,a,aw_sensor_color(type,.33f*opacity));DrawLine3D(a,b,aw_sensor_color(type,.25f*opacity));
        }
    }
    int sweep=(int)(time*12)%r->count;
    for(int i=0;i<r->count;i++){
        const AwSensorBeam*b=&r->beams[i];Vector3 end=aw_sensor_endpoint(r,i,b->hit.distance);
        float alpha=i==sweep?.85f:type==AW_SENSOR_CAMERA?.12f:.23f;
        DrawLine3D(origin,end,aw_sensor_color(type,alpha*opacity));
        if(b->hit.kind!=AW_HIT_NONE&&b->hit.kind!=AW_HIT_BOUNDARY){
            DrawSphereEx(end,selected?.13f:.095f,3,5,aw_sensor_color(type,opacity*(b->hit.kind==AW_HIT_UNIT?1:.8f)));
        }
    }
    DrawSphereEx(origin,.15f,4,6,aw_sensor_color(type,opacity));
}
static void aw_sensors_draw(const AwSensors*s,int selected,int mode,int all,int xray,float time,int isolation,int patrol_hidden){
    if(!mode)return;
    rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();
    /* A faint underlay keeps bathymetry and underground returns legible through
     * water/rock. It never changes the terrain material or its shadow pass. */
    for(int pass=xray?0:1;pass<2;pass++){
        if(pass==0)rlDisableDepthTest();else rlEnableDepthTest();
        for(int id=0;id<s->count;id++){
            if(!all&&id!=selected)continue;if(id&&(isolation||patrol_hidden))continue;
            float opacity=(pass==0?.65f:.90f)*(id==selected?1:.42f);
            for(int t=0;t<4;t++)if(mode&(1<<t))aw_sensor_draw_module(s,id,t,time,opacity,id==selected);
        }rlDrawRenderBatchActive();
    }
    rlEnableDepthTest();rlEnableBackfaceCulling();rlEnableDepthMask();
}
#endif
