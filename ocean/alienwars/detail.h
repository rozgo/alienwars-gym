#ifndef ALIENWARS_DETAIL_H
#define ALIENWARS_DETAIL_H
#include "map.h"
/* Periodic, cosmetic height textures. Four independent channels:
 * R granular soil, G fractured stone, B grass/litter, A fine snow crust.
 * Coordinates are normalized texture space; integer translation is seamless.
 * Never advances world RNG or changes terrain geometry. */
static float aw_detail_sat(float x){return fminf(1,fmaxf(0,x));}
static float aw_detail_hash(int x,int y,int period,uint32_t salt){
    x=(x%period+period)%period;y=(y%period+period)%period;
    return (aw_hash((uint32_t)x*8191u^(uint32_t)y*131071u^salt)&65535)/65535.0f;
}
static float aw_detail_noise(float u,float v,int period,uint32_t salt){
    float px=u*period,py=v*period;int x=(int)floorf(px),y=(int)floorf(py);
    float fx=px-x,fy=py-y;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
    return aw_bilinear(aw_detail_hash(x,y,period,salt),aw_detail_hash(x+1,y,period,salt),
        aw_detail_hash(x+1,y+1,period,salt),aw_detail_hash(x,y+1,period,salt),fx,fy);
}
static void aw_detail_sample(float u,float v,float out[4]){
    float broad=aw_detail_noise(u,v,8,371),grain=aw_detail_noise(u,v,64,7919);
    float fine=aw_detail_noise(u,v,173,1337);
    float px=u*24,py=v*24;int ix=(int)floorf(px),iy=(int)floorf(py);
    float nearest=100,second=100,blade=0;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
        int x=ix+dx,y=iy+dy;
        float ox=px-x-.15f-.7f*aw_detail_hash(x,y,24,9323),oy=py-y-.15f-.7f*aw_detail_hash(x,y,24,11717);
        float dist=sqrtf(ox*ox+oy*oy);
        if(dist<nearest){second=nearest;nearest=dist;}else second=fminf(second,dist);
        float angle=aw_detail_hash(x,y,24,7753)*6.2831853f;
        float along=ox*cosf(angle)+oy*sinf(angle),across=-ox*sinf(angle)+oy*cosf(angle);
        float length=.17f+.28f*aw_detail_hash(x,y,24,1921);
        float stroke=aw_detail_sat(1-fabsf(across)/.065f)*aw_detail_sat(1-fabsf(along)/length);
        blade=fmaxf(blade,stroke);
    }
    float pebble=aw_detail_sat(1-nearest/.29f);pebble=pebble*pebble*(3-2*pebble);
    float seam=1-aw_detail_sat((second-nearest)/.13f);
    float strata=aw_detail_noise(u,v,16,7231);
    out[0]=aw_detail_sat(.25f+.24f*broad+.24f*grain+.12f*fine+.25f*pebble);
    out[1]=aw_detail_sat(.36f+.28f*strata+.17f*grain+.08f*fine-.27f*seam*seam);
    out[2]=aw_detail_sat(.24f+.22f*broad+.19f*grain+.09f*fine+.30f*blade);
    out[3]=aw_detail_sat(.34f+.23f*broad+.12f*grain+.08f*fine);
}
/* Shared-corner material weights exactly follow the palette's neighborhood.
 * R grass/forest, G granular ground, B snow/ice, A roads. Residual is rock. */
static void aw_detail_weights(const AwMap*m,int vx,int vz,float out[4]){
    for(int k=0;k<4;k++)out[k]=0;int count=0;
    for(int dz=-1;dz<=0;dz++)for(int dx=-1;dx<=0;dx++){
        int x=vx+dx,z=vz+dz;if(x<0||z<0||x>=AW_SIZE||z>=AW_SIZE)continue;
        int mat=m->cells[z*AW_SIZE+x].material;count++;
        if(mat==AW_GRASS||mat==AW_FOREST)out[0]++;
        if(mat==AW_DIRT||mat==AW_SAND||mat==AW_MUD||mat==AW_SHALLOW||mat==AW_DEEP)out[1]++;
        if(mat==AW_SNOW||mat==AW_ICE)out[2]++;
        if(mat==AW_ROAD)out[3]++;
    }
    if(count)for(int k=0;k<4;k++)out[k]/=count;
}
#endif
