#ifndef ALIENWARS_CAVES_H
#define ALIENWARS_CAVES_H
/* Continuous CSG caves. Graph coordinates use cell centers and signed quarter
 * floors. No fixed layer number, rectangular roof, or terrain-height override. */
static int aw_node_cell(const AwMap*m,int node){
    if(node<AW_CELLS)return node;
    if(node>=AW_SPAN_START)return m->spans[node-AW_SPAN_START].cell;
    const AwCaveNode*n=&m->cave[node-AW_CELLS];return n->z*AW_SIZE+n->x;
}
static float aw_height_q(const AwMap*m,float x,float z){
    x=fminf(AW_SIZE-0.0001f,fmaxf(0,x));z=fminf(AW_SIZE-0.0001f,fmaxf(0,z));
    return aw_surface_q(m,(int)z*AW_SIZE+(int)x,x-floorf(x),z-floorf(z));
}
#include "mountain.h"
static float aw_cave_radius(int profile){return 0.85f+0.18f*profile;}
static float aw_cave_height(int profile){return 4.0f+0.5f*profile;}
static int aw_cave_node(AwMap*m,int x,int z,int q){
    for(int i=0;i<m->cave_count;i++)if(m->cave[i].x==x&&m->cave[i].z==z&&m->cave[i].q==q)return i;
    if(m->cave_count>=AW_CAVE_NODES)return -1;
    int i=m->cave_count++;m->cave[i]=(AwCaveNode){.x=x,.z=z,.q=q,.portal=-1};
    for(int d=0;d<6;d++)m->cave[i].links[d]=-1;return i;
}
static int aw_cave_link(AwMap*m,int a,int b){
    if(a<0||b<0||a==b)return 0;
    int da=-1,db=-1;
    for(int d=0;d<6;d++){
        if(m->cave[a].links[d]==b)return 1;
        if(m->cave[a].links[d]<0)da=d;if(m->cave[b].links[d]<0)db=d;
    }
    if(da<0||db<0||m->cave_edge_count>=AW_CAVE_EDGES)return 0;
    m->cave[a].links[da]=b;m->cave[b].links[db]=a;
    m->cave_edges[m->cave_edge_count++]=(AwCaveEdge){a,b};return 1;
}
/* A* in (x,z,elevation), with cardinal flat/ramp moves. Grade, dry land and
 * overburden are hard constraints. Excavation depth and seeded spatial cost
 * bias efficient, varied routes. The admissible heuristic ignores those costs.
 * Heading and prior grade are part of the state so turn rules remain Markov.
 * The search has a bounded volume; failure is explicit, never a straight-line
 * or rectangular fallback. All scratch storage is per generation. */
