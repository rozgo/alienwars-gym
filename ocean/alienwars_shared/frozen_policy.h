#ifndef ALIENWARS_FROZEN_POLICY_H
#define ALIENWARS_FROZEN_POLICY_H
/* Read-only contract-2 traffic. This small scalar implementation mirrors the
 * native PufferNet linear -> two MinGRUs -> linear architecture; parity against
 * src/puffercpu.c is checked before use. No allocations during inference. */
#define AW_FROZEN_HIDDEN 128
#define AW_FROZEN_FLOATS (128*AW_MISSION_INPUTS+14*128+2*3*128*128)
typedef struct {float state[2][128];} AwFrozenState;
static float*aw_frozen_load(const char*directory,int family){
    char path[2048];snprintf(path,sizeof(path),"%s/mission-%d.bin",directory,family);
    FILE*f=fopen(path,"rb");if(!f)return NULL;
    float*w=malloc(AW_FROZEN_FLOATS*sizeof(float));if(!w){fclose(f);return NULL;}
    int valid=fread(w,sizeof(float),AW_FROZEN_FLOATS,f)==AW_FROZEN_FLOATS&&fgetc(f)==EOF;fclose(f);
    for(int i=0;valid&&i<AW_FROZEN_FLOATS;i++)valid=isfinite(w[i]);
    if(!valid){free(w);return NULL;}return w;
}
static float aw_frozen_sigmoid(float x){return 1/(1+expf(-x));}
static void aw_frozen_linear(const float*w,const float*x,float*y,int rows,int columns){
    for(int r=0;r<rows;r++){float sum=0;for(int c=0;c<columns;c++)sum+=w[r*columns+c]*x[c];y[r]=sum;}
}
static void aw_frozen_forward(const float*w,AwFrozenState*s,const float*obs,float*action){
    float x[128],projection[384],logits[14];
    aw_frozen_linear(w,obs,x,128,AW_MISSION_INPUTS);const float*decoder=w+128*AW_MISSION_INPUTS;
    const float*recurrent=decoder+14*128;
    for(int l=0;l<2;l++){
        aw_frozen_linear(recurrent+l*3*128*128,x,projection,384,128);
        for(int h=0;h<128;h++){
            float candidate=projection[h]>=0?projection[h]+.5f:aw_frozen_sigmoid(projection[h]);
            float value=s->state[l][h]+aw_frozen_sigmoid(projection[128+h])*(candidate-s->state[l][h]);
            float highway=aw_frozen_sigmoid(projection[256+h]);s->state[l][h]=value;x[h]=highway*value+(1-highway)*x[h];
        }
    }
    aw_frozen_linear(decoder,x,logits,14,128);
    int offset=0;for(int head=0;head<4;head++){int size=head?3:4,best=0;
        for(int j=1;j<size;j++)if(logits[offset+j]>logits[offset+best])best=j;
        action[head]=best;offset+=size;}
}
#endif
