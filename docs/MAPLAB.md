# AlienWars Map Lab

[Open Strata Frontier](https://rozgo.github.io/alienwars-gym/maplab/?seed=73).
Shared C generation, collision and navigation, rendered with Raylib/WebAssembly.
The scout is scripted; combat and an AlienWars RL policy are not implemented.
The [trained Breakout baseline](https://rozgo.github.io/alienwars-gym/) is preserved.

## World contract — generator version 5

The world has 64 × 64 terrain tiles, two world units per tile. A floor is three
world units; elevations use quarter floors. Bases support floors 1–10. Surface
shape WFC retains the connected contour terrain introduced in version 3.

Caves are **volumetric Boolean excavations**, with signed elevations, ramps,
arched sections and junctions. There is no fixed second floor, roof tile, or
rectangular tunnel box. The model supports multiple empty intervals in one
vertical column. A fixture verifies three stacked passages, both inside a
mountain and below the ground datum, separated by solid rock.

Tunnel entrances occupy the **northeast/southwest diagonal**, opposite the
northwest/southeast bases. The planner ranks flat, dry, surface-reachable sites
in those regions using seeded regional targets and available approach space.
A shortest surface approach from the existing roads is reserved before material
WFC, so lava cannot cut it off. Approach materials change without overriding
terrain elevation sockets or their curved edge profiles.

Each entrance starts at surface floor 1 and descends toward a wide underground
chamber. Chamber positions and depths vary by seed, currently from floor -1 to
-2.5. A* connects the chambers through the generated terrain. There is no fixed
central entrance, shaft, waypoint or ring. Symmetric maps mirror the entire
entrance/approach/chamber/route plan and its WFC profiles through 180 degrees;
asymmetric maps choose each side independently, including different depths.
Mirrored connector routes can form a loop or merge into a broader passage.

Surface openings are permitted along the first eight tiles of an entrance
approach; later passage sockets require rock cover. The surface is never raised
to hide a tunnel. Where excavation intersects the terrain, it creates an actual
opening. Roads retain their support above the network.

The planner tries up to 16 seeded alternatives on the same surface. Each must
pass WFC, body/support checks, surface access to each mouth and a cave-only
crossing between them. Exhaustion reports generation failure; it never silently
substitutes the old central layout. Seeds, settings and generator version
identify the world. Version 5 changes cave layouts and approach materials;
version 4 URLs regenerate using the new generator.

This is a bounded, static terrain milestone. It does not yet generate arbitrary
strategic graphs, mine shafts, elevators, destructible terrain or simulated
flooding. Navigation is a conservative surface/centerline graph; it is not a
full free-roaming cave navmesh. Decorative props do not participate in collision.

## Why this algorithm

Use different constraints for different jobs:

1. **Strategic plan and road WFC.** Existing seeded road corridors connect base
   pads and resources. Quarter-floor socket domains propagate pinned endpoints,
   flat bends and ramps with at most one quarter-floor rise per tile. Every road
   lane, both bases, the upper crossing and all resources must remain connected.
2. **Terrain shape and material WFC.** Sixteen corner patterns resolve shared
   elevation sockets. Every neighbor must match both corners and its complete
   sampled edge profile, including tiles above caves. Beveled cliff transitions
   preserve flat shelves; road support weights join the surrounding soil to
   road grades. Twelve material domains enforce biome compatibility and blend
   colors across shared vertices. No cave-specific shape-adjacency exceptions.
3. **3D A* passage planning.** Search states include x/z position, signed
   elevation, heading and previous grade. Cardinal flat/ramp moves enforce
   maximum grade, level turns and flat entry/exit sockets. Required deep
   chambers establish seeded excavation depths. Dry terrain, road/approach support, overburden
   away from entrances and separation from existing ramps constrain the search.
   Cost includes distance, excavation depth and deterministic spatial variation;
   the admissible heuristic uses horizontal distance and required elevation
   change. Failed searches reject the map, without a geometric fallback.
4. **Passage WFC.** Four arched profiles range from narrow to chamber width.
   Chamber sockets pin a wide profile; graph-vertex sockets encode width and headroom, and sweeps share their exact
   endpoint profiles. Portal pins, narrow ramps, cover, maximum one-profile
   transitions and rotational equality propagate before MRV collapse. The
   solver has bounded backtracking (4,096 failures). WFC chooses local passage
   geometry; A* supplies the global route connectivity.
5. **Boolean terrain.** The implicit solid is the intersection of terrain below
   the surface with space outside the union of passage sweeps. Floors, walls,
   arched ceilings and natural mouths come from this one function. Flat turns
   use rounded junctions. Segment ends overlap by a quarter tile and continue
   their floor planes, avoiding zero-thickness internal cap artifacts. These
   are scalar fields, not exact signed distances: do not sphere-trace them.
6. **Polygonization and validation.** Marching tetrahedra extracts the boundary
   on a shared lattice: 1/6 tile horizontally and 1/2 quarter floor vertically.
   A fixed six-tetrahedron decomposition gives identical face diagonals between
   cells. The same barycentric tetrahedron sampling drives collision, support
   queries and camera visibility. Test the generated geometry and navigation;
   reject contradictions or disconnected/unsupported passages.

Local WFC adjacency alone does not guarantee global connectivity. See the
[original WFC constrained-synthesis notes](https://github.com/mxgmn/WaveFunctionCollapse#constrained-synthesis)
and [DeBroglie's non-local constraints](https://boristhebrave.github.io/DeBroglie/articles/constraints.html).
Implicit fields and boundary extraction remove height-map topology restrictions;
see [Boris's polygonization introduction](https://www.boristhebrave.com/2018/04/15/marching-cubes-tutorial/)
and [Lengyel's volumetric-terrain discussion](https://transvoxel.org/).
This implementation uses fixed-resolution marching tetrahedra, **not Transvoxel
LOD**. Variable resolution would require a separate seam strategy.

The assembly animation replays the surface shape solver's recorded resolution
order. It does not animate A* exploration, passage WFC or retries. Structure
counts include road and passage WFC decisions.

## Navigation and inspection

Surface cells use cardinal matching edges, legal terrain cost and at most one
quarter-floor height difference. Caves use independent graph nodes carrying
position, elevation and passage profile; a node ID is not a fixed layer index.
Only explicit portals connect these graphs. Stacked crossings acquire no
implicit navigation links. Indexed-heap Dijkstra uses terrain and grade costs.

Passage validation samples the meshed solid around a scout body of radius
0.3 tile and headroom 1.8 quarter floors. Support is located in the composed
field rather than assumed from the nominal route elevation. Floor joins must
stay within 0.8 quarter floors of the planned grade and change by no more than
0.26 quarter floors per 1/8 tile sample. Conservative centerline traversal does
not imply that every point in a wide chamber is navigable.

| Terrain | Base movement cost |
| --- | ---: |
| Road / cave | 10 |
| Grass | 12 |
| Soil | 13 |
| Rock | 16 |
| Sand | 18 |
| Forest | 19 |
| Snow | 20 |
| Ice | 23 |
| Mud | 28 |
| Shallow water | 34 |
| Deep water, lava | Blocked |

Violet markers identify both tunnel mouths in the landscape view. **Tunnel
floors NE / SW** reports the two chamber elevations. **Inspect entrance** frames
the northeast entry and pauses the scout.
**Inspect tunnel** frames the deepest point of a cave-only route between the
entrances. Unpause and enable **Follow scout** to traverse that route. Load a
world again to restore the base-to-base route.

**Isolate tunnels** hides the landscape, ocean and surface props, then frames
the complete passage network at every depth. **Show tunnel ceilings** switches
between an open-top view of floors and ramps and the full arched shell. Both
views use the actual excavated terrain triangles, rendered from either side;
entrance floors are retained where they meet the surface. **Passage guides**
shows the entire cave graph and marks entrances. Toggle isolation off to return
to the previous landscape camera and cutaway settings. These controls only
change rendering and camera state: they do not regenerate the map or replace
the scout's route. Shared URLs retain `isolate=1` and optional `ceilings=1`.

**Auto cutaway** tests the orthographic camera-to-scout ray against the meshed
solid and removes intervening fragments above the scout. **Horizontal section**
clips geometry above a plane near the scout to expose the network. **Off** shows
the complete opaque mountain. Cutaways never change collision or the map hash.
A land mask prevents the decorative ocean from covering underground interiors.
**Tile boundaries** shows retained shared surface profiles; excavated portions
are omitted.

## Rendering

The renderer uses a neutral daylight treatment with physically based material
response (GGX specular, roughness, Fresnel and filmic tone mapping). World-space
stone grain, weathering, soil variation and wet shore tint continue across tile
boundaries. Volcanic regions use cooled crust with narrow emissive fissures.
A terrain-derived ambient occlusion texture adds contact depth.
A static 2,048-square shadow map captures the terrain and decorative props;
nine filtered depth comparisons soften the edges. Shadows are suppressed during
assembly and cutaway/isolated inspection so removed surfaces do not obscure the
view.

`props.h` builds seeded cosmetic meshes: tapered trunks, roots, branches,
twigs and two-sided leaves, broadleaf trees and conifers, irregular smooth
boulders, pebbles, grass tufts, mineral clusters and industrial outposts. These
use a separate hash stream and do not change WFC, navigation or collision.
All geometry and materials are authored in C/GLSL; there are no external model,
texture, CDN or asset-license dependencies.

The ocean has animated wave normals, Fresnel reflection, deep/shallow color,
subtle caustics, restrained sun highlights and moving shoreline foam. It samples
a planar reflection of the actual terrain and static props. The reflection is
1,024 pixels wide, follows the viewport aspect ratio (height bounded to
256–1,536), and refreshes when the camera, viewport or assembly changes. A still
view reuses the reflection while waves keep moving. Shore distance and ambient
occlusion share a 256-square texture. Frame rate is visible under **Map checks**.
Regeneration paints a busy state and disables controls before the synchronous
WASM work runs outside the input event. Shader inputs retain the rank attribute
required by Raylib 5.5's non-VAO path; the default batch's unused normal slot is
also initialized to prevent invalid WebGL attribute calls.

This is a first realism pass, not a finished AAA asset pipeline. Vegetation is
static, there is no reflection of the moving scout, and the ocean is a visual
surface rather than a fluid simulation. The single shadow map loses detail at
extreme zoom. Mesh construction and GPU upload happen at world creation;
**Generation** reports only the authoritative generator, not these rendering
costs. Generator version 5 and its seed hashes are unchanged by this pass.

## Validation and release

`tests/alienwars/map_test.c` runs 256 seed/configuration cases covering the full
symmetry × ten floors × five palettes × tunnel-toggle matrix. It checks repeat
hashes, shape/material symmetry, mirrored passage graphs, all road lanes,
resources, cave reachability, graph reciprocity, path costs, full boundary
profiles, field-based body clearance, retained road support, signed depths and
entrance references. Checks also require entrances on the other diagonal,
independent surface access to both, cave-only connectivity, mirrored graph edges,
and variation across seeds in entrance locations and depths.
Deliberate failures cover disconnected passage links, missing portals, invalid
materials and incompatible road, terrain and passage sockets.

`tests/alienwars/volume_test.c` extracts three stacked passages. It checks empty
and solid intervals and verifies that every internal mesh edge is paired with
opposite orientation. Region perimeter edges are explicitly accounted for, and triangle centroids
must agree with collision sampling. It
also checks that an excavation intersecting the surface produces an opening.
Native ASan/UBSan and actual WebAssembly execute both suites and must agree.

```sh
./build.sh alienwars build/maplab --cpu --debug
./build/maplab --seed=73 --floor-a=10
./build/maplab --headless --seed=73 --symmetry=0 --floor-a=10 --floor-b=3
./scripts/build_maplab.sh
python3 scripts/check_maplab.py
python3 -m http.server 8781 --bind 127.0.0.1 --directory docs
```

The checker also compares four configurations through the packaged viewer,
verifies artifact hashes and checks JavaScript syntax. Real Chrome review must
cover the entrance, interior, opaque mountain, section, auto cutaway, isolated
network with and without ceilings, and terrain continuation. Verify unchanged
hash and route when toggling isolation. Generation timing in the UI excludes
mesh construction.

For Pages, commit authored source first, rebuild from that source revision,
validate and inspect the real browser, then commit generated `docs/maplab/`
artifacts. Push to `main`; Pages serves `main:/docs`. Verify the deployment and
public hashes. Keep SDKs, credentials, raw logs and local binaries ignored.

The PufferLib observation/action/reward contract remains future work. Keep
`map.h`, `caves.h` and `volume.h` independent of Raylib, Python and browser APIs.
Generate worlds at reset or use validated map pools, never solve during a step.
