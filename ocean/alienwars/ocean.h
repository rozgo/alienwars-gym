#ifndef ALIENWARS_OCEAN_H
#define ALIENWARS_OCEAN_H
/* Separate naval domain: the 64-square land region sits inside a 96-square
 * water grid, sharing world coordinates. Water covers [-16,80) in x/z.
 * This is navigation groundwork, not a ship simulator. The default footprint
 * occupies a whole tile and needs 1 quarter-floor (0.75 world units) of draft. */
static float aw_ocean_bed_q(const AwMap*m,float x,float z){
    float outside=fmaxf(fmaxf(-x,x-AW_SIZE),fmaxf(-z,z-AW_SIZE));
    if(outside<=0)return aw_height_q(m,x,z);
    /* Continuous shelf at the mandatory submerged domain edge. */
    return -fminf(12.0f,outside*2.0f);
}
static int aw_ocean_neighbor(int c,int d){
    int x=c%AW_OCEAN_SIZE,z=c/AW_OCEAN_SIZE;
    if(d==0)return z?c-AW_OCEAN_SIZE:-1;
    if(d==1)return x<AW_OCEAN_SIZE-1?c+1:-1;
    if(d==2)return z<AW_OCEAN_SIZE-1?c+AW_OCEAN_SIZE:-1;
    return x?c-1:-1;
}
static void aw_ocean_build(AwMap*m){
    memset(m->ocean_connected,0,sizeof(m->ocean_connected));m->ocean_count=0;
    for(int c=0;c<AW_OCEAN_CELLS;c++){
        float x=c%AW_OCEAN_SIZE-AW_OCEAN_BELT,z=c/AW_OCEAN_SIZE-AW_OCEAN_BELT,bed=-100;
        for(int dz=0;dz<3;dz++)for(int dx=0;dx<3;dx++)bed=fmaxf(bed,aw_ocean_bed_q(m,x+dx*.5f,z+dz*.5f));
        m->ocean_depth[c]=(uint16_t)aw_clamp((int)lroundf((1.44f-bed)*100),0,65535);
    }
    int queue[AW_OCEAN_CELLS],head=0,tail=0;queue[tail++]=0;m->ocean_connected[0]=1;
    while(head<tail){int c=queue[head++];for(int d=0;d<4;d++){
        int n=aw_ocean_neighbor(c,d);if(n<0||m->ocean_connected[n]||m->ocean_depth[n]<100)continue;
        m->ocean_connected[n]=1;queue[tail++]=n;
    }}
    m->ocean_count=tail;
}
/* A draft-sensitive route query, with explicit output capacity and no surface
 * or cave node aliasing. Lakes never acquire ocean access through a tunnel. */
static int aw_ocean_path(const AwMap*m,int start,int goal,int draft,int*out,int capacity){
    if(start<0||goal<0||start>=AW_OCEAN_CELLS||goal>=AW_OCEAN_CELLS||draft<100||capacity<1||!out)return 0;
    if(!m->ocean_connected[start]||!m->ocean_connected[goal]||m->ocean_depth[start]<draft||m->ocean_depth[goal]<draft)return 0;
    int prev[AW_OCEAN_CELLS],queue[AW_OCEAN_CELLS],head=0,tail=0;
    for(int i=0;i<AW_OCEAN_CELLS;i++)prev[i]=-1;prev[start]=start;queue[tail++]=start;
    while(head<tail&&prev[goal]<0){int c=queue[head++];for(int d=0;d<4;d++){
        int n=aw_ocean_neighbor(c,d);if(n<0||prev[n]>=0||!m->ocean_connected[n]||m->ocean_depth[n]<draft)continue;
        prev[n]=c;queue[tail++]=n;
    }}
    if(prev[goal]<0)return 0;int length=1;
    for(int c=goal;c!=start;c=prev[c])length++;
    if(length>capacity)return 0;
    for(int c=goal,i=length-1;i>=0;i--){out[i]=c;c=prev[c];}
    return length;
}
#endif
