#ifndef ALIENWARS_MOUNTAIN_H
#define ALIENWARS_MOUNTAIN_H
/* Route grades and excavation sockets are solved against the completed rock.
 * Surface height is never increased to hide a passage. Both open cuttings and
 * covered sections subtract from the same solid as the existing caves. */
/* Eight geometric sockets: narrow, standard, broad, chamber, tall fissure,
 * high vault, low barrel and great chamber. Compatibility uses dimensions,
 * not neighboring numeric IDs. */
static float aw_trail_radius(int profile){static const float r[8]={.72f,.90f,1.08f,1.26f,.92f,1.10f,1.24f,1.44f};return r[profile];}
static float aw_trail_height(int profile){static const float h[8]={4,4.6f,4.8f,5.8f,6.6f,5.6f,3.6f,6.8f};return h[profile];}
static float aw_trail_haunch(int profile){static const float h[8]={2,2,2,2.4f,3.5f,2.8f,1.2f,3};return h[profile];}
static int aw_trail_compatible(int a,int b){return fabsf(aw_trail_radius(a)-aw_trail_radius(b))<=.26f&&fabsf(aw_trail_height(a)-aw_trail_height(b))<=1.6f;}
static int aw_trail_grades(uint16_t*w,const uint8_t*ramp,int n){
    int changed=1;
    while(changed){changed=0;for(int i=1;i<n;i++){
        uint16_t a=w[i-1],b=w[i];
        uint16_t left=a|(ramp[i]?(a<<1)|(a>>1):0),right=b|(ramp[i]?(b<<1)|(b>>1):0);
        a&=right;b&=left;if(!a||!b)return 0;
        if(a!=w[i-1]||b!=w[i]){w[i-1]=a;w[i]=b;changed=1;}
    }}return 1;
}
static int aw_trail_grade_solve(AwMap*m,uint16_t*w,const uint8_t*ramp,const uint8_t*target,int n){
    if(!aw_trail_grades(w,ramp,n))return 0;
    for(;;){
        int best=-1,count=99;
        for(int i=0;i<n;i++){int c=__builtin_popcount(w[i]);if(c>1&&c<count){best=i;count=c;}}
        if(best<0)return 1;
        int chosen=-1,score=INT_MAX;
        for(int q=4;q<=12;q++)if(w[best]&(1<<q)){
            int s=aw_abs(q-target[best])*8+(aw_random(m)%7);if(s<score){score=s;chosen=q;}
        }
        if(chosen<0)return 0;
        w[best]=1<<chosen;m->structure_decisions++;
        if(!aw_trail_grades(w,ramp,n))return 0;
    }
}
static int aw_trail_profiles(uint8_t*w,int n){
    int changed=1;
    while(changed){changed=0;for(int i=1;i<n;i++){
        int a=w[i-1],b=w[i],left=0,right=0;
        for(int p=0;p<8;p++)for(int q=0;q<8;q++)if(aw_trail_compatible(p,q)){
            if(a&(1<<p))left|=1<<q;if(b&(1<<p))right|=1<<q;
        }
        a&=right;b&=left;if(!a||!b)return 0;
        if(a!=w[i-1]||b!=w[i]){w[i-1]=a;w[i]=b;changed=1;}
    }}return 1;
}
static int aw_mountain_trail(AwMap*m,int region,int branch){
    AwMountain*r=&m->mountains[region];int x[160],z[160],n=1;
    aw_mountain_position(r,r->branch[branch][0],&x[0],&z[0]);
    static const int dx[4]={1,0,-1,0},dz[4]={0,1,0,-1};
    x[0]-=2*dx[r->rotation];z[0]-=2*dz[r->rotation];
    for(int i=0;i<=r->length[branch];i++){
        int gx,gz;aw_mountain_position(r,r->branch[branch][i<r->length[branch]?i:i-1],&gx,&gz);
        if(i==r->length[branch]){gx+=2*dx[r->rotation];gz+=2*dz[r->rotation];}
        while(x[n-1]!=gx||z[n-1]!=gz){
            if(n>=160)return 0;x[n]=x[n-1]+(gx>x[n-1])-(gx<x[n-1]);z[n]=z[n-1]+(gz>z[n-1])-(gz<z[n-1]);n++;
        }
    }
    uint16_t wave[160];uint8_t ramp[160],target[160],profiles[160];
    for(int i=0;i<n;i++){
        float ground=40,high=0;
        for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
            float q=aw_height_q(m,x[i]+.5f+dx*1.15f,z[i]+.5f+dz*1.15f);ground=fminf(ground,q);high=fmaxf(high,q);
        }
        int cap=aw_clamp((int)floorf(ground+.001f),4,12);
        wave[i]=((1u<<(cap+1))-1)&~15u;
        int eligible=i>2&&i<n-3;
        for(int j=1;j<n-1;j++)if((x[j]-x[j-1])!=(x[j+1]-x[j])||(z[j]-z[j-1])!=(z[j+1]-z[j])){
            if(i==j||i==j+1)eligible=0;
        }
        ramp[i]=eligible;
        target[i]=branch?cap:aw_clamp(cap-2,4,9);
        /* The bypass follows the existing ground: no deep trench around a peak. */
        if(branch){
            float top=aw_height_q(m,x[i]+.5f,z[i]+.5f);
            int minimum=(int)ceilf(fmaxf(top-1.25f,high-2.5f));
            if(minimum>cap)return 0;
            if(minimum>4)wave[i]&=~((1u<<minimum)-1);
        }
        /* Existing road lanes and their full support stay authoritative. */
        for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
            int c=(z[i]+dz)*64+x[i]+dx;if(!m->cells[c].road)continue;
            int q=m->cells[c].q[0];if(q>cap)return 0;
            if(q>4)wave[i]&=~((1u<<q)-1);
        }
    }
    wave[0]=wave[n-1]=1<<4;
    if(!aw_trail_grades(wave,ramp,n))return 0;
    /* Grades follow the available ground rather than demanding a raised crest. */
    if(!aw_trail_grade_solve(m,wave,ramp,target,n))return 0;
    for(int i=0;i<n;i++){
        int q=__builtin_ctz(wave[i]);float cover=aw_height_q(m,x[i]+.5f,z[i]+.5f)-q;
        profiles[i]=branch?((1<<2)|(1<<3)|(1<<6)):255;
        if(r->step==2)profiles[i]&=branch?(1<<2):((1<<0)|(1<<1)|(1<<4));
        if(!branch&&cover>=5){
            for(int p=0;p<8;p++)if(aw_trail_height(p)+.65f>cover)profiles[i]&=~(1<<p);
            if(!profiles[i])profiles[i]=1;
        }
    }
    if(!aw_trail_profiles(profiles,n))return 0;
    for(;;){
        int best=-1,count=99;for(int i=0;i<n;i++){int c=__builtin_popcount(profiles[i]);if(c>1&&c<count){best=i;count=c;}}
        if(best<0)break;
        int pick=aw_random(m)%count,bits=profiles[best];while(pick--)bits&=bits-1;
        profiles[best]=bits&-bits;m->structure_decisions++;
        if(!aw_trail_profiles(profiles,n))return 0;
    }
    r->trail_length[branch]=n;
    for(int i=0;i<n;i++){
        if(m->trail_count>=AW_TRAIL_NODES)return 0;
        int node=m->trail_count++,q=__builtin_ctz(wave[i]),profile=__builtin_ctz(profiles[i]);
        float cover=aw_height_q(m,x[i]+.5f,z[i]+.5f)-q;
        int mode=branch?AW_TRAIL_CUT:cover>=aw_trail_height(profile)+.5f?AW_TRAIL_COVERED:AW_TRAIL_GALLERY;
        m->trail[node]=(AwTrailNode){.x=x[i],.z=z[i],.q=q,.profile=profile,.mode=mode};r->trail[branch][i]=node;
        if(i){if(m->trail_edge_count>=AW_TRAIL_EDGES)return 0;m->trail_edges[m->trail_edge_count++]=(AwTrailEdge){r->trail[branch][i-1],node,region,branch};}
    }return 1;
}
static int aw_mountain_index(AwMap*m){
    memset(m->trail_bin_count,0,sizeof(m->trail_bin_count));
    for(int i=0;i<m->trail_count;i++)m->trail[i].junction=1;
    for(int i=0;i<m->trail_edge_count;i++){
        const AwTrailEdge*e=&m->trail_edges[i];
        if(m->trail[e->a].q!=m->trail[e->b].q)m->trail[e->a].junction=m->trail[e->b].junction=0;
    }
    for(int i=0;i<m->trail_edge_count;i++){
        const AwTrailEdge*e=&m->trail_edges[i];const AwTrailNode*a=&m->trail[e->a],*b=&m->trail[e->b];
        int x0=(a->x<b->x?a->x:b->x)-2,x1=(a->x>b->x?a->x:b->x)+2;
        int z0=(a->z<b->z?a->z:b->z)-2,z1=(a->z>b->z?a->z:b->z)+2;
        for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++){
            if(x<0||z<0||x>=64||z>=64)return 0;
            int c=z*64+x,n=m->trail_bin_count[c];if(n>=AW_TRAIL_BIN)return 0;m->trail_bins[c][n]=i;m->trail_bin_count[c]++;
        }
    }return 1;
}
static int aw_mountain_build(AwMap*m){
    m->trail_count=m->trail_edge_count=0;
    if(!m->mountain_count)return 1;
    AwMountain*r=&m->mountains[0];int cover[2]={0};
    for(int b=0;b<2;b++)for(int i=0;i<r->length[b];i++){
        int x,z;aw_mountain_position(r,r->branch[b][i],&x,&z);cover[b]+=(int)aw_height_q(m,x+.5f,z+.5f)-4;
    }
    /* Assign the more enclosed arc to the interior route; topology remains WFC's. */
    if(cover[1]*r->length[0]>cover[0]*r->length[1]){
        int temp[25];memcpy(temp,r->branch[0],sizeof(temp));memcpy(r->branch[0],r->branch[1],sizeof(temp));memcpy(r->branch[1],temp,sizeof(temp));int t=r->length[0];r->length[0]=r->length[1];r->length[1]=t;
    }
    for(int b=0;b<2;b++)if(!aw_mountain_trail(m,0,b))return 0;
    if(m->options.symmetry){
        AwMountain*s=&m->mountains[1];
        memcpy(s->branch,r->branch,sizeof(r->branch));memcpy(s->length,r->length,sizeof(r->length));
        for(int b=0;b<2;b++){
            s->trail_length[b]=r->trail_length[b];
            for(int i=0;i<r->trail_length[b];i++){
                if(m->trail_count>=AW_TRAIL_NODES)return 0;
                AwTrailNode n=m->trail[r->trail[b][i]];n.x=63-n.x;n.z=63-n.z;
                int node=m->trail_count++;m->trail[node]=n;s->trail[b][i]=node;
                if(i){if(m->trail_edge_count>=AW_TRAIL_EDGES)return 0;m->trail_edges[m->trail_edge_count++]=(AwTrailEdge){s->trail[b][i-1],node,1,b};}
            }
        }
    }
    return aw_mountain_index(m);
}
static float aw_mountain_field(const AwMap*m,float x,float yq,float z){
    int c=aw_clamp((int)z,0,63)*64+aw_clamp((int)x,0,63);float result=1000;
    for(int i=0;i<m->trail_bin_count[c];i++){
        const AwTrailEdge*e=&m->trail_edges[m->trail_bins[c][i]];
        const AwTrailNode*a=&m->trail[e->a],*b=&m->trail[e->b];float dx=b->x-a->x,dz=b->z-a->z;
        float along=((x-a->x-.5f)*dx+(z-a->z-.5f)*dz)/(dx*dx+dz*dz),t=fminf(1,fmaxf(0,along));
        float cx=a->x+.5f+dx*t,cz=a->z+.5f+dz*t,q=aw_lerp(a->q,b->q,t);
        float radius=aw_lerp(aw_trail_radius(a->profile),aw_trail_radius(b->profile),t);
        float height=aw_lerp(aw_trail_height(a->profile),aw_trail_height(b->profile),t);
        float haunch=aw_lerp(aw_trail_haunch(a->profile),aw_trail_haunch(b->profile),t);
        float vx=(x-cx)/radius,vz=(z-cz)/radius,vy=e->branch?0:fmaxf(0,yq-q-haunch)/(height-haunch);
        float field=fmaxf(fmaxf(q-yq,(vx*vx+vz*vz+vy*vy-1)*2),fmaxf(-along-.25f,along-1.25f)*2);
        result=fminf(result,field);
        for(int end=0;end<2;end++){
            const AwTrailNode*n=end?b:a;if(!n->junction)continue;
            float r=aw_trail_radius(n->profile),h=aw_trail_height(n->profile),haunch=aw_trail_haunch(n->profile);
            float px=(x-n->x-.5f)/r,pz=(z-n->z-.5f)/r,py=e->branch?0:fmaxf(0,yq-n->q-haunch)/(h-haunch);
            result=fminf(result,fmaxf(n->q-yq,(px*px+pz*pz+py*py-1)*2));
        }
    }return result;
}
#endif
