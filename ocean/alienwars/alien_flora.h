#ifndef ALIENWARS_ALIEN_FLORA_H
#define ALIENWARS_ALIEN_FLORA_H
/* Original procedural fungal flora. Viewer-only, separate from terrain RNG.
 * Opaque, two-sided membranes approximate light transmission without sorting. */
static void aw_flora_triangle(AwBuilder*b,Vector3 a,Vector3 c,Vector3 d,Vector3 na,Vector3 nc,Vector3 nd,Color color,float rank,int kind,int two_sided){
    Vector3 face=Vector3CrossProduct(Vector3Subtract(c,a),Vector3Subtract(d,a));
    if(Vector3Length(face)<.00001f)return;
    if(Vector3DotProduct(face,Vector3Add(Vector3Add(na,nc),nd))<0){Vector3 tmp=c;c=d;d=tmp;tmp=nc;nc=nd;nd=tmp;}
    aw_smooth_triangle(b,a,c,d,na,nc,nd,color,rank,kind);
    if(two_sided)aw_smooth_triangle(b,d,c,a,Vector3Negate(nd),Vector3Negate(nc),Vector3Negate(na),aw_tint(color,.84f),rank,kind);
}
static Vector3 aw_flora_axis(Vector3 p,Vector3 u,Vector3 v,Vector3 n,float x,float z,float y){
    return Vector3Add(p,Vector3Add(Vector3Add(Vector3Scale(u,x),Vector3Scale(v,z)),Vector3Scale(n,y)));
}
static void aw_flora_cap(AwBuilder*b,Vector3 p,Vector3 axis,float radius,float cup,uint32_t seed,Color color,float rank){
    enum {R=7,S=24};Vector3 points[R][S],norms[R][S];
    Vector3 n=Vector3Normalize(axis),u=Vector3Normalize(Vector3CrossProduct(n,fabsf(n.y)>.9f?(Vector3){1,0,0}:(Vector3){0,1,0})),v=Vector3CrossProduct(n,u);
    float phase=aw_prop_random(seed)*2*PI;int lobes=5+seed%4;
    for(int r=0;r<R;r++)for(int i=0;i<S;i++){
        float t=(float)r/(R-1),a=i*2*PI/S,ruffle=sinf(a*lobes+phase),size=radius*t*(1+.085f*ruffle*t);
        float y=cup*(1-t*t)+.075f*radius*ruffle*t*t*t;
        points[r][i]=aw_flora_axis(p,u,v,n,cosf(a)*size,sinf(a)*size,y);
        norms[r][i]=Vector3Normalize(aw_flora_axis((Vector3){0},u,v,n,cosf(a)*2*cup*t/radius,sinf(a)*2*cup*t/radius,1));
    }
    for(int r=0;r<R-1;r++)for(int i=0;i<S;i++){
        int j=(i+1)%S;Color c=aw_tint(color,.77f+.23f*r/(R-2)+.06f*sinf(i*2.399f));
        aw_flora_triangle(b,points[r][i],points[r+1][i],points[r+1][j],norms[r][i],norms[r+1][i],norms[r+1][j],c,rank,17,1);
        aw_flora_triangle(b,points[r][i],points[r+1][j],points[r][j],norms[r][i],norms[r+1][j],norms[r][j],c,rank,17,1);
        if(i%3==0&&r>0){Vector3 a=Vector3Add(points[r][i],Vector3Scale(n,.018f)),z=Vector3Add(points[r+1][i],Vector3Scale(n,.018f));
            aw_branch(b,a,z,.017f,.012f,(Color){113,163,137,255},rank,18,4);}
    }
    for(int i=0;i<S;i++)aw_branch(b,points[R-1][i],points[R-1][(i+1)%S],.024f,.024f,aw_tint(color,1.2f),rank,18,4);
}
static void aw_flora_pod(AwBuilder*b,Vector3 p,float height,float radius,uint32_t seed,Color color,float rank){
    enum {R=11,S=18};Vector3 points[R][S],norms[R][S];
    for(int r=0;r<R;r++)for(int i=0;i<S;i++){
        float t=(float)r/(R-1),a=i*2*PI/S,lat=t*PI,bulge=sinf(lat)*(.8f+.25f*cosf(lat));
        float lobe=1+.07f*cosf(a*6),sweep=.17f*sinf(t*2.8f);
        points[r][i]=(Vector3){p.x+cosf(a)*radius*bulge*lobe+sweep,p.y+t*height,p.z+sinf(a)*radius*bulge*lobe};
        norms[r][i]=Vector3Normalize((Vector3){cosf(a)*sinf(lat)/radius,-cosf(lat)*2/height,sinf(a)*sinf(lat)/radius});
    }
    for(int r=0;r<R-1;r++)for(int i=0;i<S;i++){
        int j=(i+1)%S;Color c=aw_tint(color,.85f+.15f*r/(R-1));
        aw_flora_triangle(b,points[r][i],points[r+1][i],points[r+1][j],norms[r][i],norms[r+1][i],norms[r+1][j],c,rank,17,0);
        aw_flora_triangle(b,points[r][i],points[r+1][j],points[r][j],norms[r][i],norms[r+1][j],norms[r][j],c,rank,17,0);
        if(i%3==0&&r>1&&r<R-2)aw_branch(b,points[r][i],points[r+1][i],.018f,.018f,(Color){151,174,123,255},rank,18,4);
    }
    Vector3 top={p.x+.17f*sinf(2.8f),p.y+height,p.z};
    aw_flora_cap(b,top,(Vector3){.15f,1,0},radius*.45f,-radius*.2f,seed,color,rank);
}
static void aw_alien_flora(AwBuilder*b,Vector3 p,uint32_t seed,float rank,int snow,int dry){
    float phase=aw_prop_random(seed+1)*2*PI,height=(dry?3.0f:4.5f)+aw_prop_random(seed)*2.8f;
    Color skin=snow?(Color){99,122,134,255}:dry?(Color){132,100,85,255}:(Color){92,110,95,255};
    Color cap=snow?(Color){142,163,176,255}:dry?(Color){157,123,92,255}:(Color){91,139,125,255};
    if(!snow&&!dry&&seed%4==0)cap=(Color){133,109,139,255};
    int species=seed%3;Vector3 spine[9];
    for(int i=0;i<9;i++){float t=i/8.0f;spine[i]=(Vector3){p.x+cosf(phase)*sinf(t*1.8f)*.45f,p.y+t*height,p.z+sinf(phase)*sinf(t*1.8f)*.45f};
        if(i)aw_branch(b,spine[i-1],spine[i],.10f+.23f*(1-t),.09f+.23f*(1-t),skin,rank,19,10);}
    for(int i=0;i<6;i++){float a=phase+i*2*PI/6;Vector3 foot={p.x+cosf(a)*.62f,p.y+.025f,p.z+sinf(a)*.62f};aw_branch(b,foot,spine[1],.045f,.13f,skin,rank,19,6);}
    if(species==0){
        aw_flora_cap(b,spine[8],(Vector3){.10f*cosf(phase),1,.10f*sinf(phase)},1.65f+aw_prop_random(seed+4)*.95f,.5f,seed,cap,rank);
        Vector3 bud=spine[4];bud.x+=cosf(phase)*.55f;bud.z+=sinf(phase)*.55f;
        aw_branch(b,spine[3],bud,.10f,.065f,skin,rank,19,7);
        aw_flora_cap(b,bud,(Vector3){.2f*cosf(phase),1,.2f*sinf(phase)},.82f,.2f,seed+1,aw_tint(cap,.87f),rank);
    }else if(species==1){
        for(int i=0;i<5;i++){
            float a=phase+i*2.399963f;Vector3 tip=spine[3+i];tip.x+=cosf(a)*.72f;tip.z+=sinf(a)*.72f;
            aw_branch(b,spine[2+i],tip,.08f,.045f,skin,rank,19,6);
            aw_flora_cap(b,tip,(Vector3){cosf(a)*.9f,.48f,sinf(a)*.9f},1.1f+(i%2)*.38f,-.28f,seed+i,aw_tint(cap,.84f+i*.045f),rank);
        }
    }else{
        for(int i=0;i<3;i++){
            float a=phase+i*2.399963f;Vector3 base=spine[2+i];base.x+=cosf(a)*.7f;base.z+=sinf(a)*.7f;
            aw_branch(b,spine[1+i],base,.14f,.12f,skin,rank,19,8);
            aw_flora_pod(b,base,height*(.52f-i*.075f),.55f+i*.08f,seed+i,aw_tint(cap,.91f+i*.04f),rank);
        }
        aw_flora_cap(b,spine[8],(Vector3){0,1,0},.65f,-.23f,seed,cap,rank);
    }
}
#endif
