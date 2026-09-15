#ifndef ALIENWARS_SENSOR_RAYS_H
#define ALIENWARS_SENSOR_RAYS_H
#include "map.h"
/* World coordinates throughout: two units per tile; y = .75*q - 1.2.
 * No graphics API, heap allocation, RNG or writes to the map during queries. */
typedef struct {float x,y,z;} AwSVec;
static AwSVec aw_sv_add(AwSVec a,AwSVec b){return (AwSVec){a.x+b.x,a.y+b.y,a.z+b.z};}
static AwSVec aw_sv_scale(AwSVec a,float t){return (AwSVec){a.x*t,a.y*t,a.z*t};}
static float aw_sv_dot(AwSVec a,AwSVec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float aw_sv_length(AwSVec a){return sqrtf(aw_sv_dot(a,a));}
static AwSVec aw_sv_normal(AwSVec a){float n=aw_sv_length(a);return aw_sv_scale(a,n>1e-8f?1/n:0);}
enum {AW_HIT_NONE,AW_HIT_TERRAIN,AW_HIT_UNIT,AW_HIT_WATER,AW_HIT_BOUNDARY};
typedef struct {float distance;int kind,entity;} AwSensorHit;
typedef struct {float top[AW_CELLS];} AwRayWorld;
static void aw_ray_world_init(AwRayWorld*r,const AwMap*m){
    for(int z=0;z<AW_SIZE;z++)for(int x=0;x<AW_SIZE;x++){
        float top=0;
        /* Neighbor sockets cover both sides of lattice-boundary roundoff and
         * the bridge interpolation halo. Subtractive caves cannot raise it. */
        for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
            int c=aw_clamp(z+dz,0,63)*64+aw_clamp(x+dx,0,63);
            for(int k=0;k<4;k++)top=fmaxf(top,m->cells[c].q[k]);
            if(m->bridge_bins[c])top=fmaxf(top,m->bridges[m->bridge_bins[c]-1].crown);
        }r->top[z*64+x]=top*.75f-1.2f+.001f;
    }
}
static int aw_sensor_in_world(AwSVec p){return p.x>=-32&&p.x<160&&p.z>=-32&&p.z<160;}
static float aw_ray_density(const AwMap*m,AwSVec p){
    float x=p.x*.5f,z=p.z*.5f,q=(p.y+1.2f)/.75f;
    return x>=0&&x<64&&z>=0&&z<64?aw_density(m,x,q,z):aw_ocean_bed_q(m,x,z)-q;
}
/* Evaluate one Freudenthal tetrahedron from the eight voxel corner values.
 * The x/y/z ordering is identical to caves.h and the marching-tetrahedra mesh. */
static float aw_ray_voxel_value(const float v[8],const float f[3]){
    int a[3]={0,1,2};
    for(int i=0;i<2;i++)for(int j=i+1;j<3;j++)if(f[a[j]]>f[a[i]]){int t=a[i];a[i]=a[j];a[j]=t;}
    int c1=1<<a[0],c2=c1|(1<<a[1]);
    return v[0]*(1-f[a[0]])+v[c1]*(f[a[0]]-f[a[1]])+v[c2]*(f[a[1]]-f[a[2]])+v[7]*f[a[2]];
}
/* Exact first entry into the meshed solid, up to float roundoff. DDA walks
 * lattice voxels; x=y, x=z and y=z split each ray interval at tet boundaries.
 * Density is linear on every resulting interval, so roots need no raymarch
 * step heuristic and thin roofs / bridge decks cannot be skipped. */
static float aw_ray_lattice(const AwMap*m,AwSVec origin,AwSVec dir,float start,float end){
    float o[3]={origin.x*3,(origin.y+1.2f)*(8.0f/3),origin.z*3};
    float d[3]={dir.x*3,dir.y*(8.0f/3),dir.z*3};
    float t=start;
    while(t<end-1e-6f){
        int cell[3];float exit=end;
        for(int k=0;k<3;k++){
            float c=o[k]+d[k]*t,nearest=roundf(c);
            if(fabsf(c-nearest)<.0001f)c=nearest;
            cell[k]=(int)floorf(c);if(d[k]<0&&c==floorf(c))cell[k]--;
            if(fabsf(d[k])>1e-8f){float next=((cell[k]+(d[k]>0))-o[k])/d[k];if(next>t+1e-6f)exit=fminf(exit,next);}
        }
        if(exit<=t+1e-6f)break;
        float values[8];
        for(int c=0;c<8;c++){
            float x=(cell[0]+(c&1))/6.0f,q=(cell[1]+((c>>1)&1))*.5f,z=(cell[2]+((c>>2)&1))/6.0f;
            values[c]=aw_density_at_height(m,x,q,z,aw_height_q(m,x,z));
        }
        float cuts[5]={t,exit};int count=2;
        for(int a=0;a<3;a++)for(int b=a+1;b<3;b++)if(fabsf(d[a]-d[b])>1e-8f){
            float c=(cell[a]-cell[b]-o[a]+o[b])/(d[a]-d[b]);
            if(c>t+1e-6f&&c<exit-1e-6f)cuts[count++]=c;
        }
        for(int a=1;a<count;a++){float c=cuts[a];int b=a;while(b&&cuts[b-1]>c){cuts[b]=cuts[b-1];b--;}cuts[b]=c;}
        float last=0;
        for(int i=0;i<count;i++){
            float f[3];for(int k=0;k<3;k++)f[k]=fminf(1,fmaxf(0,o[k]+d[k]*cuts[i]-cell[k]));
            float value=aw_ray_voxel_value(values,f);
            if(value>1e-5f){
                if(!i||last>=0)return cuts[i?i-1:0];
                return cuts[i-1]+(cuts[i]-cuts[i-1])*(-last)/(value-last);
            }last=value;
        }t=exit;
    }return end;
}
/* Outside the land, the rendered shelf is the max of four sloped extents,
 * clamped to a flat deep bed. Intersect its five candidate planes analytically. */
