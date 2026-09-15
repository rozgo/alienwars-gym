#ifndef ALIENWARS_LAYOUT_H
#define ALIENWARS_LAYOUT_H
/* Seeded global planning. WFC resolves elevations/materials after this plan.
 * Roads are self-avoiding walks on a jittered coarse lattice, never a stored
 * zigzag. Flat connectors use a weighted shortest path around graded roads. */
typedef struct {int x[160],z[160],ramp[160],n;} AwRoadPlan;
typedef struct {int xs[4],zs[4],nodes[16],goal,wanted,drop,budget;uint32_t salt;AwRoadPlan *out;} AwRoadSearch;
static uint32_t aw_plan_random(uint32_t*s){*s+=0x9e3779b9u;return aw_hash(*s);}
static int aw_raster_road(AwRoadSearch*s,int count){
    AwRoadPlan*p=s->out;p->n=1;p->x[0]=s->xs[s->nodes[0]%4];p->z[0]=s->zs[s->nodes[0]/4];
    int turns[16],nt=0;
    for(int j=1;j<count;j++){
        int x=p->x[p->n-1],z=p->z[p->n-1],gx=s->xs[s->nodes[j]%4],gz=s->zs[s->nodes[j]/4];
        if(j>1&&s->nodes[j]-s->nodes[j-1]!=s->nodes[j-1]-s->nodes[j-2])turns[nt++]=p->n-1;
        while(x!=gx||z!=gz){if(p->n>=160)return 0;x+=(gx>x)-(gx<x);z+=(gz>z)-(gz<z);p->x[p->n]=x;p->z[p->n++]=z;}
    }
    int eligible=0;
    for(int i=0;i<p->n;i++){
        int ok=i>3&&i<p->n-7;
        for(int j=0;j<nt;j++)if(aw_abs(i-turns[j])<=2)ok=0;
        eligible+=p->ramp[i]=ok;
    }
    return eligible>=s->drop;
}
static int aw_road_search(AwRoadSearch*s,int count,uint16_t used){
    if(--s->budget<=0)return 0;
    int c=s->nodes[count-1];
    if(c==s->goal)return count>=s->wanted&&aw_raster_road(s,count);
    if(count==16)return 0;
    int next[4],n=0;uint32_t keys[4];
    for(int d=0;d<4;d++){
        int v=d==0?c-4:d==1?c+1:d==2?c+4:c-1;
        if(v<0||v>=16||(d==1&&c%4==3)||(d==3&&c%4==0)||(used&(1u<<v)))continue;
        uint32_t key=aw_hash(s->salt^(uint32_t)used*719u^(uint32_t)v*7919u);int i=n++;
        while(i&&key<keys[i-1]){keys[i]=keys[i-1];next[i]=next[i-1];i--;}
        keys[i]=key;next[i]=v;
    }
    for(int i=0;i<n;i++){s->nodes[count]=next[i];if(aw_road_search(s,count+1,used|(1u<<next[i])))return 1;}
    return 0;
}
static int aw_road(AwMap*m,int side){
    uint32_t salt=aw_hash(m->layout_seed^(side&&!m->options.symmetry?0x391714u:0x3197u)),rng=salt;
    AwRoadPlan p={0};AwRoadSearch search={.drop=4*((side?m->options.floors_b:m->options.floors_a)-1),.out=&p,.salt=salt};
    search.xs[0]=5+aw_plan_random(&rng)%3;search.zs[0]=5+aw_plan_random(&rng)%3;
    for(int i=1;i<4;i++){search.xs[i]=search.xs[i-1]+6+aw_plan_random(&rng)%2;search.zs[i]=search.zs[i-1]+6+aw_plan_random(&rng)%2;}
    int solved=0;
    for(int attempt=0;attempt<24&&!solved;attempt++){
        search.nodes[0]=(aw_plan_random(&rng)%3)*4+aw_plan_random(&rng)%3;
        int edge=aw_plan_random(&rng)%7;search.goal=edge<4?edge*4+3:12+edge-4;
        search.wanted=7+aw_plan_random(&rng)%7;search.budget=30000;search.salt=aw_plan_random(&rng);
        solved=aw_road_search(&search,1,1u<<search.nodes[0]);
    }
    if(!solved)return 0;
    int hq[160],floor=side?m->options.floors_b:m->options.floors_a;
    uint32_t saved=m->rng;m->rng=aw_hash(salt^0x71935u);
    if(!aw_road_wfc(m,hq,p.ramp,p.n,floor))return 0;m->rng=saved;
    uint8_t road[AW_CELLS]={0};int vq[AW_VERT*AW_VERT];
    for(int i=0;i<p.n;i++)for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++)road[(p.z[i]+dz)*64+p.x[i]+dx]=1;
    for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++)road[(p.z[0]+dz)*64+p.x[0]+dx]=1;
    for(int z=0;z<AW_VERT;z++)for(int x=0;x<AW_VERT;x++){
        int best=INT_MAX,q=4;
        for(int i=1;i<p.n;i++){
            int ax=p.x[i-1]*2+1,az=p.z[i-1]*2+1,dx=p.x[i]-p.x[i-1],dz=p.z[i]-p.z[i-1];
            int t=aw_clamp((x*2-ax)*dx+(z*2-az)*dz,0,2),ex=x*2-ax-dx*t,ez=z*2-az-dz*t,dist=ex*ex+ez*ez;
            if(dist<best){best=dist;q=(hq[i-1]*(2-t)+hq[i]*t)/2;}
        }
        if(aw_abs(x*2-(p.x[0]*2+1))<=5&&aw_abs(z*2-(p.z[0]*2+1))<=5)q=floor*4;
        vq[z*AW_VERT+x]=q;
    }
    for(int c=0;c<AW_CELLS;c++)if(road[c]){
        int x=c%64,z=c/64,dst=side?AW_CELLS-1-c:c;
        int q[4]={vq[z*AW_VERT+x],vq[z*AW_VERT+x+1],vq[(z+1)*AW_VERT+x+1],vq[(z+1)*AW_VERT+x]};
        m->cells[dst].road=1;for(int k=0;k<4;k++)m->cells[dst].q[(k+side*2)%4]=q[k];
    }
    int a=p.z[0]*64+p.x[0],b=p.z[p.n-1]*64+p.x[p.n-1];
    m->spawns[side]=side?4095-a:a;m->road_ends[side]=side?4095-b:b;
    int resource=a+2;m->resources[side*2]=side?4095-resource:resource;
    resource=p.z[p.n*2/3]*64+p.x[p.n*2/3];m->resources[side*2+1]=side?4095-resource:resource;
    return 1;
}
static void aw_landform(AwMap*m,int x2,int z2,int rx,int rz,int height,int rotation){
    if(m->landform_count>=AW_LANDFORMS)return;
    m->landforms[m->landform_count++]=(AwLandform){x2,z2,rx,rz,height,rotation};
}
static int aw_land_score(const AwMap*m,int x,int z,int *height){
    static const int dirs[8][2]={{256,0},{181,181},{0,256},{-181,181},{-256,0},{-181,-181},{0,-256},{181,-181}};
    int warp=3+(m->layout_seed%6);
    int wx=x*2+(aw_noise(m->layout_seed^71u,x,z,11)-128)*warp/64;
    int wz=z*2+(aw_noise(m->layout_seed^97u,x,z,13)-128)*warp/64;
    int land=-10000,q=4;
    for(int i=0;i<m->landform_count;i++){
        const AwLandform*f=&m->landforms[i];int dx=wx-f->x2,dz=wz-f->z2;
        int u=(dx*dirs[f->rotation][0]+dz*dirs[f->rotation][1])/256,v=(-dx*dirs[f->rotation][1]+dz*dirs[f->rotation][0])/256;
        int d=u*u*250/(f->rx*f->rx)+v*v*250/(f->rz*f->rz),score=1000-d;
        if(score>land)land=score;
        int top=f->height*4-aw_clamp(d-170,0,3000)*(1+(m->layout_seed%3))/80;
        if(score>0&&top>q)q=top;
    }
    int edge=x<z?x:z;edge=edge<64-x?edge:64-x;edge=edge<64-z?edge:64-z;
    if(edge<8)land-=(8-edge)*(8-edge)*40;
    *height=q;return land+(aw_noise(m->layout_seed^913u,x,z,3)-128);
}
static int aw_flat_corridor(AwMap*m,int start,int goal,int mirror,uint32_t salt){
    int dist[AW_CELLS],prev[AW_CELLS];uint8_t closed[AW_CELLS]={0},blocked[AW_CELLS]={0};
    for(int c=0;c<AW_CELLS;c++){
        dist[c]=INT_MAX;prev[c]=-1;
        int x=c%64,z=c/64;if(x<4||z<4||x>59||z>59){blocked[c]=1;continue;}
        for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
            int n=(z+dz)*64+x+dx;if(!m->cells[n].road)continue;
            for(int k=0;k<4;k++)if(m->cells[n].q[k]!=4)blocked[c]=1;
        }
        for(int i=0;i<m->lake_count;i++){
            const AwLake*l=&m->lakes[i];int dx=2*x+1-2*l->x,dz=2*z+1-2*l->z,rx=2*(l->rx+3),rz=2*(l->rz+3);
            if(dx*dx*rz*rz+dz*dz*rx*rx<=rx*rx*rz*rz)blocked[c]=1;
        }
    }
    if(blocked[start]||blocked[goal])return 0;dist[start]=0;
    for(int count=0;count<AW_CELLS;count++){
        int c=-1,best=INT_MAX;
        for(int i=0;i<AW_CELLS;i++)if(!closed[i]&&dist[i]<INT_MAX){int h=10*(aw_abs(i%64-goal%64)+aw_abs(i/64-goal/64));if(dist[i]+h<best){best=dist[i]+h;c=i;}}
        if(c<0)return 0;if(c==goal)break;closed[c]=1;
        for(int d=0;d<4;d++){
            int n=aw_neighbor(c,d);if(n<0||blocked[n]||closed[n])continue;
            int x=n%64,z=n/64,cc=m->options.symmetry&&n>=2048?4095-n:n,q=4;
            int land=aw_land_score(m,x,z,&q);
            int cost=dist[c]+10+aw_noise(salt,cc%64,cc/64,8)/12+(land<0?7:0);
            if(cost<dist[n]){dist[n]=cost;prev[n]=c;}
        }
    }
    if(start!=goal&&prev[goal]<0)return 0;
    int length=0;
    for(int c=goal;c>=0;c=prev[c]){
        if(++length>AW_CELLS)return 0;
        for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
            int n=c+dz*64+dx;m->cells[n].road=1;aw_flat(&m->cells[n],4);
            if(mirror){m->cells[4095-n].road=1;aw_flat(&m->cells[4095-n],4);}
        }
        if(c==start)break;
    }
    m->center=goal;return 1;
}
/* Plan closed basins before the lowland roads. Each has a dry catchment rim;
 * the route search treats the basin and shore as obstacles. All water currently
 * shares one datum, so these are inland lakes rather than elevated reservoirs. */
