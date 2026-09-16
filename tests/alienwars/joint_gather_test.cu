#include <cuda_runtime.h>
#include <cassert>
#include <cstdio>
#include <vector>
#include "src/joint_gather.cuh"
int main(){
    /* Unequal family populations, two buffers, multiple times/features. This
     * also covers the [layers, agents, hidden] recurrent-state gather. */
    int layout[]={0,3,6,7,9,12};
    for(int times: {2,7})for(int features: {1,4,13,128,645}){
        int size=times*24*features;std::vector<float>src(size);
        for(int i=0;i<size;i++)src[i]=(float)i;
        float *device,*dest;assert(cudaMalloc(&device,size*sizeof(float))==cudaSuccess);assert(cudaMalloc(&dest,size*sizeof(float))==cudaSuccess);
        cudaMemcpy(device,src.data(),size*sizeof(float),cudaMemcpyHostToDevice);
        for(int family=0;family<5;family++){
            int slice=layout[family+1]-layout[family],agents=slice*2,n=times*agents*features;
            joint_gather<<<(n+255)/256,256>>>(dest,device,times,agents,features,24,12,layout[family],slice);
            std::vector<float>got(n);assert(cudaMemcpy(got.data(),dest,n*sizeof(float),cudaMemcpyDeviceToHost)==cudaSuccess);
            for(int t=0;t<times;t++)for(int a=0;a<agents;a++)for(int f=0;f<features;f++){
                int source=(a/slice)*12+layout[family]+a%slice;
                assert(got[(t*agents+a)*features+f]==src[(t*24+source)*features+f]);
            }
        }cudaFree(device);cudaFree(dest);
    }
    puts("JOINT_GATHER five_disjoint_families=PASS unequal_populations=PASS two_buffers=PASS recurrent_state_layout=PASS");
}
