#ifndef ALIENWARS_MOUNTAIN_LAYOUT_H
#define ALIENWARS_MOUNTAIN_LAYOUT_H
/* A small regional WFC, not a stored route stencil. Every nonempty tile has
 * two directional exits. Adjacency propagates both exits and walls; a global
 * constraint rejects disconnected circuits and preserves both approaches. */
static int aw_mountain_neighbor(int c,int d){
    int x=c%AW_MOUNT_GRID,z=c/AW_MOUNT_GRID;
    if(d==0)return z?c-AW_MOUNT_GRID:-1;
    if(d==1)return x<AW_MOUNT_GRID-1?c+1:-1;
    if(d==2)return z<AW_MOUNT_GRID-1?c+AW_MOUNT_GRID:-1;
    return x?c-1:-1;
}
static void aw_mountain_position(const AwMountain*r,int c,int*x,int*z){
    int u=c%5*r->step,v=c/5*r->step,end=4*r->step;
    for(int i=0;i<r->rotation;i++){int a=u;u=end-v;v=a;}
    *x=r->x+u;*z=r->z+v;
}
static int aw_mountain_propagate(uint16_t*w,int a,int b){
    int changed=1;
    while(changed){changed=0;
        for(int c=0;c<25;c++){
            if(!w[c])return 0;
            for(int d=0;d<4;d++){
                int n=aw_mountain_neighbor(c,d),allowed=0;
                for(int t=0;t<16;t++)if(w[c]&(1u<<t))allowed|=1<<!!(t&(1<<d));
                uint16_t bits=0;
                if(n<0){for(int t=0;t<16;t++)if(!(t&(1<<d)))bits|=1u<<t;n=c;}
                else for(int t=0;t<16;t++)if(allowed&(1<<!!(t&(1<<((d+2)%4)))))bits|=1u<<t;
                bits&=w[n];if(!bits)return 0;
                if(bits!=w[n]){w[n]=bits;changed=1;}
            }
        }
        /* All forced routes must still belong to the possible component of A.
         * A sealed loop cannot strand B or any other already selected route. */
        uint8_t seen[25]={0};int queue[25],head=0,tail=0;seen[a]=1;queue[tail++]=a;
        while(head<tail){int c=queue[head++];for(int d=0;d<4;d++){
            int n=aw_mountain_neighbor(c,d),exit=0;if(n<0||seen[n])continue;
            for(int t=0;t<16;t++)exit|=!!((w[c]&(1u<<t))&&(t&(1<<d)));
            if(exit){seen[n]=1;queue[tail++]=n;}
        }}
        if(!seen[b])return 0;
        for(int c=0;c<25;c++)if(!seen[c]){
            if(!(w[c]&1))return 0;
            if(w[c]!=1){w[c]=1;changed=1;}
        }
    }return 1;
}
static int aw_mountain_collapse(AwMountain*r,uint16_t*w,int depth){
    if(depth>25||r->backtracks>6000||!aw_mountain_propagate(w,r->a,r->b))return 0;
    int best=-1,count=99;uint32_t tie=UINT_MAX;
    for(int c=0;c<25;c++){
        int n=__builtin_popcount(w[c]);uint32_t h=aw_hash(r->salt^(uint32_t)c*7919u);
        if(n>1&&(n<count||(n==count&&h<tie))){best=c;count=n;tie=h;}
    }
    if(best<0)return 1;
    uint16_t snapshot[25];memcpy(snapshot,w,sizeof(snapshot));
    int order[16],n=0;uint32_t key[16];
    for(int t=0;t<16;t++)if(w[best]&(1u<<t)){
        uint32_t h=aw_hash(r->salt^(uint32_t)best*911u^(uint32_t)t*3517u^(uint32_t)depth*17u);
        /* Prefer compact, varied routes without filling every available cell. */
        if(!t)h/=2;
        int i=n++;while(i&&h<key[i-1]){key[i]=key[i-1];order[i]=order[i-1];i--;}
        key[i]=h;order[i]=t;
    }
    for(int i=0;i<n;i++){
        memcpy(w,snapshot,sizeof(snapshot));w[best]=1u<<order[i];r->decisions++;
        if(aw_mountain_collapse(r,w,depth+1))return 1;r->backtracks++;
    }return 0;
}
static int aw_mountain_cycle(AwMountain*r){
    uint16_t wave[25];uint16_t catalog=1;
    for(int t=0;t<16;t++)if(__builtin_popcount((unsigned)t)==2)catalog|=1u<<t;
    for(int c=0;c<25;c++)wave[c]=catalog;
    wave[r->a]&=~1u;wave[r->b]&=~1u;
    for(int t=0;t<16;t++){
        if(!(t&2))wave[r->a]&=~(1u<<t);
        if(!(t&8))wave[r->b]&=~(1u<<t);
    }
    /* An interior destination prevents a perimeter-only loop. Its position is
     * seeded; all intervening topology is chosen by propagation/collapse. */
    int interior=(r->a/5)*5+2;wave[interior]&=~1u;
    if(!aw_mountain_collapse(r,wave,0))return 0;
    for(int c=0;c<25;c++)r->sockets[c]=__builtin_ctz(wave[c]);
    int branch=0;
    for(int d=0;d<4;d++)if(r->sockets[r->a]&(1<<d)){
        int prev=r->a,c=aw_mountain_neighbor(prev,d),length=1;
        r->branch[branch][0]=prev;
        while(c!=r->b){
            if(c<0||length>=24)return 0;r->branch[branch][length++]=c;
            int next=-1;
            for(int e=0;e<4;e++)if(r->sockets[c]&(1<<e)){
                int n=aw_mountain_neighbor(c,e);if(n!=prev)next=n;
            }
            prev=c;c=next;
        }
        r->branch[branch][length++]=r->b;r->length[branch++]=length;
    }
    return branch==2&&r->length[0]>=5&&r->length[1]>=5;
}
static int aw_mountain_plan(AwMap*m){
    if(!m->options.tunnels)return 1;
    uint32_t rng=aw_hash(m->layout_seed^0x7d3921u);
    int best_x=-1,best_z=-1,best_step=0,best=INT_MAX;
    /* Place a regional grammar in available space; protect lakes, high roads,
     * base pads and the two primary entrance districts. */
    for(int trial=0;trial<1000;trial++){
        int step=trial>=750?2:3+(aw_plan_random(&rng)%2),end=4*step;
        int x=6+aw_plan_random(&rng)%(52-end),z=6+aw_plan_random(&rng)%(52-end);
        if(m->options.symmetry&&z+end+3>31&&x+end+3>31)continue;
        int score=step==2?10000:0,legal=1;
        for(int side=0;side<2;side++){
            int c=m->spawns[side],dx=c%64-x,dz=c/64-z;
            if(dx>=-4&&dz>=-4&&dx<=end+4&&dz<=end+4)legal=0;
            c=m->landmarks[side];dx=c%64-x;dz=c/64-z;
            if(dx>=-3&&dz>=-3&&dx<=end+3&&dz<=end+3)legal=0;
        }
        for(int zz=z-3;zz<=z+end+3&&legal;zz++)for(int xx=x-3;xx<=x+end+3;xx++){
            int c=zz*64+xx;
            for(int l=0;l<m->lake_count;l++){
                const AwLake*p=&m->lakes[l];int dx=xx-p->x,dz=zz-p->z;
                if(dx*dx*(p->rz+2)*(p->rz+2)+dz*dz*(p->rx+2)*(p->rx+2)<=(p->rx+2)*(p->rx+2)*(p->rz+2)*(p->rz+2))legal=0;
            }
            if(m->cells[c].road){
                for(int k=0;k<4;k++)if(m->cells[c].q[k]!=4)legal=0;
                if(xx>x+1&&xx<x+end-1&&zz>z+1&&zz<z+end-1)score+=60;
                score+=3;
            }
        }
        if(!legal)continue;
        int cx=x+end/2,cz=z+end/2,nearest=1000;
        for(int c=0;c<AW_CELLS;c++)if(m->cells[c].road&&m->cells[c].q[0]==4){
            int distance=aw_abs(c%64-cx)+aw_abs(c/64-cz);if(distance<nearest)nearest=distance;
        }
        score+=nearest*4+(aw_plan_random(&rng)%31);
        if(score<best){best=score;best_x=x;best_z=z;best_step=step;}
    }
    if(best_x<0)return 0;
    AwMountain*r=&m->mountains[0];
    *r=(AwMountain){.x=best_x,.z=best_z,.step=best_step,.salt=aw_plan_random(&rng)};
    r->rotation=r->salt%4;r->a=(1+(r->salt>>4)%3)*5;r->b=r->a+4;
    int end=4*r->step;
    r->peak_count=2+(r->salt%2);
    for(int p=0;p<r->peak_count;p++){
        int c=p<2?r->a+1+p*2:(r->a/5==1?3:1)*5+1+aw_plan_random(&rng)%3;
        int x,z;aw_mountain_position(r,c,&x,&z);
        r->peak_x[p]=aw_clamp(x-r->x+(int)(aw_plan_random(&rng)%3)-1,2,end-2);
        r->peak_z[p]=aw_clamp(z-r->z+(int)(aw_plan_random(&rng)%3)-1,2,end-2);
        r->peak_q[p]=12+aw_plan_random(&rng)%17;
        r->peak_radius[p]=r->step+(aw_plan_random(&rng)%2);
    }
    int solved=0;
    for(int trial=0;trial<32&&!solved;trial++){r->salt=aw_plan_random(&rng);r->backtracks=0;solved=aw_mountain_cycle(r);}
    if(!solved)return 0;
    m->mountain_count=m->options.symmetry?2:1;
    if(m->options.symmetry){
        m->mountains[1]=*r;AwMountain*s=&m->mountains[1];s->x=63-r->x-end;s->z=63-r->z-end;s->rotation=(r->rotation+2)%4;
        for(int p=0;p<s->peak_count;p++){s->peak_x[p]=end-r->peak_x[p];s->peak_z[p]=end-r->peak_z[p];}
    }
    for(int i=0;i<m->mountain_count;i++){
        AwMountain*s=&m->mountains[i];
        for(int z=s->z;z<=s->z+end;z++)for(int x=s->x;x<=s->x+end;x++)m->mountain_mask[z*64+x]=1;
    }
    for(int side=0;side<2;side++){
        int x,z;aw_mountain_position(r,side?r->b:r->a,&x,&z);
        static const int dx[4]={1,0,-1,0},dz[4]={0,1,0,-1};
        x+=(side?2:-2)*dx[r->rotation];z+=(side?2:-2)*dz[r->rotation];
        int c=z*64+x,nearest=-1,distance=1000;
        for(int n=0;n<AW_CELLS;n++)if(m->cells[n].road&&m->cells[n].q[0]==4&&!m->mountain_mask[n]){
            int d=aw_abs(n%64-x)+aw_abs(n/64-z);if(d<distance){distance=d;nearest=n;}
        }
        if(nearest<0||!aw_flat_corridor(m,c,nearest,m->options.symmetry,aw_plan_random(&rng)))return 0;
    }
    m->structure_decisions+=r->decisions;return 1;
}
/* Domain-warped ridge lobes establish rock before route excavation. The WFC
 * route never raises terrain to obtain cover. Shared world coordinates keep
 * the density continuous through all tile boundaries. */