static int aw_cave_route(AwMap*m,int sx,int sz,int sq,int gx,int gz,int gq,int mirror){
    if(sx<0||sx>=64||sz<0||sz>=64||gx<0||gx>=64||gz<0||gz>=64||sq < -24||gq < -24||sq>40||gq>40)return 0;
    if(sx==gx&&sz==gz&&sq==gq){int a=aw_cave_node(m,sx,sz,sq);int b=mirror?aw_cave_node(m,63-sx,63-sz,sq):a;return a>=0&&b>=0;}
    int qlo=(sq<gq?sq:gq),qhi=(sq>gq?sq:gq),levels=qhi-qlo+1;
    int n=AW_CELLS*levels*13;
    int *dist=malloc((size_t)n*sizeof(int)),*prev=malloc((size_t)n*sizeof(int));
    int *heap=malloc((size_t)n*sizeof(int)),*pos=malloc((size_t)n*sizeof(int)),*score=malloc((size_t)n*sizeof(int));
    if(!dist||!prev||!heap||!pos||!score){free(dist);free(prev);free(heap);free(pos);free(score);return 0;}
    int start=((sq-qlo)*AW_CELLS+sz*AW_SIZE+sx)*13+12,goal=(gq-qlo)*AW_CELLS+gz*AW_SIZE+gx,size=1,found=0,last_state=-1;
    for(int i=0;i<n;i++){dist[i]=INT_MAX;prev[i]=-1;pos[i]=-1;}
    dist[start]=score[start]=0;heap[0]=start;pos[start]=0;
    while(size){
        int c=heap[0];pos[c]=-2;size--;
        if(size){heap[0]=heap[size];pos[heap[0]]=0;int p=0;
            for(;;){int a=p*2+1;if(a>=size)break;if(a+1<size&&score[heap[a+1]]<score[heap[a]])a++;
                if(score[heap[p]]<=score[heap[a]])break;int v=heap[p];heap[p]=heap[a];heap[a]=v;pos[heap[p]]=p;pos[v]=a;p=a;}}
        m->cave_expanded++;if(c/13==goal&&(c%13)%3==1){found=1;last_state=c;break;}
        int base=c/13,cell=base%AW_CELLS,q=base/AW_CELLS+qlo,state=c%13;
        int olddir=state==12?-1:state/3,oldrise=state==12?0:state%3-1;
        for(int d=0;d<4;d++)for(int rise=-1;rise<=1;rise++){
            if((olddir<0&&rise)||(olddir>=0&&d!=olddir&&(oldrise||rise)))continue;
            if(olddir>=0&&d==(olddir+2)%4)continue;
            int nc=aw_neighbor(cell,d),nq=q+rise;if(nc<0||nq<qlo||nq>qhi)continue;
            int nx=nc%AW_SIZE,nz=nc/AW_SIZE,next=((nq-qlo)*AW_CELLS+nc)*13+d*3+rise+1;
            if(pos[next]==-2||nx<3||nz<3||nx>60||nz>60)continue;
            float h=aw_surface_q(m,nc,0.5f,0.5f);
            int near=aw_abs(nx-sx)+aw_abs(nz-sz);
            if(h<4 || (nq+6>h && near>8))continue;
            int wet=0;
            for(int l=0;l<m->lake_count;l++){
                const AwLake*lake=&m->lakes[l];int dx=2*nx+1-2*lake->x,dz=2*nz+1-2*lake->z,rx=2*(lake->rx+2),rz=2*(lake->rz+2);
                if(dx*dx*rz*rz+dz*dz*rx*rx<=rx*rx*rz*rz)wet=1;
            }
            if(wet)continue;
            if((m->cells[nc].road||m->cave_access[nc])&&nq<h&&nq+6>h)continue;
            int trail_support=1;
            for(int j=0;j<m->trail_bin_count[nc];j++){
                const AwTrailEdge*e=&m->trail_edges[m->trail_bins[nc][j]];
                const AwTrailNode*a=&m->trail[e->a],*b=&m->trail[e->b];
                int floor=a->q<b->q?a->q:b->q;
                if(nq<floor&&nq+7>floor){trail_support=0;break;}
            }
            if(!trail_support)continue;
            int separated=1;
            for(int j=0;j<m->cave_count;j++){
                const AwCaveNode*v=&m->cave[j];int dx=nx-v->x,dz=nz-v->z,dq=aw_abs(nq-v->q);
                /* Keep existing ramps and their support out of the new sweep.
                 * Equal-elevation junctions may merge; stacked spans need cover. */
                if(dq>0&&dq<7&&dx*dx+dz*dz<4){separated=0;break;}
            }
            if(!separated)continue;
            int excavation=aw_clamp((int)h-nq-6,0,40)/8;
            int canonical=m->options.symmetry&&nc>=AW_CELLS/2?AW_CELLS-1-nc:nc;
            int cost=dist[c]+10+excavation+(aw_hash(m->seed^(uint32_t)canonical*71u)%4);
            if(cost>=dist[next])continue;
            dist[next]=cost;prev[next]=c;
            int horizontal=aw_abs(nx-gx)+aw_abs(nz-gz),vertical=aw_abs(nq-gq);
            score[next]=cost+10*(horizontal>vertical?horizontal:vertical);
            int p=pos[next];if(p<0){p=size;heap[size++]=next;pos[next]=p;}
            while(p){int parent=(p-1)/2;if(score[heap[parent]]<=score[next])break;heap[p]=heap[parent];pos[heap[p]]=p;p=parent;}heap[p]=next;pos[next]=p;
        }
    }
    if(found){
        int last=-1,other=-1;
        for(int c=last_state;c>=0;c=prev[c]){
            int cell=(c/13)%AW_CELLS,x=cell%AW_SIZE,z=cell/AW_SIZE,q=(c/13)/AW_CELLS+qlo;
            int node=aw_cave_node(m,x,z,q);
            if(node<0||(last>=0&&!aw_cave_link(m,last,node))){found=0;break;}last=node;
            if(mirror){int node2=aw_cave_node(m,63-x,63-z,q);if(node2<0||(other>=0&&!aw_cave_link(m,other,node2))){found=0;break;}other=node2;}
        }
    }
    free(dist);free(prev);free(heap);free(pos);free(score);return found;
}
/* WFC profiles are complete arch sockets, shared at graph vertices. Sweeps
 * interpolate matching endpoints. Narrow/standard/wide/chamber choices propagate
 * grade, maximum one-size transitions, portal pins, cover, and rotational pairs.
 * MRV collapse has bounded backtracking; no invalid geometry is repaired later. */
