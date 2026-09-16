#ifndef PUFFER_JOINT_GATHER_CUH
#define PUFFER_JOINT_GATHER_CUH
template <typename T>
__global__ void joint_gather(T* dst, const T* src, int times, int agents,
        int features, int all_agents, int per_buffer, int offset, int slice) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = times * agents * features;
    if (idx >= total) return;
    int feature = idx % features;
    int agent = (idx / features) % agents;
    int time = idx / (features * agents);
    int source_agent = (agent / slice) * per_buffer + offset + agent % slice;
    dst[idx] = src[(time * all_agents + source_agent) * features + feature];
}

#endif