static int aw_mountain_height(const AwMap*m,int x,int z,int original){
    for(int i=0;i<m->mountain_count;i++){
        const AwMountain*r=&m->mountains[i];int end=4*r->step;
        int dx=x-r->x,dz=z-r->z;if(dx< -2||dz< -2||dx>end+3||dz>end+3)continue;
        if(m->options.symmetry&&i){dx=end+1-dx;dz=end+1-dz;r=&m->mountains[0];}
        int q=original;
        /* Coordinates are quarter-tiles; bilinear value warping is integer and
         * reproducible across native and WASM. */
        int wx=dx*4+(aw_noise(r->salt,dx+8,dz+8,5)-128)/64;
        int wz=dz*4+(aw_noise(r->salt^917u,dx+8,dz+8,6)-128)/64;
        for(int p=0;p<r->peak_count;p++){
            int u=wx-r->peak_x[p]*4-2,v=wz-r->peak_z[p]*4-2,rr=r->peak_radius[p]*4;
            int value=(rr*rr-u*u-v*v)*r->peak_q[p]/(rr*rr);if(value>0&&value>q-4)q=4+value;
        }
        /* Organic ground follows the solved route footprint, not the square
         * boundary of its constraint grid. Existing hills are preserved. */
        int nearest=100000;
        for(int c=0;c<25;c++)for(int d=1;d<=2;d++)if(r->sockets[c]&(1<<d)){
            int n=aw_mountain_neighbor(c,d),ax,az,bx,bz;if(n<0)continue;
            aw_mountain_position(r,c,&ax,&az);aw_mountain_position(r,n,&bx,&bz);
            ax=(ax-r->x)*2+1;az=(az-r->z)*2+1;bx=(bx-r->x)*2+1;bz=(bz-r->z)*2+1;
            int px=aw_clamp(dx*2,ax<bx?ax:bx,ax>bx?ax:bx),pz=aw_clamp(dz*2,az<bz?az:bz,az>bz?az:bz);
            int distance=(px-dx*2)*(px-dx*2)+(pz-dz*2)*(pz-dz*2);if(distance<nearest)nearest=distance;
        }
        int foundation=nearest<=16?4:nearest<64?(64-nearest)*4/48:0;
        if(foundation>q)q=foundation;
        original=aw_clamp(q,0,40);
    }return original;
}
#endif
