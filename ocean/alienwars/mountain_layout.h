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
    for(int c=0;c<25;c++)wave[c]=catalog&r->domains[c];
    wave[r->a]&=~1u;wave[r->b]&=~1u;
    for(int t=0;t<16;t++){
        if(!(t&2))wave[r->a]&=~(1u<<t);
        if(!(t&8))wave[r->b]&=~(1u<<t);
    }
    /* An interior destination prevents a perimeter-only loop. Its position is
     * seeded; all intervening topology is chosen by propagation/collapse. */
    wave[r->core]&=~1u;
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
#endif
