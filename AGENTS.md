# Working on AlienWars Gym

## Scope and source of truth

This is a source-based PufferLib 5.0 project. Start with `README.md`,
`docs/DEVELOPMENT.md`, and `docs/UPSTREAM.md`. Check `git status` before edits and
preserve unrelated work. Follow explicit user instructions over this guide.
The initial training baseline is upstream Breakout. AlienWars has a procedural
3D Map Lab with a scripted navigation scout; combat and an AlienWars policy are
not implemented. Do not describe the scout or Breakout as an AlienWars policy.

User direction, September 14, 2026: the repository is public and the first
browser release uses the existing **Raylib web path**, published through
GitHub Pages from **`main:/docs`**. See `docs/WEB.md`. Preserve the working
browser baseline; Three.js is not part of this release. Browser simulation and
inference use WASM; CUDA training stays on the GPU machine.

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
- `web/shell.html`: authored browser presentation for the Raylib web build.
- `ocean/alienwars/map.h`, `layout.h`: deterministic global planning, WFC and
  navigation; independent of rendering.
- `ocean/alienwars/mountain_layout.h`, `mountain.h`, `traversal.h`: regional
  route WFC, graded excavation sockets and sampled walkable floor spans.
- `ocean/alienwars/render.h`, `props.h`, `shaders.h`, `alienwars.c`: Raylib
  Map Lab rendering, cosmetic meshes, materials and viewer.
- `web/maplab/shell.html`, `docs/maplab/`: Map Lab source and compiled Pages output.
- `docs/index.html`, `docs/game.*`, `docs/web-build.json`: deliberate compiled
  Pages artifacts; regenerate with `scripts/build_web.sh`, never hand-edit.

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
./scripts/build_web.sh                         # Emscripten 6.0.9, Raylib web
python3 scripts/check_web.py                   # artifact hashes + actual WASM inference
./build.sh alienwars build/maplab --cpu --debug # terrain viewer, not training
./scripts/build_maplab.sh
python3 scripts/check_maplab.py                # native/WASM parity + navigation
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

For Pages releases, commit the authored source first, rebuild, run the WASM
check and inspect the real browser demo before committing the generated files.
Verify public deployment status and artifact hashes after pushing. The small
compiled Pages artifacts are an explicit exception to the general rule keeping
generated binaries out of Git; credentials, SDKs, raw logs and other build
products remain ignored. Do not publish an untrained fallback as a trained demo.

For Map Lab changes, follow `docs/MAPLAB.md`. Keep the strategic route plan,
road/tunnel/material WFC constraints, height surfaces, navigation and renderer coherent. Record a
generator-version change when seed outputs change. Validate rotational symmetry, cardinal edge
connections, all road lanes through floor 10, tunnel clearance and portals, resources and paths; test contradictions and
disconnected maps. Keep cosmetic props separate from authoritative collision.
The assembly view replays recorded cell resolution; do not call it a live solver
or claim it visualizes retries. New global-layout, combat or RL behavior needs
an explicit environment contract and appropriate validation.

Natural terrain must retain connected 3D tile geometry: shared corner/edge
profiles, shaped cliff transitions, slopes and continuous shorelines. Do not
replace it with independent flat-topped columns or use material-only WFC as a
substitute for geometric tile constraints. Compare real browser renders with the
accepted Map Lab v1 contour quality (source/artifacts at `03b52577`) when changing
terrain topology. Test complete boundary profiles and terrain/road/tunnel joins;
passing pathfinding tests alone is not sufficient visual acceptance. Use the
Tile boundaries inspection mode to check actual mesh continuation.

Caves use the shared implicit solid and fixed tetrahedral lattice in `caves.h`
and `volume.h`. Do not reintroduce a roof cap, a fixed tunnel floor, or exceptions
to shared surface-edge matching. A* owns global passage connectivity; passage
WFC owns compatible arch sockets. Collision and support queries must sample the
same tetrahedra as rendering. Preserve ramp entrances and allow intentional
surface breaches. Validate stacked passages, body clearance, support, portal
references and internal mesh-edge pairing in both native and WASM builds.

Global world variety is part of correctness. Preserve seeded landform count and
shape, variable base sites, generated road walks, enclosed lake basins and
procedural chamber routes. Keep the same-settings diversity regression in the
native/WASM release checks; unique hashes or prop/material changes alone do not
prove world variety. Respect the two diagonals and rotational symmetry.

The ocean grid in `ocean.h` spans 96 × 96 cells around the 64 × 64 land region.
Preserve the submerged outer land sockets, continuous shelf, draft-aware naval
routing and separation from enclosed lakes. Naval IDs are not surface/cave IDs.

Version 7 mountain regions use a connected-cycle WFC with two route arcs, then
solve grades and eight compatible arch profiles against the terrain. Preserve
actual covered travel, a larger-body traversable bypass, seeded topology and
rotational equality. Do not replace the route solver with stored footprints.
The existing deep cave A* and its four profiles remain separate. Walkable spans
come from the same meshed density as support/collision; never link stacked floors
by x/z alone. Keep sampled small/large-body clearance tests and the mountain
meshing fixture in native/WASM parity checks. This is the first regional grammar,
not the complete terrain-aware road-search/hydrology research roadmap.