static float aw_ray_shelf(const AwMap*m,AwSVec o,AwSVec d,float a,float b){
    AwSVec p=aw_sv_add(o,aw_sv_scale(d,a));if(aw_ray_density(m,p)>1e-5f)return a;
    const float nx[5]={1,-1,0,0,0},nz[5]={0,0,1,-1,0},k[5]={0,128,0,128,-12};
    float best=b,q0=(o.y+1.2f)/.75f,dq=d.y/.75f;
    for(int i=0;i<5;i++){
        float denominator=dq-nx[i]*d.x-nz[i]*d.z;
        if(fabsf(denominator)<1e-8f)continue;
        float t=(nx[i]*o.x+nz[i]*o.z+k[i]-q0)/denominator;
        if(t<a-1e-5f||t>=best)continue;
        AwSVec hit=aw_sv_add(o,aw_sv_scale(d,t));
        if(fabsf(aw_ray_density(m,hit))<.0002f&&aw_ray_density(m,aw_sv_add(hit,aw_sv_scale(d,.0005f)))>0)best=fmaxf(a,t);
    }return best;
}
static AwSensorHit aw_ray_terrain(const AwMap*m,const AwRayWorld*r,AwSVec o,AwSVec d,float range,int water){
    AwSensorHit hit={range,AW_HIT_NONE,-1};
    if(!aw_sensor_in_world(o))return (AwSensorHit){0,AW_HIT_BOUNDARY,-1};
    if(aw_ray_density(m,o)>1e-5f)return (AwSensorHit){0,AW_HIT_TERRAIN,-1};
    float exit=range;
    if(d.x>1e-8f)exit=fminf(exit,(160-o.x)/d.x);else if(d.x< -1e-8f)exit=fminf(exit,(-32-o.x)/d.x);
    if(d.z>1e-8f)exit=fminf(exit,(160-o.z)/d.z);else if(d.z< -1e-8f)exit=fminf(exit,(-32-o.z)/d.z);
    if(exit<range)hit=(AwSensorHit){exit,AW_HIT_BOUNDARY,-1};
    if(water&&fabsf(d.y)>1e-8f){
        float t=(-.12f-o.y)/d.y;
        if(t>=0&&t<exit){AwSVec p=aw_sv_add(o,aw_sv_scale(d,t));
            if(aw_ocean_bed_q(m,p.x*.5f,p.z*.5f)<1.44f){exit=t;hit=(AwSensorHit){t,AW_HIT_WATER,-1};}}
    }
    float t=0;
    while(t<exit-1e-6f){
        AwSVec p=aw_sv_add(o,aw_sv_scale(d,t));
        float x=p.x*.5f,z=p.z*.5f;
        if(fabsf(x-roundf(x))<.00002f)x=roundf(x);if(fabsf(z-roundf(z))<.00002f)z=roundf(z);
        int cx=(int)floorf(x),cz=(int)floorf(z);
        if(d.x<0&&x==floorf(x))cx--;if(d.z<0&&z==floorf(z))cz--;
        float next=exit;
        if(fabsf(d.x)>1e-8f){float nt=(2*(cx+(d.x>0))-o.x)/d.x;if(nt>t+1e-6f)next=fminf(next,nt);}
        if(fabsf(d.z)>1e-8f){float nt=(2*(cz+(d.z>0))-o.z)/d.z;if(nt>t+1e-6f)next=fminf(next,nt);}
        float found=next;
        if(cx>=0&&cx<64&&cz>=0&&cz<64){
            if(fminf(o.y+d.y*t,o.y+d.y*next)<=r->top[cz*64+cx])found=aw_ray_lattice(m,o,d,t,next);
        }else found=aw_ray_shelf(m,o,d,t,next);
        if(found<next-1e-6f)return (AwSensorHit){found,AW_HIT_TERRAIN,-1};
        t=next;
    }return hit;
}
#endif