static int aw_cave_propagate(const AwMap*m,uint8_t*wave){
    int changed=1;
    while(changed){changed=0;
        for(int e=0;e<m->cave_edge_count;e++)for(int reverse=0;reverse<2;reverse++){
            int a=reverse?m->cave_edges[e].b:m->cave_edges[e].a,b=reverse?m->cave_edges[e].a:m->cave_edges[e].b;
            int allowed=0;for(int t=0;t<4;t++)if(wave[a]&(1<<t))allowed|=(1<<t)|(t?1<<(t-1):0)|(t<3?1<<(t+1):0);
            int bits=wave[b]&allowed;if(!bits)return 0;if(bits!=wave[b]){wave[b]=bits;changed=1;}
        }
        if(m->options.symmetry)for(int a=0;a<m->cave_count;a++)for(int b=a+1;b<m->cave_count;b++){
            const AwCaveNode*x=&m->cave[a],*y=&m->cave[b];
            if(x->x+y->x!=63||x->z+y->z!=63||x->q!=y->q)continue;
            int bits=wave[a]&wave[b];if(!bits)return 0;if(wave[a]!=bits||wave[b]!=bits){wave[a]=wave[b]=bits;changed=1;}
        }
    }return 1;
}
static int aw_cave_collapse(AwMap*m,uint8_t*wave,int depth){
    if(depth>AW_CAVE_NODES||m->cave_backtracks>4096||!aw_cave_propagate(m,wave))return 0;
    int best=-1,entropy=99;
    for(int i=0;i<m->cave_count;i++){int n=__builtin_popcount(wave[i]);if(n>1&&n<entropy){best=i;entropy=n;}}
    if(best<0)return 1;
    uint8_t snapshot[AW_CAVE_NODES];memcpy(snapshot,wave,(size_t)m->cave_count);
    int first=aw_random(m)%4;
    for(int k=0;k<4;k++){int t=(first+k)%4;if(!(snapshot[best]&(1<<t)))continue;
        memcpy(wave,snapshot,(size_t)m->cave_count);wave[best]=1<<t;m->cave_decisions++;
        if(aw_cave_collapse(m,wave,depth+1))return 1;m->cave_backtracks++;
    }return 0;
}
static int aw_cave_index(AwMap*m){
    memset(m->cave_bin_count,0,sizeof(m->cave_bin_count));
    for(int i=0;i<m->cave_count;i++){
        AwCaveNode*a=&m->cave[i];a->junction=1;
        for(int d=0;d<6;d++)if(a->links[d]>=0&&m->cave[a->links[d]].q!=a->q)a->junction=0;
    }
    for(int e=0;e<m->cave_edge_count;e++){
        const AwCaveNode*a=&m->cave[m->cave_edges[e].a],*b=&m->cave[m->cave_edges[e].b];
        int x0=aw_clamp((a->x<b->x?a->x:b->x)-2,0,63),x1=aw_clamp((a->x>b->x?a->x:b->x)+2,0,63);
        int z0=aw_clamp((a->z<b->z?a->z:b->z)-2,0,63),z1=aw_clamp((a->z>b->z?a->z:b->z)+2,0,63);
        for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++){
            int c=z*AW_SIZE+x,n=m->cave_bin_count[c];if(n>=AW_BIN_SIZE)return 0;
            m->cave_bins[c][n]=e;m->cave_bin_count[c]++;
        }
    }return 1;
}
/* Negative inside an arched, flat-bottomed sweep. This is an implicit field,
 * not an exact signed distance: do not sphere-trace it as if it were one. */
