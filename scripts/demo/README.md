# AlienWars Gym showcase

Capture and edit the 57-second Map Lab video from the actual published
Raylib/WebAssembly application. The recording adds captions and temporarily
hides interface elements; it does not replace simulation, sensor data or rendering.

## Record and edit

The capture uses Chrome, Node.js, Puppeteer Core 24.37.5 and FFmpeg with libx264.
The editor uses Python, NumPy and Pillow. Install optional tooling in ignored
directories, separate from the environment and training dependencies:

```sh
npm install --prefix .local/demo-tools puppeteer-core@24.37.5
python3 -m venv .local/demo-python
.local/demo-python/bin/pip install numpy==2.5.1 pillow==12.2.0
node scripts/demo/record.cjs --probe
node scripts/demo/record.cjs
.local/demo-python/bin/python scripts/demo/edit.py
```

Run from the repository root. `CHROME_PATH` can override the macOS Chrome
executable. The current editor uses the macOS Menlo font for its review sheet.
All generated files go to ignored `outputs/demo/`.

Chrome runs headless with its own profile in `.local/demo-chrome` and a
1920 × 1080 viewport. Headful screencasting can silently clip to the physical
display size even when a full-page screenshot looks correct. The editor checks
the first actual screencast JPEG for every shot to catch that failure. CDP
timestamps are resampled to 30 fps before H.264 encoding.

Review the exported video and `contact-sheet.jpg`, not just the pre-recording
screenshots. Check caption framing, depth preview, sensor toggles, tunnel
cutaway, the fixed-camera isolation reveal, and closing credits. `capture.json`
records the browser, published build manifests, world hashes and frame counts.
`video-manifest.json` records final duration, codecs and SHA-256.

## Edit sequence

| Time | Scene |
| --- | --- |
| 00–04 | Seed 73 overview, all sensors, AlienWars Gym title |
| 04–10 | Sensors off; orbit continuous WFC terrain |
| 10–20 | Focus gunship; LiDAR, RF, then depth camera and range image |
| 20–27 | Ground traversal, then naval patrol with sonar |
| 27–38 | Ramp entrance, underground scout and automatic cutaway |
| 38–45 | Wide, stationary camera; isolate tunnels on and off |
| 45–51 | Bridge in seed 2438762858; asymmetric world in seed 2026 |
| 51–57 | Closing world view and project credits |

Opening URL:
<https://rozgo.github.io/alienwars-gym/maplab/?seed=73&sym=1&a=6&b=6&biome=2&tunnels=1&cut=1&sensors=15&unit=7&sensorsAll=1>

Camera movements use the real pan/orbit controls. Feature switches use the
existing controls and viewer exports. World generation is deterministic, but
unit positions at capture time vary with loading and recording timing.

Closing copy:

> **ALIENWARS GYM**<br>
> A procedural world for training Alien Wars agents.<br>
> Built on PufferLib 5<br>
> Rendered with Raylib · WebAssembly Runtime<br>
> rozgo.github.io/alienwars-gym

The quiet ambient soundtrack is original, deterministic synthesis in `edit.py`:
no third-party recordings or samples. Master output is 57 seconds, 1080p30
H.264/AAC with fast-start playback, plus a poster and manifest. Durable videos
belong in GitHub Release assets; do not commit raw footage or the MP4 to Git.