static void aw_plan_lakes(AwMap*m,uint32_t*rng){
    int pair=m->options.symmetry?2:1,wanted=1+aw_plan_random(rng)%3;
    for(int attempt=0;attempt<240&&m->lake_count<wanted*pair;attempt++){
        int x=7+aw_plan_random(rng)%50,z=7+aw_plan_random(rng)%(m->options.symmetry?22:50);
        int rx=3+aw_plan_random(rng)%5,rz=3+aw_plan_random(rng)%5,ok=1;
        for(int side=0;side<2;side++){
            int c=m->landmarks[side],dx=x-c%64,dz=z-c/64;
            if(dx*dx+dz*dz<(rx+6)*(rz+6))ok=0;
            c=m->spawns[side];dx=x-c%64;dz=z-c/64;
            if(dx*dx+dz*dz<(rx+5)*(rz+5))ok=0;
        }
        for(int i=0;i<m->lake_count;i++){
            AwLake*l=&m->lakes[i];int dx=x-l->x,dz=z-l->z;
            if(dx*dx+dz*dz<(rx+l->rx+4)*(rz+l->rz+4))ok=0;
        }
        if(pair==2&&(64-2*x)*(64-2*x)+(64-2*z)*(64-2*z)<(rx+rz+5)*(rx+rz+5))ok=0;
        for(int dz=-rz-3;dz<=rz+3&&ok;dz++)for(int dx=-rx-3;dx<=rx+3;dx++){
            int nx=x+dx,nz=z+dz;if(nx<3||nz<3||nx>60||nz>60){ok=0;break;}
            if(dx*dx*(rz+3)*(rz+3)+dz*dz*(rx+3)*(rx+3)>(rx+3)*(rx+3)*(rz+3)*(rz+3))continue;
            if(m->cells[nz*64+nx].road){ok=0;break;}
        }
        if(!ok)continue;
        for(int side=0;side<pair;side++){
            int lx=side?64-x:x,lz=side?64-z:z;
            m->lakes[m->lake_count++]=(AwLake){lx,lz,rx,rz};
            aw_landform(m,lx*2,lz*2,rx+5,rz+5,1,0);
        }
    }
}
static void aw_carve_lakes(AwMap*m){
    for(int i=0;i<m->lake_count;i++){
        AwLake*l=&m->lakes[i];
        for(int dz=-l->rz-2;dz<=l->rz+2;dz++)for(int dx=-l->rx-2;dx<=l->rx+2;dx++){
            int x=l->x+dx,z=l->z+dz,v=z*65+x,cv=m->options.symmetry&&v>2112?4224-v:v;
            int metric=dx*dx*100/(l->rx*l->rx)+dz*dz*100/(l->rz*l->rz);
            if(metric<180&&m->macro_q[v]<4)m->macro_q[v]=4;
            metric+=(aw_noise(m->layout_seed^9137u,cv%65,cv/65,2)-128)/10;
            if(metric<=100){m->macro_q[v]=metric<65?0:2;m->lake_mask[v]=1;}
        }
    }
    /* A two-vertex dry rim closes the basin even at the smallest radius. */
    for(int v=0;v<AW_VERT*AW_VERT;v++)if(m->lake_mask[v]){
        int x=v%65,z=v/65;
        for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
            int nx=x+dx,nz=z+dz;if(nx<2||nz<2||nx>62||nz>62)continue;
            int n=nz*65+nx;if(!m->lake_mask[n]&&m->macro_q[n]<4)m->macro_q[n]=4;
        }
    }
}
static int aw_layout(AwMap*m){
    uint32_t rng=aw_hash(m->layout_seed^0x124731u);
    if(!aw_road(m,0)||!aw_road(m,1))return 0;
    for(int side=0;side<2;side++){
        int c=m->spawns[side];uint32_t h=aw_hash(m->layout_seed^(side&&!m->options.symmetry?7831u:99u));
        aw_landform(m,(c%64)*2+1,(c/64)*2+1,7+h%6,7+(h>>8)%6,side?m->options.floors_b:m->options.floors_a,h%8);
    }
    int extra=2+aw_plan_random(&rng)%6;
    for(int i=0;i<extra;i++){
        int x2=14+aw_plan_random(&rng)%100,z2=14+aw_plan_random(&rng)%(m->options.symmetry?50:100);
        int rx=4+aw_plan_random(&rng)%13,rz=4+aw_plan_random(&rng)%11,height=1+aw_plan_random(&rng)%5,rot=aw_plan_random(&rng)%8;
        aw_landform(m,x2,z2,rx,rz,height,rot);
        if(m->options.symmetry)aw_landform(m,128-x2,128-z2,rx,rz,height,rot);
    }
    for(int side=0;side<2;side++){
        if(side&&m->options.symmetry){m->landmarks[1]=4095-m->landmarks[0];continue;}
        int x=39+aw_plan_random(&rng)%16,z=9+aw_plan_random(&rng)%18,c=z*64+x;
        m->landmarks[side]=side?4095-c:c;
    }
    for(int side=0;side<2;side++){
        int c=m->landmarks[side];uint32_t h=aw_hash(m->layout_seed^(side&&!m->options.symmetry?1471u:77u));
        aw_landform(m,(c%64)*2+1,(c/64)*2+1,8+h%5,8+(h>>8)%5,1,h%8);
    }
    aw_plan_lakes(m,&rng);
    if(!m->lake_count)return 0;
    if(!aw_flat_corridor(m,m->road_ends[0],m->landmarks[0],m->options.symmetry,aw_plan_random(&rng)))return 0;
    if(!m->options.symmetry&&!aw_flat_corridor(m,m->road_ends[1],m->landmarks[1],0,aw_plan_random(&rng)))return 0;
    if(!aw_flat_corridor(m,m->landmarks[0],m->landmarks[1],m->options.symmetry,aw_plan_random(&rng)))return 0;
    for(int v=0;v<AW_VERT*AW_VERT;v++){
        int cv=m->options.symmetry&&v>2112?4224-v:v,x=cv%65,z=cv/65,top=4;
        int land=aw_land_score(m,x,z,&top),q=land< -85?0:land<85?2:top;
        /* The seeded entrance districts have room for actual descending mouths. */
        for(int side=0;side<2;side++){
            int c=m->landmarks[side],dx=x-c%64,dz=z-c/64;
            if(dx*dx+dz*dz<49)q=4;
        }
        /* Connected, irregular shoulders around every route provide land, not
         * isolated road decks over water. Later pin blending meets road grade. */
        for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++){
            int nx=x+dx,nz=z+dz;if(nx<0||nz<0||nx>=64||nz>=64)continue;
            if(m->cells[nz*64+nx].road&&dx*dx+dz*dz<=9&&q<4)q=4;
        }
        if(x<2||z<2||x>62||z>62)q=0;
        m->macro_q[v]=aw_clamp(q,0,40);
    }
    aw_carve_lakes(m);
    return 1;
}
#endif
