---
name: alienwars-start
description: Help a newcomer run AlienWars Gym locally, understand its simulation and training architecture, and complete the first sensor experiment with their coding agent. Use for repository onboarding or first-run setup.
---

# AlienWars first experiment

Work in this checkout. Read the root [AGENTS.md](../../../AGENTS.md) and
[start guide](../../../docs/START_HERE.md); subsystem docs are linked there.
Use the user's chosen task if they already have one. For a first visit, aim for a
running local Map Lab and the overlay-versus-equipment sensor experiment.

1. Check `git status`. Run `uv run scripts/doctor.py` from the root. If uv is
   missing, use the installation link in the start guide. Report actual missing
   prerequisites; the preview does not require Emscripten, CUDA or a native build.
2. Serve the committed `docs/` with the guide's localhost command. Keep the server
   process available for the user; if the port is occupied, use another port and
   report its actual URL. Open `/learn/` when browser control is available.
   Verify the page loads; a started process alone is not a verified demo.
3. Follow [Sensor experiment](../../../docs/lessons/sensor-equipment.md). Explain
   the prediction before the interaction, then compare measured readings. Overlay
   visibility does not disable a sensor. Equipment edits are not stored in seed
   links. Do not promise a specific route change or improved arrivals.
4. If the user wants to edit source, run `doctor.py --target web` or `--target
   native` and use the corresponding build guide. Serving packaged artifacts
   cannot show C or web-shell edits until rebuilt. For trained browser inference,
   use `uv run scripts/build_fleet_site.py`, not a controller-free build.
5. Finish with the working URL, what was observed, relevant files/commands and
   any unverified steps. Explain C simulation → sensors → policy → bounded motion;
   Raylib is display, PufferLib trains on a CUDA host, and browser inference is local.

Use small changes and the root validation table. Onboarding itself does not
require a GPU run, a policy replacement or publication. If browser control is
unavailable, give the exact manual steps and identify what remains unverified.
