# AlienWars Map Lab

[Open the Map Lab](https://rozgo.github.io/alienwars-gym/maplab/?seed=73).

This is the terrain foundation for AlienWars: a shared C map generator and
navigation graph, with a native Raylib viewer and an Emscripten/WebGL browser
build. The scout follows a scripted shortest path. Combat, harvesting, and an
AlienWars RL policy are not implemented in this milestone. The trained
[Breakout baseline](https://rozgo.github.io/alienwars-gym/) remains available.

## World contract, generator version 1

- 48 × 48 cells; each cell spans two rendering units.
- Water, lowland, and one raised plateau level. True 3D meshes; one traversable
  surface per cell. Overlapping bridges and caves are outside this version.
- Two starting plateaus, a central area, two expansion areas, four resource
  markers, and two ramps. Ramps are four cells long and three cells wide.
- Fixed strategic anchors define this first map family. Seeded radius variation,
  noise, WFC choices, and scenery vary coastlines and plateau boundaries. This
  is not yet an arbitrary strategic-layout generator or a balance guarantee.
- Decorative rocks, crystals and markers do not block movement. Tile navigation
  is authoritative; cosmetic scenery must not be used as collision geometry.
- The same unsigned 32-bit seed and generator version produce identical terrain
  IDs, ramp IDs and map hashes on the verified native and WebAssembly builds.

## How WFC is used

`ocean/alienwars/map.h` contains the entire renderer-independent core. There
are 31 tile patterns: all binary combinations of four corner heights for
water/lowland and lowland/plateau, with the duplicate all-lowland tile removed.
Adjacent edges must agree on both corner heights. A tile cannot span water
directly to a raised plateau.

The macro layout constrains vertex domains around starting areas, routes and
ramps. Coastlines and plateau boundaries retain uncertainty. A minimum-remaining-
values heuristic selects a cell, seeded weighted selection picks a tile, and a
queue propagates edge constraints. This is genuine simple-tiled constraint
propagation; the entropy heuristic uses integer option counts, not floating-
point Shannon entropy. Contradictions return failure. Up to eight deterministic
attempts are allowed, followed by an explicit error; there is no unvalidated
fallback map.

The visible assembly is a replay of the successful solver's recorded resolution
order. It includes the initially constrained cells and cells resolved by
propagation. It is not a live, time-sliced solve, and it does not visualize
discarded retry attempts. Pause and single-tile stepping inspect the record.

The renderer clips tile triangles into flat terraces and joins their contours
with vertical rock faces. Neighboring tiles share displaced grid vertices, so
their geometry meets. Ramps use quarter-level surface heights shared by
navigation and rendering. Terrain/scenery are batched into meshes; procedural
shaders add stone variation, directional lighting, water and shoreline ripples.
All geometry and materials are authored in this project; the reference images
are visual direction, not bundled game assets.

## Navigation and acceptance checks

Flat land cells and ramps are traversable. Cardinal neighbors connect only when
their shared surface edge heights match exactly. Flood fill checks both bases,
the center and all four resource markers. Every lane across both ramps must
connect. Isolated cosmetic land pockets can exist; the walkability overlay
shows them in red, connected ground in mint, and ramps in amber.

BFS produces the base-to-base path. The visible scout travels between cell
centers along that path and follows ramp elevation. It traverses the route in
both directions; the route remains visible while the scout is paused.

`tests/alienwars/map_test.c` exercises 256 seeds, including 0, 1, 73, and
4,294,967,295, and verifies:

- repeatable terrain, complete resolution records, and actual WFC choices;
- all adjacency constraints, symmetric movement edges, and path continuity;
- spawn/resource/central connectivity and three-cell-wide ramp connections;
- invalid path endpoints, same-cell paths, and water endpoints;
- rejection of a deliberately disconnected ramp and contradictory corner pins.

Native ASan/UBSan and WebAssembly run the same tests. Their aggregate hash must
match. The checker also compares four seeds through the actual packaged viewer,
checks generated asset hashes and embedded local paths, and parses/checks page
JavaScript. Browser rendering and controls are verified separately.

## Build and inspect

Use the same Raylib 5.5 and Emscripten 6.0.9 tooling as the Breakout release:

```sh
./build.sh alienwars build/maplab --cpu --debug
./build/maplab --seed=73
./build/maplab --headless --seed=73
./scripts/build_maplab.sh
python3 scripts/check_maplab.py
python3 -m http.server 8781 --bind 127.0.0.1 --directory docs
```

Open `http://127.0.0.1:8781/maplab/`. Browser controls support orbit, pan, zoom,
seed loading, regeneration, a shareable seed URL, assembly replay, pause/step,
walkability and path overlays, and scout pause. Reduced-motion preferences
pause the scout initially. Camera buttons provide an alternative to gestures.

The Map Lab uses the existing `build.sh` per-environment `web.sh` hook and the
standalone viewer branch for `--cpu`. `./build.sh alienwars` is not yet a
training target: the PufferLib observation/action/reward interface comes next.
Keep `map.h` free of Raylib, browser, and filesystem dependencies so it can be
used directly in that environment. Generate maps during setup/reset or load a
validated map pool; never run WFC in the simulation step loop.

## Publish

Authored sources: `ocean/alienwars/`, `web/maplab/shell.html`, and
`scripts/build_maplab.sh`. Published output: `docs/maplab/`.

Commit source first, rebuild from that clean revision, run the checker and
inspect the real browser, then commit `docs/maplab/index.html`, `maplab.js`,
`maplab.wasm`, and `build.json`. Push to `main`; GitHub Pages serves `/docs`.
Check the deployment commit and compare public artifact hashes to `build.json`.
There are no external runtime assets, services, or credentials in the Map Lab.

## References

- [WaveFunctionCollapse simple tiled model](https://github.com/mxgmn/WaveFunctionCollapse#tilemap-generation)
- [PufferLib web build](https://puffer.ai/docs.html)
- [Raylib 5.5](https://github.com/raysan5/raylib/releases/tag/5.5)
