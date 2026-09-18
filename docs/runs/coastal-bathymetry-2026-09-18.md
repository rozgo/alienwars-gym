# Coast-following ocean floor — September 18, 2026

Source: `e0032dd055e1cbcecc7bc62dea62b95581d5264e`. Generator version 13.

The previous seabed stayed flat inside the square land grid and descended only
beyond its edges. Depth-based water shading exposed that square even when the
island itself was oval. The shared seabed now uses Euclidean distance from the
actual coastline, retaining the first submerged tile and descending smoothly
over the following eight tiles to the existing deep-ocean datum. Enclosed lakes
and dry beach grades are preserved. This is physical geometry used by rendering,
collision, navigation and sonar.

The reset-time cache adds 18,818 bytes per map. Sampling is constant-time and
does not allocate. Sonar intersects the same seabed triangles, including the
outer ocean belt; the historical generator-11 Navigation Lab is unchanged.

Validation on macOS arm64, Apple Clang 21, Emscripten 6.0.9 and Node 23.11:

- Native ASan/UBSan and WASM agree across 256 map seeds (`902c2041`), 48
  diversity worlds, 32 mountain/relief worlds and eight patrol worlds.
- New bathymetry fixtures verify Euclidean distance rather than a Manhattan
  diamond, oval coasts, rotational symmetry, enclosed lakes, shore preservation,
  all four land-grid seams, mesh/density agreement and unchanged RNG.
- Beach, cave mesh, bridge, occlusion and detail-map checks pass. Dry-layout
  diversity and mountain/bridge coverage remain unchanged from version 12.
- Sensors pass 960 independent mesh-ray comparisons in native and WASM,
  including curved outer seabed triangles. Cadence, observation packing,
  deterministic resets and zero step allocations pass.
- Sensor-only benchmark: 12 units, three worlds, 1,800 steps; 0.2428 ms per
  world-step native and 0.4459 ms WASM. These exclude rendering, generation,
  route preparation and training; they are local measurements, not CUDA claims.
- Shared route, collision, command, reset and allocation checks pass. The five
  family policies retain inference digest `6586e0b4` over 500 recurrent decisions
  and 2,000 categorical actions; the packaged policy bytes are unchanged.
- Packaged viewer parity, artifact hashes and JavaScript syntax pass. Chrome
  shows the corrected coastline shelf on the default Temperate world (seed 73,
  symmetric, floors 6, hash `f4b6bd9c`). Live rendering was observed at 56 fps.
  Asymmetric Desert (`1f760c98`) and symmetric Frozen (`58b8c42f`) coasts were
  also inspected; all three biome overview images are new browser captures.

No new GPU training or held-out evaluation was performed. Previous training
scores describe the earlier terrain distribution; they are not measurements
of the new underwater geometry. See the accompanying structured validation
and the gallery capture manifest for exact artifacts and captures.
