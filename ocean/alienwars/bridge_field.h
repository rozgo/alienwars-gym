#ifndef ALIENWARS_BRIDGE_FIELD_H
#define ALIENWARS_BRIDGE_FIELD_H
/* Bridges add solid deck to the same lattice used by terrain meshing and
 * collision. The original height field remains the water bed below the deck. */
static float aw_bridge_q(const AwBridge*b,float u){
    return fminf(b->crown,fminf(b->qa+fmaxf(0,u-1),b->qb+fmaxf(0,b->length-u-1)));
}
static float aw_bridge_field(const AwMap*m,float x,float q,float z){
    int c=aw_clamp((int)z,0,63)*64+aw_clamp((int)x,0,63),id=m->bridge_bins[c];
    if(!id)return -1000;
    const AwBridge*b=&m->bridges[id-1];
    float dx=x-b->x-.5f,dz=z-b->z-.5f,u=dx*b->dx+dz*b->dz,v=dx*b->dz-dz*b->dx;
    float top=aw_bridge_q(b,u);
    return fminf(fminf(u+.65f,b->length+.65f-u),fminf(1.16f-fabsf(v),fminf(top-q,q-(top-1.25f))));
}
/* A compact weld at abutments prevents a sub-lattice air sliver where a
 * rising underside meets the bank. Clamp to the planned top, so no seam lip
 * changes support or road grades. The open span keeps its full clearance. */
static float aw_bridge_union(const AwMap*m,float x,float q,float z,float rock,float height){
    int id=m->bridge_bins[aw_clamp((int)z,0,63)*64+aw_clamp((int)x,0,63)];
    if(!id)return rock;
    const AwBridge*b=&m->bridges[id-1];
    float solid=aw_bridge_field(m,x,q,z),h=fmaxf(0,.5f-fabsf(rock-solid))/.5f;
    float joined=fmaxf(rock,solid)+.125f*h*h;
    float u=(x-b->x-.5f)*b->dx+(z-b->z-.5f)*b->dz;
    return fminf(joined,fmaxf(height,aw_bridge_q(b,u))-q);
}
static int aw_bridge_index(AwMap*m){
    memset(m->bridge_bins,0,sizeof(m->bridge_bins));
    for(int i=0;i<m->bridge_count;i++){
        const AwBridge*b=&m->bridges[i];
        for(int u=-2;u<=b->length+2;u++)for(int v=-2;v<=2;v++){
            int x=b->x+b->dx*u+b->dz*v,z=b->z+b->dz*u-b->dx*v;
            if(x<0||z<0||x>=64||z>=64)return 0;
            int c=z*64+x;if(m->bridge_bins[c]&&m->bridge_bins[c]!=i+1)return 0;
            m->bridge_bins[c]=i+1;
        }
    }return 1;
}
#endif
