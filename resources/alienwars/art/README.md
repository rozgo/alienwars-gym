# AlienWars viewer art

This small, deliberately checked-in asset kit is packaged with the Map Lab
WebAssembly runtime. Training does not load it. No runtime CDN or network
requests are needed beyond the site's own files.

## Original biological meshes

The thirteen `.awm` meshes listed in `models.json` are original AlienWars assets,
covered by the repository's MIT license. Rebuild with Blender 5.0.1:

```sh
/Applications/Blender.app/Contents/MacOS/Blender -b --python scripts/art/build_biological_assets.py
python3 scripts/art/check_assets.py
```

The script is the editable source; it also saves editable Blender scenes to
ignored `outputs/art/`. The runtime format is `AWM1`, a little-endian uint32
vertex count followed by triangle vertices: position (3 floats), normal (3),
material class (1), articulation weight (1), RGBA (4 bytes). Game coordinates
are +Y up, +Z forward. `models.json` records bounds, counts and SHA-256 hashes.
The thirteen meshes total 89,142 triangles and approximately 9.18 MiB. Each unit is
one reusable GPU mesh. The nursery is incorporated into the scenery and its
existing static shadow/reflection and occlusion passes.

All twelve current Map Lab units use these authored biological bodies.
Each variant has a distinct silhouette and shares the same +Z forward convention. The nursery is
a visual base form; cultivation, growth, combat and faction species are not
implemented by these assets.

## CC0 scanned terrain materials

Powered by [Poly Haven](https://polyhaven.com/). These scans are released under
[CC0 1.0](https://polyhaven.com/license):

- [Forest Ground 04](https://polyhaven.com/a/forest_ground_04)
- [Rock Face 03](https://polyhaven.com/a/rock_face_03)
- [Leafy Grass](https://polyhaven.com/a/leafy_grass)
- [Bark Brown 02](https://polyhaven.com/a/bark_brown_02)

`forest.png`, `rock.png`, `grass.png` and `bark.png` pack 1024×1024 sRGB albedo in RGB and linear height in
alpha. Original 1K diffuse/displacement JPEGs are downloaded and hash-verified
by `scripts/art/build_materials.py` (Python + Pillow). `materials.json` contains
source URLs, source MD5s, transformations and output SHA-256s. Source downloads
are cached outside Git in `outputs/art/source`. Rebuild only at authoring time:

```sh
python3 scripts/art/build_materials.py
python3 scripts/art/check_assets.py
```

World-space triplanar sampling keeps detail continuous across terrain tiles,
cliff walls and tunnel ceilings. Mipmaps filter distant detail; height modifies
shading, never the authoritative surface or navigation clearance.
