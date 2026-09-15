#ifndef ALIENWARS_TRAVERSAL_H
#define ALIENWARS_TRAVERSAL_H
/* Walkable area sampled from the shared solid, including several floors in a
 * column. Centerlines remain a validated skeleton; these spans fill chambers,
 * cuttings, ledges and their actual openings onto the surrounding surface. */
static int aw_body_fits(const AwMap*m,float x,float q,float z,int large){
    float radius=large?.86f:.30f,height=large?2.5f:1.8f;
    static const float ring[9][2]={{0,0},{1,0},{.70710678f,.70710678f},{0,1},{-.70710678f,.70710678f},{-1,0},{-.70710678f,-.70710678f},{0,-1},{.70710678f,-.70710678f}};
    for(int i=0;i<9;i++){
        float px=x+ring[i][0]*radius,pz=z+ring[i][1]*radius;
        if(px<2||pz<2||px>62||pz>62)return 0;
        float support=aw_support_q(m,px,pz,q);
        if(fabsf(support-q)>(large?1.05f:.46f))return 0;
        if(aw_density(m,px,support+.45f,pz)>0||aw_density(m,px,support+height,pz)>0)return 0;
    }return 1;
}
static int aw_span_find(const AwMap*m,int cell,float q){
    for(int i=m->span_first[cell];i>=0;i=m->spans[i].next)if(fabsf(m->spans[i].q-q)<.6f)return i;
    return -1;
}
static int aw_span_add(AwMap*m,int cell,float hint){
    float x=cell%64+.5f,z=cell/64+.5f,q=aw_support_q(m,x,z,hint);
    if(fabsf(q-hint)>.8f)return -1;
    int found=aw_span_find(m,cell,q);if(found>=0)return found;
    if(m->span_count>=AW_SPANS||!aw_body_fits(m,x,q,z,0))return -1;
    int i=m->span_count++;AwSpan*s=&m->spans[i];
    *s=(AwSpan){.cell=cell,.q=q,.next=m->span_first[cell],.cave=-1,.fits=1};
    for(int d=0;d<4;d++)s->links[d]=-1;
    if(aw_body_fits(m,x,q,z,1))s->fits|=2;
    m->span_first[cell]=i;return i;
}
static int aw_span_segment(const AwMap*m,int a,int b,int large){
    const AwSpan*x=&m->spans[a],*y=&m->spans[b];
    if(!(x->fits&y->fits&(1<<large))||fabsf(x->q-y->q)>1.1f)return 0;
    for(int step=1;step<4;step++){
        float t=step*.25f,px=aw_lerp(x->cell%64,y->cell%64,t)+.5f,pz=aw_lerp(x->cell/64,y->cell/64,t)+.5f,q=aw_lerp(x->q,y->q,t);
        float actual=aw_support_q(m,px,pz,q);
        if(fabsf(actual-q)>.30f||!aw_body_fits(m,px,actual,pz,large))return 0;
    }return 1;
}
static int aw_traversal_build_uncached(AwMap*m){
    m->span_count=m->span_ready=0;
    memset(m->span_first,255,sizeof(m->span_first));memset(m->surface_span,255,sizeof(m->surface_span));
    memset(m->cave_span,255,sizeof(m->cave_span));memset(m->trail_span,255,sizeof(m->trail_span));
    /* The possible floor planes come from nearby excavations, but acceptance
     * depends on support and body clearance in the actual polygonal solid. */
    for(int c=0;c<AW_CELLS;c++){
        for(int j=0;j<m->cave_bin_count[c];j++){
            const AwCaveEdge*e=&m->cave_edges[m->cave_bins[c][j]];const AwCaveNode*a=&m->cave[e->a],*b=&m->cave[e->b];
            float dx=b->x-a->x,dz=b->z-a->z,t=((c%64-a->x)*dx+(c/64-a->z)*dz)/(dx*dx+dz*dz);
            if(t<-.3f||t>1.3f)continue;
            float q=aw_lerp(a->q,b->q,t);if(aw_span_find(m,c,q)<0)aw_span_add(m,c,q);
        }
        for(int j=0;j<m->trail_bin_count[c];j++){
            const AwTrailEdge*e=&m->trail_edges[m->trail_bins[c][j]];const AwTrailNode*a=&m->trail[e->a],*b=&m->trail[e->b];
            float dx=b->x-a->x,dz=b->z-a->z,t=((c%64-a->x)*dx+(c/64-a->z)*dz)/(dx*dx+dz*dz);
            if(t<-.3f||t>1.3f)continue;
            float q=aw_lerp(a->q,b->q,t);if(aw_span_find(m,c,q)<0)aw_span_add(m,c,q);
        }
    }
    for(int i=0;i<m->span_count;i++){
        AwSpan*s=&m->spans[i];
        float top=aw_surface_q(m,s->cell,.5f,.5f);
        if(fabsf(s->q-top)<.08f)m->surface_span[s->cell]=i;
        for(int d=1;d<=2;d++){
            int cell=aw_neighbor(s->cell,d);if(cell<0)continue;
            for(int j=m->span_first[cell];j>=0;j=m->spans[j].next){
                if(!aw_span_segment(m,i,j,0))continue;
                /* Different stories never gain a link from x/z overlap. */
                if(s->links[d]>=0)return 0;
                s->links[d]=j;m->spans[j].links[(d+2)%4]=i;
                int fits=1|(aw_span_segment(m,i,j,1)?2:0);s->edge_fits[d]=m->spans[j].edge_fits[(d+2)%4]=fits;
            }
        }
    }
    for(int i=0;i<m->cave_count;i++){
        const AwCaveNode*n=&m->cave[i];int s=aw_span_find(m,n->z*64+n->x,n->q);
        if(s<0)return 0;m->cave_span[i]=s;m->spans[s].cave=i;
    }
    for(int i=0;i<m->trail_count;i++){
        const AwTrailNode*n=&m->trail[i];int s=aw_span_find(m,n->z*64+n->x,n->q);
        if(s<0)return 0;m->trail_span[i]=s;
    }
    m->span_ready=1;return 1;
}
static int aw_traversal_build(AwMap*m){
    m->density_cache=calloc(AW_DENSITY_CACHE,sizeof(AwDensitySample));if(!m->density_cache)return 0;
    int result=aw_traversal_build_uncached(m);free(m->density_cache);m->density_cache=NULL;return result;
}
static int aw_mountain_quality(const AwMap*m){
    if(!m->mountain_count)return 1;
    const AwMountain*r=&m->mountains[0];int covered=0,run=0,longest=0;
    for(int i=0;i<r->trail_length[0];i++){
        const AwTrailNode*n=&m->trail[r->trail[0][i]];int roof=0;
        for(float q=n->q+3;q<aw_height_q(m,n->x+.5f,n->z+.5f)+1;q+=.5f)
            roof|=aw_density(m,n->x+.5f,q,n->z+.5f)>0;
        covered+=roof;run=roof?run+1:0;if(run>longest)longest=run;
    }
    /* A named mountain passage must contain useful enclosed travel, not just
     * a circuit painted around small bumps or a one-sample rock sliver. */
    return covered>=4&&longest>=3;
}
static int aw_mountain_validate(const AwMap*m){
    if(!m->options.tunnels)return !m->mountain_count&&!m->trail_count;
    if(m->mountain_count!=(m->options.symmetry?2:1)||!m->span_ready)return 0;
    for(int region=0;region<m->mountain_count;region++){
        const AwMountain*r=&m->mountains[region];
        for(int branch=0;branch<2;branch++){
            if(r->trail_length[branch]<5)return 0;
            for(int i=0;i<r->trail_length[branch];i++){
                int n=r->trail[branch][i];if(n<0||n>=m->trail_count)return 0;
                int s=m->trail_span[n];if(s<0||!m->reachable[AW_SPAN_START+s])return 0;
                if(branch&&!(m->spans[s].fits&2))return 0;
                if(i){
                    int prev=m->trail_span[r->trail[branch][i-1]],connected=0;
                    for(int d=0;d<4;d++)if(m->spans[prev].links[d]==s)
                        connected|=!branch||(m->spans[prev].edge_fits[d]&2);
                    if(!connected)return 0;
                }
            }
        }
    }return 1;
}
#endif
