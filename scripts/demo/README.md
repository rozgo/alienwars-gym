# AlienWars Gym showcase

Revision 4 is a silent, 59-second showcase with twelve worlds, all-unit sensor
overlays, smooth camera tracks, real training metrics and a trained navigation
checkpoint. The user approved merging and publishing this revision after local
review. Earlier cuts remain in outputs/demo/, outputs/demo-v2/ and
outputs/demo-v3/. These scripts only create local artifacts.

## Build, capture and edit

Requires Chrome, Node.js, Puppeteer Core 24.37.5, FFmpeg with libx264, Python with
Pillow 12.2.0, and the existing Emscripten 6.0.9 / Raylib 5.5 web tooling. Optional
dependencies belong in ignored .local/ directories. The editor uses macOS Menlo.

Run from the repository root:

```sh
python3 scripts/demo/build_capture.py
python3 -m http.server 8769 --bind 127.0.0.1 --directory build/demo-web
```

For the current revision, reuse the reviewed revision 3 source footage and
recapture only the corrected closing card. With that server running:

```sh
DEMO_OUT=outputs/demo-v4 node scripts/demo/record.cjs --shot=13-closing
python3 scripts/demo/prepare.py
python3 scripts/demo/edit.py
```

prepare.py reads the two supplied clips and manifest from the sibling
alienwars-gym-rl worktree's outputs/navigation/demo directory. It verifies their
SHA-256 hashes, leaves those files unchanged, and records source in points,
durations, playback rates and source hashes in the assembled capture manifest.
The observatory uses its first four seconds. Navigation Lab retains all eight
seconds at its original speed. Its trained bridge crossing replaces the earlier
Map Lab ground bridge take. The twelve-world gallery follows the tunnels.

CHROME_PATH overrides Chrome, DEMO_BASE the capture server, DEMO_RL_CLIPS the
supplied-clip directory, and DEMO_SOURCE the reviewed source footage directory
(default outputs/demo-v3/). DEMO_OUT selects output: the recorder/gallery default
to demo-v3, while preparation/editor default to demo-v4. To regenerate all source
footage without overwriting a reviewed cut:

```sh
DEMO_OUT=outputs/demo-source node scripts/demo/gallery.cjs
DEMO_OUT=outputs/demo-source python3 scripts/demo/gallery.py
DEMO_OUT=outputs/demo-source node scripts/demo/record.cjs
DEMO_SOURCE=outputs/demo-source python3 scripts/demo/prepare.py
python3 scripts/demo/edit.py
```

The build creates an ignored filming copy of the current Map Lab. Production
sources and docs/maplab/ remain untouched. Generation, physics, navigation,
sensor mounts and measurements are unchanged. Capture-only adjustments:

- Frame-synchronous quintic camera orbits and damped follow avoid the integer
  mouse-delta stutter of the earlier recorder.
- Stronger all-unit sensor opacity and thicker radio links keep cached RF
  bearing/range connections visible after video compression.

The build manifest records those adjustments and artifact hashes. The build
script checks its camera insertion anchor and RF drawing expression before
instrumenting the copy. Capture controls do not ship to Pages automatically.
Feature switches still use the real UI. All units enables overlays on units
with the corresponding module attached; filming never changes equipment.

Chrome runs headless at 1920 × 1080 using .local/demo-chrome. Headful
screencasting can clip to the physical display even when a screenshot looks
correct. The editor checks the first actual screencast JPEG of every take.
CDP timestamps are resampled to 30 fps.

## Edit sequence

| Time | Scene |
| --- | --- |
| 00–03 | Original seed 73 overview and title |
| 03–07 | All-unit laser range sensing |
| 07–11 | All-unit radio range and bearing connections |
| 11–15 | Air unit, depth preview and all-unit fields of view |
| 15–19 | Naval patrol and sonar, seed 175847449 |
| 19–22 | Ramp entrance into the frozen world's underground network |
| 22–27 | Underground travel and automatic terrain cutaway |
| 27–31 | Symmetric four-chamber network and mountain passages |
| 31–34 | Asymmetric four-chamber network and mountain passage |
| 34–38 | Wave Function Collapse: three Temperate and three Desert worlds |
| 38–42 | Wave Function Collapse: three Frozen and three Mixed worlds |
| 42–46 | Training observatory: reinforcement learning, learning progress and policy behavior |
| 46–54 | Full supplied Navigation Lab clip: trained scout and sensors |
| 54–59 | Temperate world and corrected AlienWars project credits |

Each palette has three different seeds and both symmetric and asymmetric
examples. Gallery images are real browser renders, checked against native world
hashes. The gallery lasts eight seconds. Both tunnel isolation reveals preserve
the camera. Training labels sit above the actual metrics, leaving all numbers
and plots unchanged. Keep the captions focused on reinforcement learning and
policy behavior; do not add a playback-speed explanation. The original UI's
replay indicator remains part of the supplied footage.

Use **Wave Function Collapse**, not the acronym. No music or audio stream.
Closing copy:

> **ALIENWARS GYM**<br>
> A procedural world for training AlienWars agents.<br>
> Built on PufferLib 5<br>
> Rendered with Raylib · WebAssembly Runtime<br>
> rozgo.github.io/alienwars-gym

Opening world:
<https://rozgo.github.io/alienwars-gym/maplab/?seed=73&sym=1&a=6&b=6&biome=2&tunnels=1&cut=1&sensors=15&unit=7&sensorsAll=1>

## Review and reproducibility

Review the encoded video, contact-sheet.jpg and motion, not only setup
screenshots. Check both gallery boards, radio links, the depth preview,
all-unit settings, terrain cutaway, camera-preserving isolation and credits.
capture.json records builds, hashes, frame counts and visibility settings.
video-manifest.json records final duration, codecs and SHA-256. The master is
AlienWars-Gym-Demo-v4.mp4: 59 seconds, 1080p30 H.264, fast-start, no audio.

For a retake, run node scripts/demo/record.cjs --shot=05-radio. It writes a
separate manifest; preserve and update the complete manifest when assembling
retakes. Worlds are deterministic; unit positions vary with capture timing.

scout.c screens actual generator outputs without changing them. Its cave graph
metrics help select candidates, but small cycles need visual inspection and do
not necessarily imply large visible loops.

```sh
clang -std=c11 -O2 -I. scripts/demo/scout.c -lm -o build/demo-scout
build/demo-scout 96 > outputs/demo/scouting.jsonl
```
