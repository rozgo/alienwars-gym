# First biological visual slice — September 18, 2026

Render-only changes: original scout, skiff, recon wing and nursery assets;
CC0 rock/forest-floor scans; fuller foliage and slight wind; seabed-depth water,
surface wakes and ground contact shadows; expanded Play view.

Validation completed on macOS / Chrome:

- Native optimized renderer: seed 73, Temperate, five frames, clean exit and no
  shader errors. AO bake: 559 ms, 582,421 samples. This is a single local smoke
  measurement, not a cross-device performance claim.
- Asset validation: 23,606 triangles across four meshes, finite unit normals,
  consistent winding, packed-file hashes and static vehicle bounds.
- Shared navigation tests: native ASan/UBSan and WASM agree on route planning,
  synchronous collision, adapter ownership/reset, command handling and map
  immutability. Five-family inference remains `6586e0b4` over 500 recurrent
  decisions / 2,000 categorical actions.
- Packaged Map Lab: headless seed comparisons, artifact hashes and JavaScript
  syntax pass. Seed 73 Temperate retains map hash `f6969e6d`.

No new training run is needed for these cosmetic changes. The five evaluated
policy files are retained byte-for-byte. The historical Navigation Lab also
packages the new shared-renderer assets while retaining its evaluated checkpoint.
See [art pipeline](../ART_PIPELINE.md) for the visual scope and limitations.
