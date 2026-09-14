# AlienWars Gym

A native C/CUDA reinforcement-learning workspace based on **PufferLib 5.0**.
The first baseline is upstream **Breakout**: train on an NVIDIA GPU, then watch
the learned policy run in your browser through Raylib and WebAssembly.
The AlienWars **Map Lab** now generates seeded 3D terrain with WFC tiles,
validates navigation and runs a scripted scout. Combat and AlienWars policy
training are the next implementation stage.

**[Explore the 3D Map Lab](https://rozgo.github.io/alienwars-gym/maplab/?seed=73)**
· [Watch trained Breakout](https://rozgo.github.io/alienwars-gym/)

- [Agent instructions](AGENTS.md)
- [Map Lab specification, build and validation](docs/MAPLAB.md)
- [Development and GPU workflow](docs/DEVELOPMENT.md)
- [Raylib web build and GitHub Pages workflow](docs/WEB.md)
- [Upstream version and source references](docs/UPSTREAM.md)
- [First GPU run: 54.9M steps in 1.98 seconds](docs/runs/breakout_20260914_01.md)
- [PufferLib documentation](https://puffer.ai/docs.html)

## Run the trained baseline locally

The bring-up run trained Breakout on an RTX 4090 and opened its learned policy
on the Mac. Download its 64 KB checkpoint (already present locally):

```sh
mkdir -p outputs
gh release download breakout-baseline-20260914 --repo rozgo/alienwars-gym \
  --pattern breakout-demo.bin --dir outputs
./scripts/play_breakout.sh outputs/breakout-demo.bin
```

## Try it

On macOS, install the Xcode command-line tools and Homebrew `libomp`, then:

```sh
./scripts/check.sh
```

This compiles Breakout and the Minimal multiagent example with ASan/UBSan and
runs 1,024 headless environment steps in each. `build.sh` downloads Raylib 5.5
on first use. These checks exercise simulation without a learned policy.

On Linux with an NVIDIA development toolkit:

```sh
./scripts/train_breakout.sh
```

This builds native CUDA Breakout, trains with the upstream 55-million-step
configuration and seed 73, and evaluates the final checkpoint. It uses one GPU
and limits the training/evaluation process to three minutes. Each run gets a
new ignored `outputs/breakout_<UTC>/` folder with source provenance, compiler/GPU
information, logs, resolved configuration and a SHA-256 checkpoint manifest.
Inspect existing GPU workloads first; do not launch concurrent builds/training
in the same checkout.

Copy the selected `.bin` checkpoint back to this checkout and run:

```sh
./scripts/play_breakout.sh outputs/YOUR_RUN/checkpoints/breakout/YOUR_RUN/YOUR_STEPS.bin
```

The actual filename is recorded in `outputs/YOUR_RUN/run.json`. Playback uses
the same `config/breakout.ini` policy architecture. Escape closes the window;
hold Left Shift and use Left/Right arrows for human control. Run commands from
the repo root so resource paths resolve.

For headless CPU checkpoint evaluation:

```sh
./scripts/play_breakout.sh PATH/TO/CHECKPOINT.bin --headless --base.eval_episodes=10
```

A checkpoint is policy weights, not a complete optimizer-resume state. CPU and
CUDA Breakout are separate implementations; cross-backend scores are not an
exact numerical-equivalence test. See the run report for measured results.

## Add AlienWars

Define the game state, agent observations/actions, rewards, reset/termination
rules and success criteria first. Implement `ocean/alienwars/alienwars.h` and
`config/alienwars.ini` against `src/pufferenv.h`; study `ocean/minimal/minimal.h`
and Breakout for the current interface. Validate CPU behavior and memory safety,
then train a small baseline before adding CUDA simulation or sweeps.

PufferLib 5 has no CPU training path and this setup does not use the older
Python/Gymnasium API. Native trainer source is included so environment and
learning behavior remain inspectable.

Based on [PufferAI/PufferLib](https://github.com/PufferAI/PufferLib), retained
under its [MIT license](LICENSE). Existing third-party asset notices remain
with their source files.
