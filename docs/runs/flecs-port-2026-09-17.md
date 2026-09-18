# Flecs port — September 17, 2026

The current twelve-unit fleet runs on Flecs 4.1.6 component storage through the
C API, shared by the Raylib browser viewer and the native PufferLib 5 trainer.
Existing policies, terrain generation, navigation, sensing and numerical control
remain unchanged. See [the architecture](../FLECS.md) and the
[measurement record](flecs-port-2026-09-17.json).

## Behavior and cost

The saved pre-port baseline is `179cbf02`. The initial port is `12af8262`;
`eebca7fe` adds the POSIX feature macro required by strict C11 on Linux. Upstream
Flecs files remain byte-identical to release v4.1.6, with recorded hashes and MIT
license. Browser artifacts include dependency and source provenance.

Both backends reproduce their respective pre-port traces over 1,200 world steps,
including resets, observations, rewards, terminal events, pose and velocity.
The trace quantizes floats at 1e-5; it is not a cross-platform bitwise equality
claim. Native digest: `4c97cac7`; WASM digest: `1e3a8cff`. Existing native/WASM
inference still agrees for 500 recurrent decisions and 2,000 categorical actions
with digest `6586e0b4`.

| Timed step loop | Pre-port median | Flecs median | Ratio |
| --- | ---: | ---: | ---: |
| Native C | 0.711432 s | 0.700237 s | 0.984 |
| WebAssembly / Node | 1.069585 s | 1.073428 s | 1.004 |

Three samples per version/backend exclude map preparation. These short CPU-time
measurements show essentially unchanged step cost, not an established speedup.
Other validation work ran concurrently; exact samples and compiler versions are
in the JSON. The timing run used clean source `12af8262`; the later Linux-only
clock declaration fix does not change its macOS or WASM simulation code.

Flecs has a memory cost: native live-world storage rises from 202,568 bytes to
312,587 bytes, approximately **107 KiB extra per world** (2.52 MiB for 24 worlds).
This includes 256,139 retained Flecs heap bytes plus the 56,448-byte owner and
sensor buffers, excluding allocator metadata and shared terrain/route banks.
`FLECS_LOW_FOOTPRINT` reduces the default ECS allocation substantially. The WASM
fixture retains 229,107 heap bytes per world plus its 56,416-byte owner.

Twenty-four simultaneous worlds pass ownership, independent-state, stable-slot,
allocation-free step/reset and complete-teardown checks. Native ASan/UBSan runs
pass on macOS and Linux; the matching WASM fixture passes. The active shared
adapter also checks application and Flecs allocation counters.

## PufferLib and CUDA

On an RTX 4090, the clean `eebca7fe` checkout completed **147,456 agent steps**
(eight updates, 24 worlds, twelve units/world) with five independent family
learners. All five loaded the deployed checkpoint hashes exactly, changed their
weights, and produced finite parameters and metrics. The task used traffic
curriculum 1, training maps 301–302, seed 6373, 645 observations, 4/3/3/3 action
heads, and two 128-wide recurrent layers. Each final checkpoint is 730,624 bytes.

Trainer time: 3.198 s; process time including preparation: 10.025 s. The final
reported interval was 50,970 agent steps/s. These are smoke-run measurements,
not a new learning-quality result or a before/after GPU speed comparison.
CUDA gather checks cover all five unequal family slices, both buffers and
recurrent-state layout. The ordinary single-policy path also completed an
8,192-step Minimal regression with finite metrics and weights.

The native build uses:

```sh
CUDA_HOME=/usr/local/cuda ./build.sh alienwars_shared build/puffer-shared
```

The JSON retains exact training/build/check commands,
resolved configurations, compiler versions, timings and checkpoint hashes.
Raw logs and smoke checkpoints are in ignored
`outputs/flecs-validation/run-20260917/`. The smoke weights do not replace the
evaluated browser release policies. Existing navigation failure rates and
deadlocks documented in the previous report still apply.

Self-play was considered and deferred: this pass validates storage compatibility,
without changing the RL objective or training a competitive faction policy.
The next gameplay/RL slice still requires the documented self-play assessment.

## Viewer and terrain

Native debug builds and headless checks pass. The full terrain suite passes
256 native/WASM seeds, 48 diversity worlds, mountain/tunnel/bridge geometry,
manifold winding, occlusion and detail continuity. Generator version remains 11.

Chrome spot checks cover symmetric Desert and asymmetric Frozen seed 73 worlds,
all twelve profiles and five loaded policies, fixed-wing/quad/submarine movement,
live sensor readings, equipment removal/restoration, single/all-unit overlays,
underwater visibility, tunnel isolation, fleet restart, world reconstruction and
an accepted aerial destination through the canvas (1/1 routed). No runtime or GL
errors appeared in the captured console log.
The initial world took about 30.6 s to construct while other validation jobs ran;
steady displayed frame rates ranged from 48 to 60 fps. These are observations,
not a controlled graphics benchmark. Existing synchronous reconstruction can
temporarily block browser automation until the new world is ready.

The current ECS has fixed topology. Biological births/deaths, organs, cultivation,
combat and faction warfare remain future work. Terrain cells and cosmetic props
retain specialized storage. Flecs makes a foundation for that work; this port
does not claim the design exploration is already implemented.