static float aw_cave_field(const AwMap*m,float x,float yq,float z){
    int c=aw_clamp((int)z,0,63)*AW_SIZE+aw_clamp((int)x,0,63);float result=1000;
    for(int i=0;i<m->cave_bin_count[c];i++){
        const AwCaveEdge*e=&m->cave_edges[m->cave_bins[c][i]];
        const AwCaveNode*a=&m->cave[e->a],*b=&m->cave[e->b];
        float dx=b->x-a->x,dz=b->z-a->z,den=dx*dx+dz*dz;
        float along=((x-a->x-0.5f)*dx+(z-a->z-0.5f)*dz)/den;
        float t=fminf(1,fmaxf(0,along));
        float cx=a->x+0.5f+dx*t,cz=a->z+0.5f+dz*t;
        float floor=aw_lerp(a->q,b->q,along),radius=aw_lerp(aw_cave_radius(a->profile),aw_cave_radius(b->profile),t);
        float height=aw_lerp(aw_cave_height(a->profile),aw_cave_height(b->profile),t);
        float ex=(x-cx)/radius,ez=(z-cz)/radius,ey=fmaxf(0,yq-floor-2)/(height-2);
        /* Overlap section ends to keep internal join planes strictly empty.
         * Extrapolated floor planes match through straight ramp sockets. */
        float field=fmaxf(fmaxf(floor-yq,(ex*ex+ez*ez+ey*ey-1)*2),fmaxf(-along-0.25f,along-1.25f)*2);
        result=fminf(result,field);
        for(int end=0;end<2;end++){
            const AwCaveNode*v=end?b:a;if(!v->junction)continue;
            float r=aw_cave_radius(v->profile),h=aw_cave_height(v->profile);
            float vx=(x-v->x-0.5f)/r,vz=(z-v->z-0.5f)/r,vy=fmaxf(0,yq-v->q-2)/(h-2);
            result=fminf(result,fmaxf(v->q-yq,(vx*vx+vz*vz+vy*vy-1)*2));
        }
    }return result;
}
static float aw_density_at_height(const AwMap*m,float x,float q,float z,float height){
    float rock=height-q;
    if(m->trail_edge_count)rock=fminf(rock,aw_mountain_field(m,x,q,z));
    return m->cave_edge_count?fminf(rock,aw_cave_field(m,x,q,z)):rock;
}
/* Barycentric sampling of the SAME six tetrahedra used by the mesher. This
 * makes collision/clearance agree with polygonal cave walls, not an unsampled
 * analytic approximation. The sorted fractional axes select a Freudenthal tet. */
