# Development and GPU workflow

## Native dependencies

macOS uses Apple Clang, the Xcode command-line tools, Homebrew and `libomp`:

```sh
xcode-select --install    # Only if the command-line tools are missing.
brew install libomp      # Only if missing.
./scripts/check.sh
```

Linux requires a CUDA **development** toolkit, including `nvcc`, cuBLAS,
cuSOLVER and cuRAND; an NVIDIA driver; NCCL headers/library; Clang, ccache,
OpenMP, Git, curl, a C/C++ host compiler, OpenGL/GLFW libraries, and Python 3
standard-library tooling. The project does not install a Python ML stack.

The [upstream installer](https://github.com/PufferAI/PufferTank/blob/5.0/install.sh)
contains the full Ocean dependency list. On an existing GPU workstation, inspect
installed dependencies first and install only missing packages. The installer
also provisions unrelated environments and uses system package management.
On a provisioned machine, explicitly select the toolkit without changing the
system CUDA symlink:

```sh
export CUDA_HOME=/usr/local/cuda
export PATH="$CUDA_HOME/bin:$PATH"
nvcc --version
nvidia-smi
./scripts/train_breakout.sh
```

`--cu` means the environment itself runs on CUDA. A normal native build without
`--cu` still trains on CUDA but runs a supported `.h` environment on the CPU.
`--cpu` builds inference/playback only. Debug builds use ASan/UBSan; default
trainer precision is bf16, with `--float` available for numerical diagnostics.

## Remote access and source synchronization

Use an existing SSH alias from the user's SSH configuration. Machine-specific
connection details belong in ignored `.local/` notes, never committed docs.
The local bring-up notes, when present, are `.local/GPU.md`.

```sh
ssh GPU_ALIAS 'nvidia-smi'
# In a fresh dedicated directory on the GPU machine:
git clone git@github.com:rozgo/alienwars-gym.git
cd alienwars-gym
git status --short
git pull --ff-only
git rev-parse HEAD
./scripts/train_breakout.sh breakout_MY_UNIQUE_RUN
```

Use a committed, clean source revision and an isolated directory. Do not
change source beneath a running job or stop unrelated GPU processes. Builds
share intermediate files, so concurrent experiments need separate worktrees.
The launch wrapper records an existing dirty diff but does not make untracked
source reproducible: commit source before a reportable run.

After training, copy that run's outputs back using SCP/rsync. With the same
relative paths on both machines, verify the manifest:

```sh
python3 - outputs/YOUR_RUN/run.json <<'PY'
import hashlib, json, pathlib, sys
for item in json.loads(pathlib.Path(sys.argv[1]).read_text())['checkpoints']:
    p = pathlib.Path(item['path'])
    assert hashlib.sha256(p.read_bytes()).hexdigest() == item['sha256'], p
    print('Verified', p)
PY
./scripts/play_breakout.sh PATH/TO/CHECKPOINT.bin
```

## Validate and interpret the first run

1. `./scripts/check.sh`: CPU debug build and bounded simulation for Breakout
   and Minimal. This is a memory-safety smoke check, not a gameplay test suite.
2. `./scripts/train_breakout.sh RUN_ID`: native GPU build, one training seed,
   upstream hyperparameters, 55M requested steps, and a bounded final evaluation.
3. Inspect `outputs/RUN_ID/logs/breakout/RUN_ID.ini`: this includes resolved
   configuration and logged metric series. Reject nonfinite losses or metrics.
4. Evaluate an explicit checkpoint with a distinct `--base.seed` using
   `./build/puffer-breakout eval PATH.bin --headless --base.eval_episodes=256
   --base.seed=1073`. CUDA evaluation is the matching training backend.
5. Copy and hash-check weights, then run CPU headless evaluation and the native
   Mac viewer. The imported CPU viewer does not wire `base.seed` into its RNG;
   do not claim its seed override works. Use CUDA for seeded evaluation.

The trainer rounds requested timesteps down to whole rollout batches. With
4096 agents and horizon 32, 55,000,000 requested steps produces 54,919,168
steps. Report actual counters from logs. The trainer's uptime excludes some
initialization/evaluation work; the wrapper records full process elapsed time
separately. Shared-GPU measurements are not isolated performance benchmarks.

A no-checkpoint CPU run uses the default zero/NOOP actions, not a random policy.
Label that baseline precisely. A single seed and a live visual demonstration
establish bring-up, not general policy robustness. Do not confuse the website's
RTX 5090 throughput claim with measurements from another GPU.

## Git and artifacts

`upstream` tracks PufferAI; `origin` is the project repository. Keep the license
and upstream history. Generated binaries, Raylib downloads, raw logs and
checkpoints remain ignored. Commit concise reports to `docs/runs/`, with exact
commands, source revision, configuration, backend, measured metrics and hashes.
Use release assets or deliberately configured LFS for selected durable binaries.
Review `git diff --check` and the staged diff before committing or publishing.
