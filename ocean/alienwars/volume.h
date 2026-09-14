#ifndef ALIENWARS_VOLUME_H
#define ALIENWARS_VOLUME_H
#include "map.h"
/* Fixed-resolution marching tetrahedra on a shared lattice. Every cube uses the
 * same body diagonal; neighboring faces have identical triangulations. No case
 * ambiguity, roof adapters, independently closed tiles, or level-of-detail seams.
 * q spacing .5, x/z spacing 1/6 tile. Coordinates remain in map units here. */
typedef struct {float x,q,z;} AwVolumePoint;
typedef void (*AwVolumeTriangle)(void*,AwVolumePoint,AwVolumePoint,AwVolumePoint);
static AwVolumePoint aw_volume_lerp(AwVolumePoint a,AwVolumePoint b,float va,float vb){
    float t=va/(va-vb);return (AwVolumePoint){aw_lerp(a.x,b.x,t),aw_lerp(a.q,b.q,t),aw_lerp(a.z,b.z,t)};
}
static void aw_volume_emit(AwVolumeTriangle emit,void*ctx,AwVolumePoint a,AwVolumePoint b,AwVolumePoint c,AwVolumePoint outward){
    float bx=b.x-a.x,by=b.q-a.q,bz=b.z-a.z,cx=c.x-a.x,cy=c.q-a.q,cz=c.z-a.z;
    float nx=by*cz-bz*cy,ny=bz*cx-bx*cz,nz=bx*cy-by*cx;
    if(nx*nx+ny*ny+nz*nz<1e-16f)return;
    if(nx*outward.x+ny*outward.q+nz*outward.z<0){AwVolumePoint t=b;b=c;c=t;}
    emit(ctx,a,b,c);
}
static void aw_tetrahedron(AwVolumePoint*p,float*v,AwVolumeTriangle emit,void*ctx){
    int solid[4],air[4],ns=0,na=0;
    for(int i=0;i<4;i++){if(v[i]>0)solid[ns++]=i;else air[na++]=i;}
    if(!ns||!na)return;
    AwVolumePoint direction={p[air[0]].x-p[solid[0]].x,p[air[0]].q-p[solid[0]].q,p[air[0]].z-p[solid[0]].z};
    if(ns==1||na==1){
        int a=ns==1?solid[0]:air[0];int *other=ns==1?air:solid;
        AwVolumePoint t[3];for(int i=0;i<3;i++){int b=other[i];t[i]=aw_volume_lerp(p[a],p[b],v[a],v[b]);}
        aw_volume_emit(emit,ctx,t[0],t[1],t[2],direction);
    }else{
        AwVolumePoint t[4];for(int i=0;i<2;i++)for(int j=0;j<2;j++){int a=solid[i],b=air[j];t[i*2+j]=aw_volume_lerp(p[a],p[b],v[a],v[b]);}
        aw_volume_emit(emit,ctx,t[0],t[1],t[3],direction);aw_volume_emit(emit,ctx,t[0],t[3],t[2],direction);
    }
}
static void aw_volume_cell(const AwMap*m,int cell,AwVolumeTriangle emit,void*ctx){
    static const int tet[6][4]={{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6},{0,5,1,6}};
    static const int dx[4]={0,1,1,0},dz[4]={0,0,1,1};
    int cx=cell%AW_SIZE,cz=cell/AW_SIZE;float bottom=1000;
    for(int i=0;i<m->cave_bin_count[cell];i++){
        const AwCaveEdge*e=&m->cave_edges[m->cave_bins[cell][i]];
        bottom=fminf(bottom,fminf(m->cave[e->a].q,m->cave[e->b].q)-1);
    }
    for(int z=0;z<AW_SUBDIV;z++)for(int x=0;x<AW_SUBDIV;x++){
        float h[4],lo=1000,hi=-1000;AwVolumePoint p[8];float v[8];
        for(int k=0;k<4;k++){
            /* Integer lattice indices avoid tile-dependent rounding at seams. */
            float gx=(float)(cx*AW_SUBDIV+x+dx[k])/AW_SUBDIV,gz=(float)(cz*AW_SUBDIV+z+dz[k])/AW_SUBDIV;
            h[k]=aw_height_q(m,gx,gz);lo=fminf(lo,h[k]);hi=fmaxf(hi,h[k]);p[k]=(AwVolumePoint){gx,0,gz};p[k+4]=p[k];
        }
        lo=fminf(lo,bottom);int q0=(int)floorf(lo*2)-1,q1=(int)ceilf(hi*2)+1;
        for(int k=0;k<4;k++){p[k].q=q0*0.5f;v[k]=aw_density_at_height(m,p[k].x,p[k].q,p[k].z,h[k]);}
        for(int q=q0;q<q1;q++){
            int mask=0;for(int k=0;k<4;k++){p[k+4].q=(q+1)*0.5f;v[k+4]=aw_density_at_height(m,p[k+4].x,p[k+4].q,p[k+4].z,h[k]);if(v[k]>0)mask|=1<<k;if(v[k+4]>0)mask|=1<<(k+4);}
            if(mask&&mask!=255)for(int t=0;t<6;t++){
                AwVolumePoint points[4];float values[4];for(int k=0;k<4;k++){points[k]=p[tet[t][k]];values[k]=v[tet[t][k]];}
                aw_tetrahedron(points,values,emit,ctx);
            }
            for(int k=0;k<4;k++){p[k]=p[k+4];v[k]=v[k+4];}
        }
    }
}
#endif
