#ifndef ALIENWARS_NATURAL_ROUTES_H
#define ALIENWARS_NATURAL_ROUTES_H
/* Optional routes fit a completed, valid world. This pass can only excavate:
 * it cannot add a peak, raise a road foundation, move a lake, or change a tile.
 * Failed candidates leave that same world intact rather than rerolling it. */
typedef struct {float low,high,top;uint8_t dry,approach;} AwNaturalSite;
static void aw_natural_sites(const AwMap*m,AwNaturalSite*sites){
    for(int c=0;c<AW_CELLS;c++){
        AwNaturalSite*s=&sites[c];int x=c%64,z=c/64;
        *s=(AwNaturalSite){.low=40,.high=0,.top=aw_height_q(m,x+.5f,z+.5f)};
        if(x<4||z<4||x>59||z>59)continue;
        for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
            float q=aw_height_q(m,x+.5f+dx*1.15f,z+.5f+dz*1.15f);
            s->low=fminf(s->low,q);s->high=fmaxf(s->high,q);
        }
        s->dry=s->low>=3.999f&&aw_cost[m->cells[c].material]>0;
        /* Preserve the full support footprint of existing elevated roads. */
        for(int dz=-2;dz<=2&&s->dry;dz++)for(int dx=-2;dx<=2;dx++){
            const AwCell*t=&m->cells[(z+dz)*64+x+dx];
            if(t->road)for(int k=0;k<4;k++)if(t->q[k]!=4)s->dry=0;
        }
        s->approach=s->dry&&s->high<4.01f&&m->walkable[c]&&m->reachable[c];
    }
}
static int aw_natural_edge(const AwNaturalSite*sites,int ax,int az,int bx,int bz){
    int dx=(bx>ax)-(bx<ax),dz=(bz>az)-(bz<az);
    for(;;){if(!sites[az*64+ax].dry)return 0;if(ax==bx&&az==bz)return 1;ax+=dx;az+=dz;}
}
static int aw_natural_domain(AwMountain*r,const AwNaturalSite*sites,uint32_t*rng){
    int best=-1;float cover=11;
    for(int c=0;c<25;c++){
        int x,z;aw_mountain_position(r,c,&x,&z);uint16_t domain=1;
        if(sites[z*64+x].dry){
            int exits=0;
            for(int d=0;d<4;d++){
                int n=aw_mountain_neighbor(c,d),nx,nz;if(n<0)continue;
                aw_mountain_position(r,n,&nx,&nz);
                if(aw_natural_edge(sites,x,z,nx,nz))exits|=1<<d;
            }
            for(int t=0;t<16;t++)if(__builtin_popcount((unsigned)t)==2&&(t&exits)==t)domain|=1u<<t;
            if(c%5>0&&c%5<4&&c/5>0&&c/5<4&&domain!=1&&sites[z*64+x].top>cover){best=c;cover=sites[z*64+x].top;}
        }
        r->domains[c]=domain;
    }
    if(best<0)return 0;r->core=best;
    static const int dx[4]={1,0,-1,0},dz[4]={0,1,0,-1};
    int ends[2][3],count[2]={0};
    for(int side=0;side<2;side++)for(int row=1;row<4;row++){
        int c=row*5+(side?4:0),x,z;aw_mountain_position(r,c,&x,&z);int ok=1;
        for(int i=0;i<=2;i++){
            int nx=x+(side?i:-i)*dx[r->rotation],nz=z+(side?i:-i)*dz[r->rotation];
            if(!sites[nz*64+nx].approach)ok=0;
        }
        if(ok&&r->domains[c]!=1)ends[side][count[side]++]=c;
    }
    if(!count[0]||!count[1])return 0;
    r->a=ends[0][aw_plan_random(rng)%count[0]];r->b=ends[1][aw_plan_random(rng)%count[1]];
    return 1;
}
static void aw_natural_passages(AwMap*m){
    if(!m->options.tunnels||m->mountain_count)return;
    AwNaturalSite*sites=calloc(AW_CELLS,sizeof(*sites));AwMap*original=malloc(sizeof(*original));
    if(!sites||!original){free(sites);free(original);return;}
    aw_natural_sites(m,sites);memcpy(original,m,sizeof(*m));
    int rock[AW_CELLS],count=0;
    for(int c=0;c<AW_CELLS;c++)if(sites[c].dry&&sites[c].top>=12)rock[count++]=c;
    uint32_t rng=aw_hash(m->layout_seed^0x7d3921u);int solves=0,geometry=0;
    for(int trial=0;count&&trial<3000&&solves<96&&geometry<24;trial++){
        int c=rock[aw_plan_random(&rng)%count],step=3+aw_plan_random(&rng)%2,end=step*4;
        AwMountain r={.x=c%64-step*(1+aw_plan_random(&rng)%3),.z=c/64-step*(1+aw_plan_random(&rng)%3),
            .step=step,.rotation=aw_plan_random(&rng)%4,.salt=aw_plan_random(&rng)};
        if(r.x<6||r.z<6||r.x+end>57||r.z+end>57)continue;
        if(m->options.symmetry&&r.z+end+3>31&&r.x+end+3>31)continue;
        if(!aw_natural_domain(&r,sites,&rng))continue;
        solves++;if(!aw_mountain_cycle(&r))continue;
        memcpy(m,original,sizeof(*m));m->rng=aw_hash(r.salt^7919u);
        m->mountains[0]=r;m->mountain_count=m->options.symmetry?2:1;
        if(m->options.symmetry){AwMountain*s=&m->mountains[1];*s=r;s->x=63-r.x-end;s->z=63-r.z-end;s->rotation=(r.rotation+2)%4;}
        if(!aw_mountain_build(m)||!aw_mountain_quality(m))continue;
        geometry++;m->structure_decisions+=r.decisions;
        if(aw_validate(m)){free(sites);free(original);return;}
    }
    memcpy(m,original,sizeof(*m));free(sites);free(original);
}
#endif
