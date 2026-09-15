#ifndef ALIENWARS_OCCLUSION_H
#define ALIENWARS_OCCLUSION_H
#include "map.h"
/* Static, cosmetic accessibility bake. World metres: x/z = tiles * 2,
 * y = q * .75 - 1.2. No random stream or authoritative map writes.
 * Horizon sampling handles the open landscape; short hemisphere rays sample
 * the SAME tetrahedral solid as the cave mesher (not a distance-field march).
 * Direct-mapped caches are bounded and disposable after the bake. */
#define AW_AO_GRID (AW_SIZE*AW_SUBDIV+1)
#define AW_AO_CACHE (1<<18)
#define AW_AO_CONTACTS (AW_CELLS+16)
typedef struct {int x,q,z,valid;float value;} AwAoDensity;
typedef struct {int key[6],valid;float value;} AwAoSample;
typedef struct {float x,y,z,rx,rz,height,strength;int next;} AwAoContact;
typedef struct {
    const AwMap *map;
    float *height;
    AwAoDensity *density;
    AwAoSample *samples;
    AwAoContact contacts[AW_AO_CONTACTS];
    int heads[AW_CELLS],contact_count;
    unsigned samples_baked,cave_samples;
} AwOcclusion;
static float aw_ao_clamp(float a,float lo,float hi){return fminf(hi,fmaxf(lo,a));}
static void *aw_ao_alloc(size_t count,size_t size){
    void*p=calloc(count,size);if(!p){fprintf(stderr,"Occlusion bake allocation failed\n");exit(2);}return p;
}
static void aw_ao_init(AwOcclusion*a,const AwMap*m){
    memset(a,0,sizeof(*a));a->map=m;
    a->height=aw_ao_alloc(AW_AO_GRID*AW_AO_GRID,sizeof(float));
    a->density=aw_ao_alloc(AW_AO_CACHE,sizeof(AwAoDensity));
    a->samples=aw_ao_alloc(AW_AO_CACHE,sizeof(AwAoSample));
    for(int i=0;i<AW_CELLS;i++)a->heads[i]=-1;
    for(int z=0;z<AW_AO_GRID;z++)for(int x=0;x<AW_AO_GRID;x++)
        a->height[z*AW_AO_GRID+x]=aw_height_q(m,(float)x/AW_SUBDIV,(float)z/AW_SUBDIV);
}
static void aw_ao_free(AwOcclusion*a){free(a->height);free(a->density);free(a->samples);memset(a,0,sizeof(*a));}
static float aw_ao_height(const AwOcclusion*a,float x,float z){
    float gx=aw_ao_clamp(x*.5f*AW_SUBDIV,0,AW_AO_GRID-1.0001f),gz=aw_ao_clamp(z*.5f*AW_SUBDIV,0,AW_AO_GRID-1.0001f);
    int ix=(int)gx,iz=(int)gz;float fx=gx-ix,fz=gz-iz;const float*h=a->height+iz*AW_AO_GRID+ix;
    return aw_bilinear(h[0],h[1],h[AW_AO_GRID+1],h[AW_AO_GRID],fx,fz)*.75f-1.2f;
}
static const float aw_ao_dx[8]={1,.70710678f,0,-.70710678f,-1,-.70710678f,0,.70710678f};
static const float aw_ao_dz[8]={0,.70710678f,1,.70710678f,0,-.70710678f,-1,-.70710678f};
/* Horizon elevation at the receiver's actual height, integrated with its
 * normal. Thus cliff feet and slopes differ from flat, exposed plateau tops;
 * a single x/z shadow is never projected through multiple cave floors. */
static float aw_ao_horizon(const AwOcclusion*a,float x,float y,float z,float nx,float ny,float nz){
    static const float radius[8]={.6f,1.2f,2.2f,3.8f,6,9,13,18};
    float blocked=0,total=0;
    for(int d=0;d<8;d++){
        float slope=-1000;
        for(int j=0;j<8;j++){
            float r=radius[j],px=x+aw_ao_dx[d]*r,pz=z+aw_ao_dz[d]*r;
            if(px<0||pz<0||px>128||pz>128)continue;
            float dh=aw_ao_height(a,px,pz)-y-.08f;
            /* Broad occlusion fades with range; local self-intersection bias
             * is smaller than a terrain lattice edge (.333 world units). */
            slope=fmaxf(slope,dh/r-(r/18)*.18f);
        }
        for(int v=0;v<8;v++){
            float dy=-.875f+v*.25f,h=sqrtf(1-dy*dy);
            float weight=fmaxf(0,nx*aw_ao_dx[d]*h+ny*dy+nz*aw_ao_dz[d]*h);
            float cover=aw_ao_clamp((slope-dy/h)*2.5f+.5f,0,1);
            total+=weight;blocked+=weight*cover;
        }
    }
    return total>0?aw_ao_clamp(blocked/total,0,1):0;
}
static float aw_ao_lattice(AwOcclusion*a,int x,int q,int z){
    uint32_t h=aw_hash((uint32_t)x*73856093u^(uint32_t)q*19349663u^(uint32_t)z*83492791u)&(AW_AO_CACHE-1);
    AwAoDensity*c=&a->density[h];
    if(!c->valid||c->x!=x||c->q!=q||c->z!=z){
        c->x=x;c->q=q;c->z=z;c->valid=1;
        c->value=aw_density_at_height(a->map,(float)x/AW_SUBDIV,q*.5f,(float)z/AW_SUBDIV,a->height[z*AW_AO_GRID+x]);
    }return c->value;
}
/* Same Freudenthal interpolation as aw_density; caching only replaces its
 * repeated lattice evaluations. Points outside the terrain are open air. */
