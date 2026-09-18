#ifndef ALIENWARS_BATHYMETRY_H
#define ALIENWARS_BATHYMETRY_H
/* Reset-time bathymetry from actual WFC coast sockets. No RNG, heap ownership or
 * step-time search. Wet edge-connected vertices form the sea; enclosed lake
 * beds stay unchanged. Integer squared Euclidean distance avoids a Manhattan
 * diamond and makes mirrored layouts exactly equal. */
static void aw_bathymetry_build(AwMap*m){
#if AW_VERSION >= 13
    enum {N=AW_OCEAN_VERT,COUNT=N*N,INF=100000};
    int along[COUNT],distance[COUNT],queue[COUNT];uint8_t wet[COUNT],sea[COUNT]={0};
    memset(m->shelf_drop,0,sizeof(m->shelf_drop));
    for(int z=0;z<N;z++)for(int x=0;x<N;x++){
        int gx=x-AW_OCEAN_BELT,gz=z-AW_OCEAN_BELT,q=0;
        if(gx>=0&&gx<=AW_SIZE&&gz>=0&&gz<=AW_SIZE){
            int cx=aw_clamp(gx,0,AW_SIZE-1),cz=aw_clamp(gz,0,AW_SIZE-1);
            int k=gz>cz?(gx>cx?2:3):(gx>cx?1:0);q=m->cells[cz*AW_SIZE+cx].q[k];
        }
        wet[z*N+x]=q==0;
    }
    /* Separable exact transform on this small fixed grid: ~1.8M integer
     * comparisons per reset, independent of the number/complexity of coasts. */
    for(int z=0;z<N;z++)for(int x=0;x<N;x++){
        int best=INF;for(int k=0;k<N;k++)if(!wet[z*N+k]){int d=(x-k)*(x-k);if(d<best)best=d;}
        along[z*N+x]=best;
    }
    for(int z=0;z<N;z++)for(int x=0;x<N;x++){
        int best=INF;for(int k=0;k<N;k++){int d=along[k*N+x]+(z-k)*(z-k);if(d<best)best=d;}
        distance[z*N+x]=best;
    }
    int head=0,tail=0;
    for(int z=0;z<N;z++)for(int x=0;x<N;x++)if((x==0||z==0||x==N-1||z==N-1)&&wet[z*N+x]){
        int i=z*N+x;if(!sea[i]){sea[i]=1;queue[tail++]=i;}
    }
    while(head<tail){int i=queue[head++],x=i%N,z=i/N;
        int neighbors[4]={z?i-N:-1,x<N-1?i+1:-1,z<N-1?i+N:-1,x?i-1:-1};
        for(int k=0;k<4;k++){int n=neighbors[k];if(n>=0&&wet[n]&&!sea[n]){sea[n]=1;queue[tail++]=n;}}
    }
    for(int i=0;i<COUNT;i++)if(sea[i]){
        /* Keep the first submerged tile intact; descend gently over the next
         * eight tiles, then meet the same deep-ocean datum everywhere. */
        float t=fminf(1,fmaxf(0,(sqrtf((float)distance[i])-1)/8));
        m->shelf_drop[i]=(uint16_t)lroundf(1200*t*t*(3-2*t));
    }
#else
    (void)m;
#endif
}
static float aw_shelf_drop_q(const AwMap*m,float x,float z){
#if AW_VERSION >= 13
    x=fminf(AW_OCEAN_SIZE,fmaxf(0,x+AW_OCEAN_BELT));
    z=fminf(AW_OCEAN_SIZE,fmaxf(0,z+AW_OCEAN_BELT));
    int ix=aw_clamp((int)x,0,AW_OCEAN_SIZE-1),iz=aw_clamp((int)z,0,AW_OCEAN_SIZE-1),i=iz*AW_OCEAN_VERT+ix;
    float a=m->shelf_drop[i],b=m->shelf_drop[i+1],c=m->shelf_drop[i+1+AW_OCEAN_VERT],d=m->shelf_drop[i+AW_OCEAN_VERT],u=x-ix,v=z-iz;
    /* Same 0→2 diagonal as the land subdivision and the outer ocean mesh. */
    return (u>=v?a+(b-a)*u+(c-b)*v:a+(c-d)*u+(d-a)*v)*.01f;
#else
    (void)m;(void)x;(void)z;return 0;
#endif
}
#endif
