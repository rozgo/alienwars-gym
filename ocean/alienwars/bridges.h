#ifndef ALIENWARS_BRIDGES_H
#define ALIENWARS_BRIDGES_H
/* Terrain-first bank-pair search. Rank real gaps by the existing walking
 * detour, then validate deck, ramps and both body sizes on the meshed solid.
 * No terrain, lake, road or cave is moved to make a bridge candidate fit. */
static int aw_bridge_bank(const AwMap*m,int c){
    if(c<0||c>=AW_CELLS||!m->reachable[c]||!m->walkable[c]||m->bridge_bins[c]||m->cave_bin_count[c]||m->trail_bin_count[c])return 0;
    int q=m->cells[c].q[0];if(q<4||q>16)return 0;
    for(int k=1;k<4;k++)if(m->cells[c].q[k]!=q)return 0;
    return aw_body_fits(m,c%64+.5f,q,c/64+.5f,1);
}
static int aw_bridge_candidate(const AwMap*m,AwBridge*b){
    int gaps=0,wet=0;
    for(int u=-1;u<=b->length+1;u++)for(int v=-1;v<=1;v++){
        int x=b->x+b->dx*u+b->dz*v,z=b->z+b->dz*u-b->dx*v,c=z*64+x;
        if(x<3||z<3||x>60||z>60||m->bridge_bins[c]||m->cave_bin_count[c]||m->trail_bin_count[c])return 0;
        float top=aw_bridge_q(b,u);
        for(int k=0;k<5;k++){
            float px=x+(k==0?.5f:(k==1||k==4?0:1)),pz=z+(k==0?.5f:(k<=2?0:1));
            if(aw_height_q(m,px,pz)>top+.05f)return 0;
        }
        if(!v&&u>1&&u<b->length-1){
            float bed=aw_height_q(m,x+.5f,z+.5f);
            gaps+=bed<top-2;wet+=bed<1.44f;
        }
    }
    return gaps>=3&&gaps*2>=b->length&&wet>=2&&wet*3>=b->length;
}
static int aw_bridge_validate(const AwMap*m,const AwBridge*b){
    int previous=-1;
    for(int u=0;u<=b->length;u++){
        int c=(b->z+b->dz*u)*64+b->x+b->dx*u,s=aw_span_find(m,c,aw_bridge_q(b,u));
        if(s<0||!m->reachable[AW_SPAN_START+s]||!(m->spans[s].fits&2))return 0;
        if(previous>=0){int linked=0;for(int d=0;d<4;d++)if(m->spans[previous].links[d]==s)linked=m->spans[previous].edge_fits[d]&2;if(!linked)return 0;}
        previous=s;
        if((u==0||u==b->length)&&m->surface_span[c]!=s)return 0;
    }return 1;
}
static void aw_bridges(AwMap*m){
    typedef struct {AwBridge bridge;uint32_t score;} Candidate;
    Candidate best[32];int count=0;uint8_t bank[AW_CELLS];
    for(int c=0;c<AW_CELLS;c++)bank[c]=aw_bridge_bank(m,c);
    /* Cheap geometric filtering precedes pathfinding and volume validation. */
    for(int c=0;c<AW_CELLS;c++){
        if(m->options.symmetry&&c>2047)continue;
        if(!bank[c])continue;
        for(int axis=0;axis<2;axis++)for(int length=6;length<=(AW_VERSION>=12?24:20);length++){
            int dx=!axis,dz=axis,x=c%64,z=c/64,bx=x+dx*length,bz=z+dz*length;
            if(bx>60||bz>60)break;
            int end=bz*64+bx;if(!bank[end])continue;
            int qa=m->cells[c].q[0],qb=m->cells[end].q[0];if(aw_abs(qa-qb)>length-5)continue;
            AwBridge b={x,z,dx,dz,length,qa,qb,(qa>qb?qa:qb)+2};
            if(!aw_bridge_candidate(m,&b))continue;
            if(m->options.symmetry){
                /* Avoid self-overlapping mirrored footprints before ranking. */
                int x0=x-2,x1=bx+2,z0=z-2,z1=bz+2;
                if(x0<=63-x0&&x1>=63-x1&&z0<=63-z0&&z1>=63-z1)continue;
            }
            if(!aw_find_path(m,c,end)||m->path_length<length+7)continue;
            uint32_t score=(m->path_length-length)*256+aw_hash(m->layout_seed^(uint32_t)c*313u^(uint32_t)length*7919u^axis)%256;
            /* Keep geographically distinct alternatives instead of filling the
             * shortlist with adjacent lanes at one unsuitable crossing. */
            int nearby=-1;
            for(int j=0;j<count;j++){const AwBridge*a=&best[j].bridge;
                if(a->dx==dx&&a->dz==dz&&aw_abs(a->x-x)+aw_abs(a->z-z)<=4&&aw_abs(a->length-length)<=4){nearby=j;break;}
            }
            if(nearby>=0){if(score<=best[nearby].score)continue;for(int j=nearby;j<count-1;j++)best[j]=best[j+1];count--;}
            if(count==32&&score<=best[31].score)continue;int pos=count<32?count++:31;
            while(pos&&score>best[pos-1].score){if(pos<32)best[pos]=best[pos-1];pos--;}
            best[pos]=(Candidate){b,score};
        }
    }
    AwMap*saved=malloc(sizeof(*saved));if(!saved){aw_find_path(m,m->spawns[0],m->spawns[1]);return;}
    int wanted=(1+aw_hash(m->layout_seed^0x371fu)%2)*(m->options.symmetry?2:1);
    for(int i=0;i<count&&m->bridge_count<wanted;i++){
        AwBridge b=best[i].bridge;if(!aw_bridge_candidate(m,&b))continue;
        *saved=*m;m->bridges[m->bridge_count++]=b;
        if(m->options.symmetry){b.x=63-b.x;b.z=63-b.z;b.dx=-b.dx;b.dz=-b.dz;m->bridges[m->bridge_count++]=b;}
        int valid=aw_bridge_index(m)&&aw_validate(m);
        if(!valid)*m=*saved;
    }
    free(saved);aw_find_path(m,m->spawns[0],m->spawns[1]);
}
#endif