static float aw_density(const AwMap*m,float x,float q,float z){
    float coord[3]={x*AW_SUBDIV,q*2,z*AW_SUBDIV},fraction[3];int lattice[3],axis[3]={0,1,2};
    for(int i=0;i<3;i++){lattice[i]=(int)floorf(coord[i]);fraction[i]=coord[i]-lattice[i];}
    for(int i=0;i<2;i++)for(int j=i+1;j<3;j++)if(fraction[axis[j]]>fraction[axis[i]]){int t=axis[i];axis[i]=axis[j];axis[j]=t;}
    float value[4];
    for(int i=0;i<4;i++){
        float gx=(float)lattice[0]/AW_SUBDIV,gq=lattice[1]*0.5f,gz=(float)lattice[2]/AW_SUBDIV;
        if(m->density_cache){
            uint32_t h=aw_hash((uint32_t)lattice[0]*73856093u^(uint32_t)lattice[1]*19349663u^(uint32_t)lattice[2]*83492791u)&(AW_DENSITY_CACHE-1);
            AwDensitySample*s=&m->density_cache[h];
            if(!s->valid||s->x!=lattice[0]||s->q!=lattice[1]||s->z!=lattice[2]){
                *s=(AwDensitySample){lattice[0],lattice[1],lattice[2],1,aw_density_at_height(m,gx,gq,gz,aw_height_q(m,gx,gz))};
            }value[i]=s->value;
        }else value[i]=aw_density_at_height(m,gx,gq,gz,aw_height_q(m,gx,gz));
        if(i<3)lattice[axis[i]]++;
    }
    float a=fraction[axis[0]],b=fraction[axis[1]],c=fraction[axis[2]];
    return value[0]*(1-a)+value[1]*(a-b)+value[2]*(b-c)+value[3]*c;
}
/* Locate support on the composed field, including junction bevels, rather than
 * assuming that a route's nominal elevation survived Boolean excavation. */
static float aw_support_q(const AwMap*m,float x,float z,float hint){
    float top=hint+0.8f,prev=top;
    for(int i=1;i<=24;i++){
        float q=top-i*0.1f;
        if(aw_density(m,x,q,z)>0){float lo=q,hi=prev;for(int j=0;j<12;j++){float mid=(lo+hi)*0.5f;if(aw_density(m,x,mid,z)>0)lo=mid;else hi=mid;}return (lo+hi)*0.5f;}
        prev=q;
    }return hint-100;
}
/* Conservative route clearance validation samples the actual field around a
 * scout-sized body, including ramp joins. Surface and underground graphs meet
 * only at declared entrances; stacked crossings do not gain phantom links. */
