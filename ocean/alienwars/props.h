#ifndef ALIENWARS_PROPS_H
#define ALIENWARS_PROPS_H
/* Cosmetic geometry only. Seeds do not consume the authoritative map RNG.
 * Alien vegetation uses ribbed stalks and two-sided fungal membranes. */
static float aw_prop_random(uint32_t seed){return (aw_hash(seed)&65535)/65535.0f;}
static Color aw_tint(Color c,float v){return (Color){(unsigned char)Clamp(c.r*v,0,255),(unsigned char)Clamp(c.g*v,0,255),(unsigned char)Clamp(c.b*v,0,255),c.a};}
static void aw_smooth_triangle(AwBuilder*b,Vector3 a,Vector3 c,Vector3 d,Vector3 na,Vector3 nc,Vector3 nd,Color color,float rank,float kind){
    aw_triangle(b,a,c,d,color,rank,kind);int n=b->count-3;b->normals[n]=na;b->normals[n+1]=nc;b->normals[n+2]=nd;
}
static void aw_branch(AwBuilder*b,Vector3 a,Vector3 end,float ra,float rb,Color color,float rank,float kind,int sides){
    Vector3 axis=Vector3Normalize(Vector3Subtract(end,a));
    Vector3 u=Vector3Normalize(Vector3CrossProduct(axis,fabsf(axis.y)>.9f?(Vector3){1,0,0}:(Vector3){0,1,0}));
    Vector3 v=Vector3CrossProduct(axis,u);
    for(int i=0;i<sides;i++){
        float t=i*2*PI/sides,t1=(i+1)*2*PI/sides;
        Vector3 n=Vector3Add(Vector3Scale(u,cosf(t)),Vector3Scale(v,sinf(t)));
        Vector3 nn=Vector3Add(Vector3Scale(u,cosf(t1)),Vector3Scale(v,sinf(t1)));
        Vector3 p=Vector3Add(a,Vector3Scale(n,ra)),q=Vector3Add(end,Vector3Scale(n,rb));
        Vector3 r=Vector3Add(end,Vector3Scale(nn,rb)),s=Vector3Add(a,Vector3Scale(nn,ra));
        aw_smooth_triangle(b,p,r,q,n,nn,n,color,rank,kind);
        aw_smooth_triangle(b,p,s,r,n,nn,nn,color,rank,kind);
        if(rb>0)aw_triangle(b,end,q,r,color,rank,kind);
    }
}
static void aw_leaf(AwBuilder*b,Vector3 p,Vector3 direction,float length,float width,float roll,Color color,float rank){
    Vector3 d=Vector3Normalize(direction),axis=Vector3CrossProduct(d,(Vector3){0,1,0});
    if(Vector3Length(axis)<.01f)axis=(Vector3){1,0,0};axis=Vector3Normalize(axis);
    Vector3 up=Vector3CrossProduct(d,axis);axis=Vector3Add(Vector3Scale(axis,cosf(roll)),Vector3Scale(up,sinf(roll)));
    Vector3 tip=Vector3Add(p,Vector3Scale(d,length));
    Vector3 mid=Vector3Add(Vector3Lerp(p,tip,.48f),Vector3Scale(up,.035f));
    Vector3 l=Vector3Add(Vector3Lerp(p,tip,.42f),Vector3Scale(axis,width));
    Vector3 r=Vector3Subtract(Vector3Lerp(p,tip,.42f),Vector3Scale(axis,width));
    Vector3 pairs[4][3]={{p,l,mid},{l,tip,mid},{tip,r,mid},{r,p,mid}};
    for(int i=0;i<4;i++){
        aw_triangle(b,pairs[i][0],pairs[i][1],pairs[i][2],color,rank,3);
        aw_triangle(b,pairs[i][2],pairs[i][1],pairs[i][0],color,rank,3);
    }
}
#include "alien_flora.h"
static void aw_boulder(AwBuilder*b,Vector3 p,float radius,float height,uint32_t seed,Color color,float rank){
    enum {RINGS=5,SIDES=11};Vector3 v[RINGS][SIDES],norm[RINGS][SIDES];
    float phase=aw_prop_random(seed)*2*PI;
    for(int y=0;y<RINGS;y++)for(int i=0;i<SIDES;i++){
        float latitude=-.5f+y*(PI/2+.5f)/(RINGS-1),a=phase+i*2*PI/SIDES;
        float shape=.82f+.25f*aw_prop_random(seed+i*119u+y*257u);
        Vector3 n={cosf(a)*cosf(latitude),sinf(latitude),sinf(a)*cosf(latitude)};
        v[y][i]=(Vector3){p.x+n.x*radius*shape,p.y+(n.y+.48f)*height*.68f,p.z+n.z*radius*shape};
        norm[y][i]=Vector3Normalize((Vector3){n.x/radius,n.y/(height*.68f),n.z/radius});
    }
    for(int y=0;y<RINGS-1;y++)for(int i=0;i<SIDES;i++){
        int j=(i+1)%SIDES;Color c=aw_tint(color,.89f+.18f*aw_prop_random(seed+y*5+i));
        aw_smooth_triangle(b,v[y][i],v[y+1][i],v[y+1][j],norm[y][i],norm[y+1][i],norm[y+1][j],c,rank,0);
        aw_smooth_triangle(b,v[y][i],v[y+1][j],v[y][j],norm[y][i],norm[y+1][j],norm[y][j],c,rank,0);
    }
}
static void aw_grass(AwBuilder*b,Vector3 p,uint32_t seed,float rank,int dry){
    for(int i=0;i<7;i++){
        uint32_t h=aw_hash(seed+i*73);float a=aw_prop_random(h)*2*PI,r=aw_prop_random(h+1)*.22f;
        Vector3 q={p.x+cosf(a)*r,p.y,p.z+sinf(a)*r};
        aw_leaf(b,q,(Vector3){cosf(a)*.3f,1,sinf(a)*.3f},.19f+aw_prop_random(h+3)*.26f,.024f,a,dry?(Color){133,119,79,255}:(Color){88,105,53,255},rank);
    }
}
/* Low fern clumps dress forest floors without adding apparent body obstacles. */
static void aw_fern(AwBuilder*b,Vector3 p,uint32_t seed,float rank){
    for(int f=0;f<5;f++){
        float angle=f*2.399963f+aw_prop_random(seed)*6.28f;Vector3 last=p;
        for(int k=1;k<=6;k++){
            float t=k/6.0f,r=.48f*t;
            Vector3 q={p.x+cosf(angle)*r,p.y+.08f+.38f*sinf(t*2.4f),p.z+sinf(angle)*r};
            aw_branch(b,last,q,.009f,.006f,(Color){83,100,48,255},rank,3,3);
            for(int side=-1;side<=1;side+=2){float a=angle+side*1.05f;
                aw_leaf(b,q,(Vector3){cosf(a),.2f,sinf(a)},.19f*(1-t*.72f),.045f*(1-t*.6f),0,(Color){66,103,45,255},rank);}
            last=q;
        }
    }
}
static void aw_box(AwBuilder*b,Vector3 p,Vector3 size,Color color,float rank,float kind){
    Vector3 v[8];for(int i=0;i<8;i++)v[i]=(Vector3){p.x+(i&1?.5f:-.5f)*size.x,p.y+(i&2?.5f:-.5f)*size.y,p.z+(i&4?.5f:-.5f)*size.z};
    static const int faces[6][4]={{0,2,3,1},{4,5,7,6},{0,4,6,2},{1,3,7,5},{2,6,7,3},{0,1,5,4}};
    for(int i=0;i<6;i++){const int*f=faces[i];aw_triangle(b,v[f[0]],v[f[1]],v[f[2]],color,rank,kind);aw_triangle(b,v[f[0]],v[f[2]],v[f[3]],color,rank,kind);}
}
static void aw_outpost(AwBuilder*b,Vector3 p,int side){
    const float rank=AW_CELLS-1; /* Reveal the assembled base with the final terrain. */
    Color metal={115,120,115,255},dark={48,54,54,255},trim=side?(Color){174,119,77,255}:(Color){103,152,155,255};
    Vector3 q=p;q.y+=.10f;aw_box(b,q,(Vector3){4.0f,.2f,3.7f},dark,rank,5);
    q=p;q.y+=.48f;aw_box(b,q,(Vector3){2.7f,.74f,2.2f},metal,rank,5);
    q.y+=.43f;aw_box(b,q,(Vector3){2.5f,.12f,2.0f},dark,rank,5);
    for(int i=0;i<6;i++){
        q=(Vector3){p.x-1.08f+i*.42f,p.y+.59f,p.z-1.115f};aw_box(b,q,(Vector3){.31f,.29f,.04f},(Color){58,91,103,255},rank,6);
        q.z=p.z+1.115f;aw_box(b,q,(Vector3){.31f,.29f,.04f},(Color){58,91,103,255},rank,6);
    }
    for(int i=0;i<2;i++){
        q=(Vector3){p.x+(i?1:-1)*1.63f,p.y+.38f,p.z+.65f};Vector3 end=q;end.y+=.42f;
        aw_branch(b,q,end,.25f,.25f,metal,rank,5,12);
        q.y+=.46f;aw_box(b,q,(Vector3){.45f,.06f,.45f},trim,rank,5);
    }
    q=(Vector3){p.x+.62f,p.y+.96f,p.z+.4f};Vector3 mast=q;mast.y+=1.05f;
    aw_branch(b,q,mast,.033f,.022f,metal,rank,5,6);
    Vector3 cross=mast;cross.x-=.4f;Vector3 end=mast;end.x+=.4f;aw_branch(b,cross,end,.022f,.022f,metal,rank,5,5);
    q=(Vector3){p.x-.65f,p.y+1.0f,p.z+.15f};aw_box(b,q,(Vector3){.70f,.20f,.7f},metal,rank,5);
    for(int i=0;i<5;i++){q.z=p.z-.1f+i*.12f;q.y=p.y+1.115f;aw_box(b,q,(Vector3){.58f,.01f,.045f},dark,rank,5);}
}
#endif
