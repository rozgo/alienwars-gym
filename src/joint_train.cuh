/* Synchronous independent PPO with multiple learning policies. Environments
 * step once with everyone's actions; all learners consume that same rollout.
 * The normal single-policy / frozen-opponent path remains unchanged.
 * Included by pufferl.cu after allocator, PPO and checkpoint definitions. */

#include "joint_gather.cuh"

static void joint_setup(PuffeRL* root) {
    root->learners = (PuffeRL**)calloc(root->num_policies, sizeof(PuffeRL*));
    for (int family = 0; family < root->num_policies; family++) {
        PuffeRL* p = (PuffeRL*)calloc(1, sizeof(PuffeRL));
        root->learners[family] = p;
        p->hypers = root->hypers;
        int slice = root->vec->policy_layout[family + 1] - root->vec->policy_layout[family];
        int agents = slice * root->vec->buffers;
        int horizon = root->hypers.horizon;
        int segments = root->hypers.minibatch_size / horizon;
        if (segments > agents) segments = agents;
        while (segments > 1 && agents % segments) segments--;
        assert(agents > 0 && segments > 0);
        p->hypers.total_agents = agents;
        p->hypers.minibatch_size = segments * horizon;
        p->hypers.total_timesteps = (root->hypers.total_timesteps / root->hypers.total_agents) * agents;
        p->policies = &root->policies[family];
        p->num_policies = 1;
        p->act_sizes = root->act_sizes;
        p->is_continuous = root->is_continuous;
        p->train_stream = root->train_stream;
        Policy* policy = p->policies;
        Allocator* acts = &p->activ_alloc;
        Allocator* grads = &p->grads_alloc;
        int heads = NUM_ATNS, sizes[] = ACT_SIZES, outputs = 0;
        for (int i = 0; i < heads; i++) outputs += sizes[i];
        p->train_activs = arch_reg_train(&policy->arch, policy->weights,
            acts, grads, segments * horizon);
        register_rollout_buffers(&p->rollouts, acts, horizon, agents, OBS_SIZE, heads, outputs);
        if (!p->hypers.reset_every_horizon) {
            p->rollouts.initial_states = {.shape = {1, p->hypers.num_layers, agents, p->hypers.hidden_size}};
            alloc_register(acts, &p->rollouts.initial_states);
        }
        register_rollout_buffers(&p->train_rollouts, acts, agents, horizon, OBS_SIZE, heads, outputs);
        register_train_buffers(p->train_buf, acts, segments, horizon);
        register_ppo_buffers(p->ppo_bufs, acts, segments, horizon, outputs, p->is_continuous);
        p->train_state = {.shape = {p->hypers.num_layers, agents, p->hypers.hidden_size}};
        alloc_register(acts, &p->train_state);
        muon_init(&p->muon, &policy->params_alloc, p->hypers.momentum, acts);
        alloc_create(grads);alloc_create(acts);
        p->grad = {.data = (precision_t*)grads->mem, .shape = {grads->total_elems}};
        cudaMalloc((void**)&p->losses, NUM_LOSSES * sizeof(float));
        cudaMemset(p->losses, 0, NUM_LOSSES * sizeof(float));
        cudaMalloc((void**)&p->profile.stamps, 2 * NUM_TE * sizeof(unsigned long long));
        cudaMemcpy(p->muon.lr, &p->hypers.lr, sizeof(float), cudaMemcpyHostToDevice);
        cudaMemset(p->muon.mb.data, 0, numel(p->muon.mb.shape) * sizeof(float));
        printf("JOINT_LEARNER policy=%d agents=%d minibatch=%d params=%ld\n",
            family, agents, p->hypers.minibatch_size, (long)numel(policy->param.shape));
    }
}

static void train_joint(PuffeRL* root) {
    int full = root->hypers.total_agents, per = root->vec->agents_per_buf;
    int times = root->hypers.horizon;
    for (int family = 0; family < root->num_policies; family++) {
        PuffeRL* p = root->learners[family];
        int offset = root->vec->policy_layout[family];
        int slice = root->vec->policy_layout[family + 1] - offset;
        int agents = p->hypers.total_agents;
        cudaStream_t stream = p->train_stream;
        RolloutBuf* to = &p->rollouts;
        RolloutBuf* from = &root->rollouts;
#define JOINT_COPY(field, features) \
        joint_gather<<<grid_size(times * agents * (features)), BLOCK_SIZE, 0, stream>>>( \
            to->field.data, from->field.data, times, agents, (features), full, per, offset, slice)
        JOINT_COPY(observations, OBS_SIZE);
        JOINT_COPY(actions, NUM_ATNS);
        JOINT_COPY(logprobs, 1);
        JOINT_COPY(rewards, 1);
        JOINT_COPY(terminals, 1);
        JOINT_COPY(values, 1);
        JOINT_COPY(action_mask, (int)from->action_mask.shape[2]);
#undef JOINT_COPY
        if (from->initial_states.data) {
            int layers = p->hypers.num_layers, hidden = p->hypers.hidden_size;
            joint_gather<<<grid_size(layers * agents * hidden), BLOCK_SIZE, 0, stream>>>(
                to->initial_states.data, from->initial_states.data,
                layers, agents, hidden, full, per, offset, slice);
        }
        /* Gather completes before CUDA capture; the captured learner reads
         * stable compact buffers. No other family's samples enter this PPO. */
        cudaStreamSynchronize(stream);
        train_impl(p, NULL);
        for (int i = PROF_TRAIN_MISC; i <= PROF_TRAIN_MODEL; i++) {
            root->profile.accum[i] += p->profile.accum[i];p->profile.accum[i] = 0;
        }
    }
    root->epoch++;
}