static int aw_cave_validate(const AwMap*m){
    if(m->cave_count<0||m->cave_count>AW_CAVE_NODES||m->cave_edge_count<0||m->cave_edge_count>AW_CAVE_EDGES)return 0;
    if(!m->options.tunnels)return m->cave_count==0&&m->cave_edge_count==0&&m->cave_room_count==0;
    if(m->cave_room_count!=0&&m->cave_room_count!=2)return 0;
    for(int i=0;i<m->cave_room_count;i++){
        int r=m->cave_rooms[i];if(r<0||r>=m->cave_count||m->cave[r].profile!=2||m->cave[r].q>=0)return 0;
    }
    int portals=0;
    for(int i=0;i<m->cave_count;i++){
        const AwCaveNode*n=&m->cave[i];
        if(n->x<0||n->x>=64||n->z<0||n->z>=64||n->q < -24||n->q>40||n->profile>3)return 0;
        if(n->portal < -1)return 0;
        if(n->portal>=0){if(n->portal!=n->z*64+n->x||!m->cells[n->portal].portal||fabsf(aw_surface_q(m,n->portal,.5f,.5f)-n->q)>.01f)return 0;portals++;}
        for(int d=0;d<6;d++){
            int next=n->links[d];if(next < -1||next>=m->cave_count||next==i)return 0;
            if(next<0)continue;int registered=0;
            for(int e=0;e<m->cave_edge_count;e++)registered+=(m->cave_edges[e].a==i&&m->cave_edges[e].b==next)||(m->cave_edges[e].b==i&&m->cave_edges[e].a==next);
            if(registered!=1)return 0;
        }
    }
    if(portals!=2||m->cave_entrances[0]==m->cave_entrances[1]||m->cave_hubs[0]==m->cave_hubs[1])return 0;
    for(int side=0;side<2;side++){
        int p=m->cave_entrances[side],h=m->cave_hubs[side];
        if(p<0||p>=m->cave_count||h<0||h>=m->cave_count)return 0;
        const AwCaveNode*n=&m->cave[p],*hub=&m->cave[h];
        int x=side?63-n->x:n->x,z=side?63-n->z:n->z;
        if(n->portal<0||n->q!=4||x<35||x>58||z<5||z>29||hub->q>=0||hub->profile!=2)return 0;
    }
    uint8_t seen[AW_CAVE_NODES]={0};int queue[AW_CAVE_NODES],head=0,tail=0;
    queue[tail++]=m->cave_entrances[0];seen[queue[0]]=1;
    while(head<tail){int c=queue[head++];for(int d=0;d<6;d++){
        int n=m->cave[c].links[d];if(n>=0&&!seen[n]){seen[n]=1;queue[tail++]=n;}
    }}
    if(tail!=m->cave_count)return 0;
    for(int i=0;i<m->cave_edge_count;i++){
        const AwCaveEdge*e=&m->cave_edges[i];if(e->a<0||e->b<0||e->a>=m->cave_count||e->b>=m->cave_count||e->a==e->b)return 0;
        const AwCaveNode*a=&m->cave[e->a],*b=&m->cave[e->b];
        if(aw_abs(a->x-b->x)+aw_abs(a->z-b->z)!=1||aw_abs(a->q-b->q)>1||aw_abs(a->profile-b->profile)>1)return 0;
        int ab=0,ba=0;for(int d=0;d<6;d++){ab+=a->links[d]==e->b;ba+=b->links[d]==e->a;}if(ab!=1||ba!=1)return 0;
        float previous=0;
        for(int step=0;step<=8;step++){
            float t=step/8.0f,x=aw_lerp(a->x,b->x,t)+0.5f,z=aw_lerp(a->z,b->z,t)+0.5f,q=aw_lerp(a->q,b->q,t);
            float support=aw_support_q(m,x,z,q);
            if(fabsf(support-q)>0.8f || (step&&fabsf(support-previous)>0.26f))return 0;
            previous=support;
            for(int side=0;side<5;side++){
                float dx=side==1?0.3f:side==2?-0.3f:0,dz=side==3?0.3f:side==4?-0.3f:0;
                if(aw_density(m,x+dx,support+1.8f,z+dz)>0||aw_density(m,x+dx,support+0.5f,z+dz)>0)return 0;
            }

        }
    }return 1;
}
/* Entrance sites belong to the surface component shared by the bases. Rank
 * flat, dry sites on the OTHER diagonal by regional position, approach space
 * and a seeded preference. Retry ranks explore alternatives on the same world. */
static int aw_cave_site(const AwMap*m,int side,int rank){
    int sites[AW_CELLS],scores[AW_CELLS],count=0;
    uint32_t salt=aw_hash(m->layout_seed^(side?0x729a51u:0xa13b72u));
    int landmark=side?4095-m->landmarks[side]:m->landmarks[side];
    int tx=landmark%64+(int)(salt%7)-3,tz=landmark/64+(int)((salt>>8)%7)-3;
    for(int z=5;z<=29;z++)for(int x=35;x<=58;x++){
        int canonical=z*64+x,c=side?AW_CELLS-1-canonical:canonical;
        const AwCell*t=&m->cells[c];
        if(!m->reachable[c]||(t->road&&!m->cave_access[c])||t->material==AW_SHALLOW||t->q[0]!=4)continue;
        int flat=1,approaches=0;
        for(int k=1;k<4;k++)flat&=t->q[k]==4;
        for(int d=0;d<4;d++){int n=aw_neighbor(c,d);if(n>=0&&m->reachable[n]&&aw_edge_matches(m,c,n,d))approaches++;}
        if(!flat||approaches<2)continue;
        int score=4*(aw_abs(x-tx)+aw_abs(z-tz))-3*approaches+(int)(aw_hash(salt^(uint32_t)c*71u)%32);
        int at=count;while(at&&score<scores[at-1]){sites[at]=sites[at-1];scores[at]=scores[at-1];at--;}
        sites[at]=c;scores[at]=score;count++;
    }
    return count?sites[rank%count]:-1;
}
static void aw_cave_clear(AwMap*m){
    m->span_count=m->span_ready=0;
    m->cave_count=m->cave_edge_count=m->cave_decisions=m->cave_expanded=m->cave_backtracks=m->cave_room_count=0;
    memset(m->cave_bin_count,0,sizeof(m->cave_bin_count));
    for(int c=0;c<AW_CELLS;c++)m->cells[c].tunnel=m->cells[c].portal=0;
    for(int s=0;s<2;s++)m->cave_entrances[s]=m->cave_hubs[s]=-1;
}
/* Choose a covered rendezvous in each half, inward from its entrance. The
 * central connection searches between these sites; there is no fixed center
 * shaft, ring, tunnel start, or prescribed Manhattan connector. */
