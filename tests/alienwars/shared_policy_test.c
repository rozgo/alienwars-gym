#include "src/puffercpu.c"
int main(int argc,char**argv){
    if(argc!=2)return 2;int sizes[]={4,3,3,3};unsigned digest=2166136261u;
    for(int family=0;family<5;family++){
        char path[512];snprintf(path,sizeof(path),"%s/mission-%d.bin",argv[1],family);Weights*w=load_weights(path);assert(w&&w->size-7==182656);
        PufferNet*n=make_puffernet(w,1,645,128,2,sizes,4);
        for(int step=0;step<100;step++){
            float obs[645],actions[4],terminal=step%25==0;for(int i=0;i<645;i++)obs[i]=((i*13+step*7)%31-15)/16.0f;
            for(int i=0;i<5;i++)obs[16+i]=i==family;
            forward_puffernet(n,obs,actions,NULL,&terminal);multidiscrete(n->multidiscrete,n->decoder->output,actions,1,NULL);
            for(int i=0;i<14;i++)assert(isfinite(n->decoder->output[i]));
            for(int i=0;i<4;i++){assert(actions[i]>=0&&actions[i]<=(i?2:3)&&floorf(actions[i])==actions[i]);digest=(digest^(unsigned)actions[i])*16777619u;}
        }free_puffernet(n);free(w);
    }
    printf("SHARED_POLICY families=5 recurrent_decisions=500 categorical_actions=2000 finite=PASS digest=%08x\n",digest);return 0;
}