static float aw_ao_density(AwOcclusion*a,float x,float y,float z){
    if(x<0||z<0||x>=128||z>=128)return -1000;
    float coord[3]={x*.5f*AW_SUBDIV,(y+1.2f)/.75f*2,z*.5f*AW_SUBDIV},f[3];int p[3],axis[3]={0,1,2};
    for(int i=0;i<3;i++){p[i]=(int)floorf(coord[i]);f[i]=coord[i]-p[i];}
    for(int i=0;i<2;i++)for(int j=i+1;j<3;j++)if(f[axis[j]]>f[axis[i]]){int t=axis[i];axis[i]=axis[j];axis[j]=t;}
    float v[4];for(int i=0;i<4;i++){v[i]=aw_ao_lattice(a,p[0],p[1],p[2]);if(i<3)p[axis[i]]++;}
    float u=f[axis[0]],w=f[axis[1]],t=f[axis[2]];
    return v[0]*(1-u)+v[1]*(u-w)+v[2]*(w-t)+v[3]*t;
}
static float aw_ao_cavity(AwOcclusion*a,float x,float y,float z,float nx,float ny,float nz){
    float blocked=0,total=0;
    /* Uniform spherical strata, weighted by cosine to the surface. Paired
     * azimuths preserve 180-degree symmetry; no random per-tile ray rotations. */
    for(int d=0;d<8;d++)for(int v=0;v<4;v++){
        float dy=-.75f+v*.5f,h=sqrtf(1-dy*dy),dx=aw_ao_dx[d]*h,dz=aw_ao_dz[d]*h;
        float weight=fmaxf(0,nx*dx+ny*dy+nz*dz);if(weight<.001f)continue;
        total+=weight;
        /* Fixed .25-unit steps are below the .333/.375 lattice spacing.
         * This is finite-sample AO, not collision or guaranteed ray traversal. */
        for(int j=1;j<=24;j++){
            float t=j*.25f;
            if(aw_ao_density(a,x+nx*.10f+dx*t,y+ny*.10f+dy*t,z+nz*.10f+dz*t)>.015f){
                blocked+=weight/(1+t*.22f);break;
            }
        }
    }
    return total>0?aw_ao_clamp(blocked/total,0,1):0;
}
/* Analytic, soft contact footprints for cosmetic props, indexed by tile.
 * Vertical falloff prevents a prop on a shelf from staining a lower floor.
 * Elliptical radii follow each placed tree root, boulder or outpost footprint. */
static void aw_ao_contact_add(AwOcclusion*a,float x,float y,float z,float rx,float rz,float height,float strength){
    if(a->contact_count>=AW_AO_CONTACTS){fprintf(stderr,"Occlusion contact capacity exceeded\n");exit(2);}
    int c=aw_clamp((int)(z*.5f),0,63)*64+aw_clamp((int)(x*.5f),0,63),i=a->contact_count++;
    a->contacts[i]=(AwAoContact){x,y,z,rx,rz,height,strength,a->heads[c]};a->heads[c]=i;
}
static float aw_ao_contact(const AwOcclusion*a,float x,float y,float z){
    int cx=(int)floorf(x*.5f),cz=(int)floorf(z*.5f);float visibility=1;
    for(int zc=aw_clamp(cz-2,0,63);zc<=aw_clamp(cz+2,0,63);zc++)for(int xc=aw_clamp(cx-2,0,63);xc<=aw_clamp(cx+2,0,63);xc++)
        for(int i=a->heads[zc*64+xc];i>=0;i=a->contacts[i].next){
            const AwAoContact*p=&a->contacts[i];float dx=(x-p->x)/p->rx,dz=(z-p->z)/p->rz,dy=y-p->y;
            float radial=fmaxf(0,1-(dx*dx+dz*dz));
            float vertical=fmaxf(0,1-fabsf(dy)/(dy<0?.7f:p->height));
            visibility*=1-p->strength*radial*radial*vertical*vertical;
        }
    return 1-visibility;
}
static float aw_ao_sample(AwOcclusion*a,float x,float y,float z,float nx,float ny,float nz){
    int key[6]={(int)lroundf(x*4096),(int)lroundf(y*4096),(int)lroundf(z*4096),(int)lroundf(nx*1024),(int)lroundf(ny*1024),(int)lroundf(nz*1024)};
    uint32_t h=2166136261u;for(int i=0;i<6;i++)h=(h^(uint32_t)key[i])*16777619u;h=aw_hash(h)&(AW_AO_CACHE-1);
    AwAoSample*c=&a->samples[h];if(c->valid&&!memcmp(c->key,key,sizeof(key)))return c->value;
    int cell=aw_clamp((int)(z*.5f),0,63)*64+aw_clamp((int)(x*.5f),0,63);
    int underground=x>=0&&z>=0&&x<128&&z<128&&a->map->cave_bin_count[cell]&&y<aw_ao_height(a,x,z)-.06f;
    float occlusion;
    if(underground){
        occlusion=aw_ao_cavity(a,x,y,z,nx,ny,nz);a->cave_samples++;
        float cover=aw_ao_height(a,x,z)-y;
        if(cover<.6f)occlusion=aw_lerp(aw_ao_horizon(a,x,y,z,nx,ny,nz),occlusion,cover/.6f);
    }
    else occlusion=aw_ao_horizon(a,x,y,z,nx,ny,nz);
    float contact=aw_ao_contact(a,x,y,z);
    float visibility=(1-occlusion)*(1-contact);
    memcpy(c->key,key,sizeof(key));c->valid=1;c->value=aw_ao_clamp(visibility,0,1);a->samples_baked++;
    return c->value;
}
#endif