static int aw_cave_hub_cell(const AwMap*m,int portal,int side,uint32_t salt){
    int x=portal%64,z=portal/64;
    int tx=x+(side?1:-1)*(6+(salt%5)),tz=z+(side?-1:1)*(4+((salt>>8)%6));
    int best=-1,score=INT_MAX;
    for(int dz=-4;dz<=4;dz++)for(int dx=-4;dx<=4;dx++){
        int nx=tx+dx,nz=tz+dz;if(nx<5||nx>58||nz<5||nz>58)continue;
        int c=nz*64+nx;if(!m->reachable[c]||aw_surface_q(m,c,.5f,.5f)<4)continue;
        int cost=8*(aw_abs(dx)+aw_abs(dz))+(aw_hash(salt^(uint32_t)c)%7);
        if(cost<score){score=cost;best=c;}
    }
    return best;
}
/* Optional extra chambers turn a crossing into a seeded multi-leg network.
 * Sites must be dry, connected land, apart from the primary chambers and from
 * each other. Their depths and arch sockets are still validated by passage WFC. */
static int aw_cave_room_site(const AwMap*m,int a,int b,int exclude,uint32_t salt){
    int tx=13+salt%38,tz=13+(salt>>8)%38,best=-1,score=INT_MAX;
    for(int z=9;z<=54;z++)for(int x=9;x<=54;x++){
        int c=z*64+x;if(!m->reachable[c]||aw_surface_q(m,c,.5f,.5f)<4)continue;
        if(aw_abs(x-a%64)+aw_abs(z-a/64)<8||aw_abs(x-b%64)+aw_abs(z-b/64)<8)continue;
        if(exclude>=0&&aw_abs(x-exclude%64)+aw_abs(z-exclude/64)<8)continue;
        int cost=8*(aw_abs(x-tx)+aw_abs(z-tz))+(aw_hash(salt^(uint32_t)c)%13);
        if(cost<score){score=cost;best=c;}
    }
    return best;
}
static int aw_caves(AwMap*m,int attempt){
    if(!m->options.tunnels)return 1;
    int portals[2],hubs[2],depth[2];
    uint32_t plan=aw_hash(m->layout_seed^((uint32_t)attempt*0x9e3779b9u)^0x174b39u);
    portals[0]=aw_cave_site(m,0,attempt);
    portals[1]=m->options.symmetry?AW_CELLS-1-portals[0]:aw_cave_site(m,1,attempt*3);
    if(portals[0]<0||portals[1]<0||portals[1]>=AW_CELLS)return 0;
    hubs[0]=aw_cave_hub_cell(m,portals[0],0,plan);
    hubs[1]=m->options.symmetry?AW_CELLS-1-hubs[0]:aw_cave_hub_cell(m,portals[1],1,aw_hash(plan^917u));
    if(hubs[0]<0||hubs[1]<0||hubs[1]>=AW_CELLS||hubs[0]==hubs[1])return 0;
    depth[0]=-4-(int)(plan%7);depth[1]=m->options.symmetry?depth[0]:-4-(int)((plan>>8)%7);
    if(!aw_cave_route(m,portals[0]%64,portals[0]/64,4,hubs[0]%64,hubs[0]/64,depth[0],m->options.symmetry))return 0;
    if(!m->options.symmetry&&!aw_cave_route(m,portals[1]%64,portals[1]/64,4,hubs[1]%64,hubs[1]/64,depth[1],0))return 0;
    if((plan>>19)&1){
        int rooms[2],q=depth[0]<depth[1]?depth[0]:depth[1];
        rooms[0]=aw_cave_room_site(m,hubs[0],hubs[1],-1,aw_hash(plan^571u));
        rooms[1]=m->options.symmetry?4095-rooms[0]:aw_cave_room_site(m,hubs[0],hubs[1],rooms[0],aw_hash(plan^919u));
        if(rooms[0]<0||rooms[1]<0||rooms[1]>=AW_CELLS||rooms[0]==rooms[1])return 0;
        if(!aw_cave_route(m,hubs[0]%64,hubs[0]/64,depth[0],rooms[0]%64,rooms[0]/64,q,m->options.symmetry))return 0;
        if(!m->options.symmetry&&!aw_cave_route(m,rooms[1]%64,rooms[1]/64,q,hubs[1]%64,hubs[1]/64,depth[1],0))return 0;
        if(!aw_cave_route(m,rooms[0]%64,rooms[0]/64,q,rooms[1]%64,rooms[1]/64,q,m->options.symmetry))return 0;
        for(int i=0;i<2;i++)m->cave_rooms[m->cave_room_count++]=aw_cave_node(m,rooms[i]%64,rooms[i]/64,q);
    }else if(!aw_cave_route(m,hubs[0]%64,hubs[0]/64,depth[0],hubs[1]%64,hubs[1]/64,depth[1],m->options.symmetry))return 0;
    for(int side=0;side<2;side++){
        int p=aw_cave_node(m,portals[side]%64,portals[side]/64,4),h=aw_cave_node(m,hubs[side]%64,hubs[side]/64,depth[side]);
        if(p<0||h<0)return 0;
        m->cave[p].portal=portals[side];m->cells[portals[side]].portal=1;
        m->cave_entrances[side]=p;m->cave_hubs[side]=h;
    }
    uint8_t wave[AW_CAVE_NODES];
    for(int i=0;i<m->cave_count;i++){
        AwCaveNode*n=&m->cave[i];wave[i]=15;
        if(n->portal>=0)wave[i]=1;
        if(i==m->cave_hubs[0]||i==m->cave_hubs[1])wave[i]=1<<2;
        for(int r=0;r<m->cave_room_count;r++)if(i==m->cave_rooms[r])wave[i]=1<<2;
        for(int d=0;d<6;d++)if(n->links[d]>=0&&n->q!=m->cave[n->links[d]].q)wave[i]&=3;
        int low=n->q;
        for(int d=0;d<6;d++)if(n->links[d]>=0&&m->cave[n->links[d]].q<low)low=m->cave[n->links[d]].q;
        /* Profile domains protect road-lane support around the whole sweep,
         * including the flat socket immediately before a descending ramp. */
        for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
            int x=n->x+dx,z=n->z+dz;if(x<0||z<0||x>=64||z>=64)continue;
            int c=z*64+x;if(!m->cells[c].road&&!m->cave_access[c])continue;
            float road=aw_surface_q(m,c,0.5f,0.5f),distance=sqrtf((float)(dx*dx+dz*dz));
            if(low>=road)continue;
            for(int t=0;t<4;t++)if(n->q+aw_cave_height(t)>road&&aw_cave_radius(t)>distance-0.1f)wave[i]&=~(1<<t);
        }
        float cover=aw_height_q(m,n->x+0.5f,n->z+0.5f)-n->q;
        int approach=0;for(int side=0;side<2;side++)approach|=aw_abs(n->x-portals[side]%64)+aw_abs(n->z-portals[side]/64)<=8;
        if(!approach)for(int t=0;t<4;t++)if(aw_cave_height(t)+1.5f>cover)wave[i]&=~(1<<t);
        if(!wave[i])return 0;
    }
    if(!aw_cave_collapse(m,wave,0))return 0;
    for(int i=0;i<m->cave_count;i++){m->cave[i].profile=__builtin_ctz(wave[i]);m->cells[m->cave[i].z*64+m->cave[i].x].tunnel=1;}
    return aw_cave_index(m);
}
#endif
