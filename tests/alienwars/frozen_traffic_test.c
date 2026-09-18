#include "src/puffercpu.c"
#include "ocean/alienwars_shared/shared_api.c"
static void parity(const char*directory,int family){
    Weights*w=calloc(1,sizeof(Weights)+(AW_FROZEN_FLOATS+7)*sizeof(float));w->size=AW_FROZEN_FLOATS+7;w->data=(float*)(w+1);
    unsigned rng=451;for(int i=0;i<AW_FROZEN_FLOATS;i++)w->data[i]=((int)(aw_shared_rng(&rng)%2001)-1000)*.00003f;
    if(directory){float*loaded=aw_frozen_load(directory,family);assert(loaded);memcpy(w->data,loaded,AW_FROZEN_FLOATS*sizeof(float));free(loaded);}
    int sizes[]={4,3,3,3};PufferNet*net=make_puffernet(w,1,645,128,2,sizes,4);
    AwFrozenState state={0};float obs[645],a[4],b[4],terminal=1;
    for(int tick=0;tick<30;tick++){
        for(int i=0;i<645;i++)obs[i]=((int)(aw_shared_rng(&rng)%2001)-1000)*.001f;
        aw_frozen_forward(w->data,&state,obs,a);forward_puffernet(net,obs,b,NULL,&terminal);terminal=0;
        multidiscrete(net->multidiscrete,net->decoder->output,b,1,NULL);
        for(int i=0;i<4;i++)assert(a[i]==b[i]);
        for(int i=0;i<256;i++)assert(fabsf(((float*)state.state)[i]-net->mingru->state[i])<1e-5f);
    }
    memset(&state,0,sizeof(state));terminal=1;aw_frozen_forward(w->data,&state,obs,a);forward_puffernet(net,obs,b,NULL,&terminal);
    for(int i=0;i<256;i++)assert(fabsf(((float*)state.state)[i]-net->mingru->state[i])<1e-5f);
    free_puffernet(net);free(w);
}
int main(int argc,char**argv){
    parity(NULL,0);if(argc>1)for(int family=0;family<5;family++)parity(argv[1],family);
    puts("FROZEN_TRAFFIC official_inference_parity=PASS recurrent_reset=PASS separate_state=PASS");
}
