# Working on AlienWars Gym

## Scope and source of truth

This is a source-based PufferLib 5.0 project. Start with `README.md`,
`docs/DEVELOPMENT.md`, and `docs/UPSTREAM.md`. Check `git status` before edits and
preserve unrelated work. Follow explicit user instructions over this guide.
The initial playable/training baseline is upstream Breakout; AlienWars gameplay
has not been implemented. Do not describe the baseline as an AlienWars policy.

Use the native C/CUDA API in this checkout. Do not add a legacy PufferLib 2/3
Python package, Gymnasium wrapper, PyTorch trainer, or global Python environment
as the default training path. Python here is optional standard-library tooling.
The website occasionally trails the source: there is no `binding.c` in the
current minimal template. Inspect headers before following an older tutorial.

## Repository map

- `ocean/<env>/<env>.h`: CPU environment and observation/action metadata.
- `ocean/<env>/<env>.cu`: optional CUDA environment or custom encoder; read it
  before assuming which it is. `--cu` selects a CUDA environment backend.
- `src/pufferenv.h`: `Env`, `Agent`, logging, and `puf_*` interface.
- `ocean/minimal/`: commented multiagent example with a custom encoder.
- `ocean/breakout/`: small CPU/CUDA baseline used for bring-up.
- `config/default.ini` then `config/<env>.ini`: merged runtime configuration;
  command-line `--section.key=value` overrides both.
- `src/pufferl.cu`, `src/algo.cu`: native trainer and learning kernels.
- `src/puffercpu.c`: standalone CPU inference and viewer.
- `scripts/`: project checks, bounded training, playback.
- `outputs/`, `build/`, `checkpoints/`, `logs/`: ignored generated artifacts.
- `docs/runs/`: concise checked-in measurements and artifact hashes.

## Environment implementation

1. Specify observations, legal actions, reward terms, terminal conditions,
   reset behavior, simulation timestep and measurable success before tuning.
2. Start with a CPU environment using the existing header interface. Match
   `obs_t`, `OBS_SIZE`, `NUM_ATNS`, `ACT_SIZES`, agent count and buffer strides.
   Actions/rewards/terminals use float pointers in `Agent`.
3. Implement `puf_init`, `puf_reset`, `puf_step`, `puf_render`, `puf_close`, and
   `puf_log`. `Log` contains float fields with `n` last. Keep required `Env`
   fields consistent with `src/pufferenv.h` and the vectorizer.
4. Write all observations each step or clear sparse buffers first. Clear rewards
   and terminals every step. Reset internally and emit a terminal exactly when
   intended; test episode boundaries and recurrent-state resets explicitly.
5. Keep observation/reward scales roughly order one. Inspect real data for
   nonfinite values, invalid actions, stale flags and out-of-bounds access.
   Diagnose memory corruption before tuning away early NaNs.
6. Allocate stable contiguous buffers at initialization. Avoid allocations,
   file/network I/O and rendering in the step loop. Use per-environment RNG;
   seed it explicitly. Stagger fixed-length environments where appropriate.
7. Establish a correct CPU baseline before optimizing or implementing CUDA.
   CPU and GPU sources are separate: verify behavior with matched configuration
   and seeds; matching weights alone does not prove backend equivalence.

## Build and validation

Run from the repository root (configuration/resources use relative paths).

```sh
./scripts/check.sh
./build.sh breakout build/breakout --cpu
./build.sh breakout build/puffer-breakout --cu   # Linux NVIDIA CUDA toolkit
./scripts/train_breakout.sh                    # bounded 55M-step baseline
./scripts/play_breakout.sh PATH/TO/CHECKPOINT.bin
```

Use `--cpu --debug` with ASan/UBSan for environment development on Linux and
macOS. Run focused contract/boundary tests when environment logic changes, then
a bounded headless rollout, short GPU training with finite losses, explicit
checkpoint evaluation and the real viewer. Documentation-only edits need diff
checks, not expensive training. Existing `tests/` includes historical Python
tests; do not assume blanket `pytest` validates this native branch.

Check actions, reset/terminal behavior and rewards against the declared task,
including poor-but-valid and failure trajectories. A finished training process
is not proof of learning. Compare a disclosed baseline with held-out evaluation
and keep training, selection and evaluation seeds distinct. Use several training
seeds before claiming robustness. Report failed runs and limits honestly.

## Training and reproducibility

- Training requires an NVIDIA GPU and development toolkit (NVCC, cuBLAS, NCCL,
  OpenMP, ccache and graphics link libraries). macOS supports CPU inference and
  rendering, not native training. Do not infer CUDA availability from Python.
- Inspect GPU utilization/processes before launching. Use an isolated checkout
  for this project; preserve other jobs. Start with one GPU and a bounded run;
  use Protein sweeps only after environment correctness and baseline learning.
- Record source commit/dirty diff, native compiler versions, GPU, backend,
  precision, seeds, resolved config, commands, actual steps and timings.
  Separate build/process time from trainer-reported time.
- Use unique output directories, explicit model paths and checkpoint SHA-256.
  Avoid `latest` for reproducible reports. Flat `.bin` policies need their
  matching configuration/architecture and are not optimizer-resume snapshots.
- Keep long-running source checkouts fixed. Move source by normal Git commits
  and fast-forward pulls; do not force-push or reset another machine's work.

## Credentials, artifacts and handoff

Keep SSH keys, tokens, host addresses and machine-specific paths outside Git.
Use an SSH alias in the user's SSH config or ignored `.local/` notes. Publish
only within the user's authorization; preserve the upstream MIT license and
asset attribution. Keep upstream as a separate remote and update it deliberately.

Keep raw logs, native binaries, downloaded libraries and checkpoints ignored.
Commit concise reports with measurements, settings and hashes. Selected durable
binary artifacts should use release assets or an explicit LFS policy, not an
accidental bulk `git add .`. Review `git diff --check` and staged changes.
Handoff should include what ran, what was measured, remaining limitations and
an exact command to watch the trained policy.
