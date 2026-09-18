# Biological fleet and alien ecologies

All twelve current fleet roles and both bases have authored biological bodies. Segmented shells, structural ribs,
membranes and small luminous forward organs establish a shared visual language.
All units face +Z in model space and retain their existing physics dimensions.
Ground roles use distinct limb and shell arrangements; surface swimmers use frills and flotation organs; the hover body has four radial lift organs; fixed wings have forward flight membranes; submarines have different pressure bodies, fins and storage lobes.

The Blender source, runtime format, material provenance and rebuild commands
are in [the asset kit](../resources/alienwars/art/README.md). Art is owned by the
viewer, loaded once and reused through world resets. It never enters Flecs
training components, observation buffers, sensor rays, collision or rewards.
Existing trained policies are unchanged. The accompanying generator-v12 change
introduces oval coastlines; it is documented separately in [Map Lab](MAPLAB.md).

The environment combines four 1K CC0 photographic material scans with procedural
variation, shared material weights and the existing GGX terrain lighting.
Rock uses two sampling scales to reduce repetition, grass receives a leafy turf
scan, and fibrous fungal stalks reuse the bark scan as monochrome tissue detail. Texture height drives surface-gradient
normals without displacement. Snow keeps
its finer procedural treatment; paths suppress relief. Traditional trees have been replaced by original alien flora in `alien_flora.h`.
Temperate uses fungal parasols, spiral membrane fans and pitcher-like pod towers.
Desert uses swollen ribbed mushroom-cacti with amber shade caps. Frozen uses
rooted frost-harp colonies with curled blue-white fins. Climate-specific
colors, ribbed skins, raised veins, restrained emission and approximate membrane
backlighting establish the alien canopy. Membranes are opaque two-sided geometry;
this is a light-transmission shading approximation, not sorted transparency. Low
fern clumps and groundcover retain subtle shader motion. Additional small rock clusters sit in nonwalkable cells
outside roads, bridges and cave/trail cells. Props
remain cosmetic; this pass adds no new navigation obstacles.

Temperate combines blue-green groundcover, warm fungal soil and cool stone.
Desert stone has muted plum strata and warm ribbed soil; frozen ground has
blue-violet stone and fine blue mineral seams. A signed shore-distance mask
blends dry ground through damp beach material into shallow water, including
inland lakes. This mask changes shading only; actual beach shelves and dry
shore navigation come from the generator-v12 shared terrain.

Water now uses sampled seabed depth as well as shoreline distance, with moving
shore foam, cached planar reflections and local wakes from the three surface
boats. Wakes stop when bodies stop or traffic is hidden. They are visual cues,
not fluid simulation. Ground bodies receive soft contact shadows sampled on
their current support layer. Existing static cast shadows and baked terrain
occlusion remain intact during cutaways. Baked occlusion has a stronger ambient
response, the shadowed key retains 22% of sunlight, and moving ground bodies have
deeper contact shadows. Ambient fill prevents completely black terrain.

Walking limbs and swimming frills move with distance actually travelled by the
rendered body. Pausing, failed movement and teleports do not continue a fictitious
walking cycle. The wing has only subtle membrane flex; existing fixed-wing speed,
turning, bank and pitch still control its pose. Articulation is intentionally
small, without a skeleton or foot-placement solver.

**Play view** expands the battlefield and tucks away the inspector, keeping
camera controls and selected-unit status. **Show controls** restores the full
lab. `view=play` shares the expanded view; toggling it resizes the viewport without
resetting the camera, world or simulation.

## Validation

- `uv run scripts/art/check_assets.py`: packed file lengths/hashes, finite unit
  normals, triangle winding, material attributes and static and animated physical envelopes.
- `uv run scripts/check_shared.py`: native/ASan and WASM route, collision,
  command, allocation and five-policy inference contracts.
- `./build.sh alienwars build/maplab-art --cpu` then
  `./build/maplab-art --seed=73 --biome=1 --frames=5`: bounded native GL smoke,
  including asset loading, shaders, rendering and cleanup.
- `uv run scripts/build_fleet_site.py`: packages the same five verified
  evaluated checkpoints with the art kit.
- Real Chrome review: three biomes, close-ups, through-terrain bodies, sensor
  overlays, tunnel isolation, motion/pause, regeneration and Play view.

## Boundaries and next art work

This establishes an art pipeline and a first reusable kit, not finished AAA art.
Next: sculpt distinctive faction materials, add proper skeletal gait/foot
placement and author biological bridge
and resource forms. Avoid raising environmental contrast at the expense of units
and sensors. Vegetation is still generated geometry; groundcover motion uses static shadows
and cached reflections. Large fungal crowns are static. Moving units do not appear in the
planar reflection. Contact shadows are approximate, and wakes follow the current
boat pose rather than retaining a simulated trail.
