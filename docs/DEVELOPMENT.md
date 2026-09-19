# Development and GPU workflow

New here? [Start with a local preview](START_HERE.md) and the
[browser lessons](https://rozgo.github.io/alienwars-gym/learn/). Run
`uv run scripts/doctor.py --target native` (or `web` / `train`) for focused
prerequisite checks. The default `preview` target requires no compiler or GPU.

## Next development slice

The [navigation reliability iteration](runs/navigation-reliability-2026-09-18.md)
implemented sensor-derived traffic anticipation and bounded recovery, then trained
all five families across three seeds. It did not pass the reliability/endurance
gates; candidate checkpoints remain experimental and public policies are unchanged.
Further navigation work should focus on the recorded ground, submarine and
aircraft conflicts before expanding tactical behavior.

The fleet port to the [Flecs C runtime](FLECS.md) is validated, including native/WASM
behavior and a five-family training smoke. The [first biological visual slice](ART_PIPELINE.md)
adds an authored asset kit and terrain/water detail without changing the policy contract.

The accepted direction is [biological warfare and cultivation](BIOLOGICAL_WARFARE.md):
an entirely alien world where military organisms and equipment are grown.
[Cultivate and Defend](CULTIVATION_SLICE.md) proposes the next playable slice,
including soil/water preparation, growth, collection, supply and bounded combat.
Its completion checks cover accounting, physical tradeoffs, perception, native/WASM
parity and performance. The first milestone uses reference opponents; combat PPO
and historical-opponent self-play follow environment validation. Current published
policies remain navigation policies.

## Native development

Install [uv](https://docs.astral.sh/uv/getting-started/installation/) and run
`uv sync --locked` from the checkout. The committed `.python-version` selects
Python 3.12.12; core tooling has no third-party Python dependencies. Invoke scripts
with `uv run scripts/...py`. Image tools use `uv run --group art`; their Pillow
dependency is locked separately from the core environment. Blender uses its own
embedded Python. Native simulation and PPO remain C/CUDA; Python orchestrates
builds, checks, training processes and reports.

Map Lab is the standalone C terrain and shared-fleet policy viewer. See
[SHARED_NAVIGATION.md](SHARED_NAVIGATION.md) for its five-learner PPO contract. Navigation Lab
adds a native PufferLib scout task using the same terrain, support and sensors.
Its contract and commands are in [NAVIGATION_RL.md](NAVIGATION_RL.md).

macOS uses Apple Clang, the Xcode command-line tools, Homebrew and `libomp`:

```sh
xcode-select --install    # Only if the command-line tools are missing.
brew install libomp      # Only if missing.
./scripts/check.sh
./build/maplab --seed=73
```

The smoke check builds Map Lab and the upstream Minimal example with ASan/UBSan,
generates a headless world and runs Minimal for 1,024 steps. Use
`uv run scripts/check_maplab.py` after the native and web builds for the full
native/WASM terrain and navigation checks. See [MAPLAB.md](MAPLAB.md) and
[WEB.md](WEB.md) for browser validation and publishing.

The native environment interface is `src/pufferenv.h`; `ocean/minimal/minimal.h`
provides a reference. Run `uv run scripts/check_navigation.py` for action motion,
terminal/reset contracts, recurrent-state reset and native/WASM route outcomes.
Run `uv run scripts/check_flecs.py` for pre-port behavior, ECS ownership, memory
and allocation-free step/reset checks. Shared fleet consumers link
`ocean/alienwars/flecs_runtime.c` as C, including the CUDA trainer.

## GPU prerequisites

Native training requires a CUDA development toolkit, including `nvcc`,
cuBLAS, cuSOLVER and cuRAND; an NVIDIA driver; NCCL; Clang, ccache, OpenMP,
a C/C++ host compiler and graphics development libraries. Python tooling here
uses the standard library, not a Python ML stack.

The [upstream installer](https://github.com/PufferAI/PufferTank/blob/5.0/install.sh)
lists Ocean dependencies. Inspect installed dependencies before running an
installer: it also provisions unrelated environments and uses system packages.
On an existing machine, select the toolkit without changing the system symlink:

```sh
export CUDA_HOME=/usr/local/cuda
export PATH="$CUDA_HOME/bin:$PATH"
nvcc --version
nvidia-smi
```

For supported upstream environments, `--cu` selects a CUDA simulation backend.
Native training without `--cu` still uses CUDA learning kernels with a CPU
environment. `--cpu` builds playback/evaluation, not CPU training. Map Lab has
its own standalone viewer path. For AlienWars, default native builds train the
navigation task; `--cpu --rl` and `--web --rl` select its policy viewer.

## Remote access and source synchronization

Use an existing SSH alias from the user's SSH configuration. Machine-specific
connection details belong in ignored `.local/` notes, never committed docs.
The local access notes, when present, are `.local/GPU.md`.

```sh
ssh GPU_ALIAS 'nvidia-smi'
# In a fresh dedicated directory on the GPU machine:
git clone git@github.com:rozgo/alienwars-gym.git
cd alienwars-gym
git status --short
git pull --ff-only
git rev-parse HEAD
```

Use a committed, clean source revision and an isolated directory. Do not change
source beneath a running job or stop unrelated GPU processes. Builds share
intermediate files, so concurrent experiments need separate worktrees.

## Reproducible training

At the start of the next RL iteration, explicitly decide whether to implement
self-play and record the rationale and evaluation plan. The intended direction
is training warfare between alien factions; historical traffic controllers may
be a useful earlier navigation experiment. Follow the
[self-play and faction warfare direction](SHARED_NAVIGATION.md#self-play-and-faction-warfare),
and distinguish planned competitive training from today's joint navigation task.

Start with validated CPU environment behavior and a bounded single-GPU run.
Record the source revision, compiler/toolkit and GPU versions, backend,
precision, seeds, resolved configuration, exact commands and checkpoint hashes.
Keep a unique ignored output directory for each run.

Reject nonfinite observations, losses and metrics. Evaluate an explicit
checkpoint on held-out seeds and compare with a disclosed baseline. A live
visual demonstration is not evidence of general policy robustness. CPU and
CUDA environments may be separate implementations, so report backend-specific
results rather than assuming exact parity.

Report actual logged timestep counters: the trainer rounds requested steps to
whole rollout batches. Distinguish trainer-reported time from full process time.
Checkpoint weights are not a complete optimizer-resume state. Hash-check files
after transferring them between machines, and use their matching architecture
and configuration.

## Git and artifacts

`upstream` tracks PufferAI; `origin` is the project repository. Keep the license
and upstream history. Generated binaries, Raylib downloads, raw logs and
checkpoints remain ignored. Commit concise reports to `docs/runs/`, with source,
settings, measured results and hashes. Use release assets or deliberately
configured LFS for selected durable binaries. Review `git diff --check` and the
staged diff before committing or publishing.
