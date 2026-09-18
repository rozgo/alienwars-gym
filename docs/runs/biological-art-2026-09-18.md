# Biological fleet and alien terrain — September 18, 2026

Source: `8164781ebb751143af064f981d088f097c3fabd7`. Generator version 12.

All twelve current Map Lab roles and both bases now use original biological
meshes. Temperate fungi, Desert mushroom-cacti and Frozen frost harps replace
conventional trees. Four 1K CC0 scans, biome materials, stronger baked/cast/contact
shadows, sampled seabed water, shore foam and swimming wakes form the visual kit.
Temperate uses blue-green groundcover, warm fungal soil and cool stone.

The generator fits roads and landforms inside a noisy horizontal ellipse before
height pinning. Shared quarter-height beach shelves and damp shore materials
connect low land to oceans and lakes. Sea level remains 1.44 quarter floors;
rendering, dry-shore support and naval navigation use the same geometry.

Validation on macOS / Chrome:

- Thirteen assets: 89,142 triangles, 9,627,440 packed bytes. Hashes, finite normals,
  triangle winding and static/animated physical envelopes pass.
- Native renderer: seed 73, Temperate, five frames, clean exit without shader
  errors. One AO bake measured 759 ms / 591,990 samples; this is a local smoke
  measurement, not a cross-device performance claim.
- Native ASan/UBSan and WASM: 256 map seeds agree (`9bafdc60`), as do 48 diversity
  worlds, 32 mountain/relief worlds, beach fixtures, cave meshes, occlusion,
  detail maps and patrol routes. The mountain cohort includes 12 optional
  regions. The relief cohort includes 17 bridges in 11 worlds, with at least
  32 walkable slopes per world.
- The oval footprint and graded shores reduce suitable flat bridge banks.
  Version-12 aggregate coverage bounds were explicitly rebased to 10/32 bridge
  worlds (at least five per symmetry mode) and 28 minimum walkable slopes,
  versus the old 16/32 and 40. Individual bridge water-gap, bank-join, clearance,
  support and rotational-pair checks are retained. No bridge is forced into an
  unsuitable site to meet a frequency target.
- Shared route, collision, command and allocation checks pass in native/WASM.
  Five-family inference is still `6586e0b4` over 500 recurrent decisions and
  2,000 categorical actions. All five evaluated policies are packaged byte-for-byte.
- Packaged viewer seed parity, artifact hashes and JavaScript syntax pass.
  Default seed 73 / symmetric / floors 6 / Temperate has hash `9dd82f86`.
- Chrome inspection covers all twelve bodies, through-water/terrain visibility,
  sensor overlays, live Flecs inspection, three alien ecologies and Play view.
  Updated gallery images are real browser captures; see their capture manifest.

The historical Navigation Lab remains on generator 11 and retains checkpoint
`ec8c3c85f688e79704a585068434c7eb1b8e4d356585ed28ceb8fb43071fcc5a`.
No new GPU training or held-out policy evaluation was performed in this art pass.
Earlier training scores describe the older terrain distribution, not v12.
Vegetation and small props remain cosmetic; gait/contact shadows and membrane
lighting are approximations. Cultivation and combat are not implemented here.

See [structured validation](biological-art-2026-09-18.json) and
[art pipeline](../ART_PIPELINE.md).
