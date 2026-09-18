# Continuous demo patrols — September 18, 2026

Automatic Map Lab patrols now start a fresh episode after a timeout, aircraft
impact, unavailable return route, or 30 seconds confined to a small area. A
short restart notice precedes a per-unit respawn at a clear patrol anchor. Paused
units do not restart. User-commanded missions retain their normal semantics.

This viewer lifecycle uses the existing contract-2 policy weights. It does not
change training, promote the rejected contract-3 candidate, or count restarts as
arrivals. The earlier no-respawn reliability measurements remain unchanged.

Source: `545b2feb7c117fcc6c8e9d6384757c49c2a2ea4f`.

- Native ASan/UBSan and WebAssembly agree on timeout/impact restarts for all twelve
  roles, occupied-anchor retry, pause at the restart boundary, manual commands,
  recurrent terminals, equipment preservation and separate lifetime counters.
- Existing mission, adapter, recovery, frozen-actor and policy-inference checks
  pass. Packaged browser assets, four native/WASM map seeds and JavaScript pass.
- A native run with the deployed weights advances 650 simulated seconds on seed
  73. It records **14 per-unit restarts**, no world resets, finite state and no
  overlapping hulls. All twelve units are active at the end. The fixed wings
  complete 29 and 14 arrivals; forced-impact restart coverage is in the focused
  test, not inferred from those natural patrol counts.

Chrome verifies the packaged contract-2 policies, unchanged terrain and active
rendering. By decision 1,460, the skiff has restarted once and is travelling
again; the three air units have completed 1, 7 and 4 missions. No console runtime
errors were observed. Pausing for 491 simulation ticks preserved every unit
position and restart counter; Live mode was then restored.

[Recorded native counters and checks](patrol-loop-2026-09-18.json).

```sh
uv run scripts/check_shared.py
clang -std=c11 -O3 -DAW_NAV_VERSION=2 -I. \
  tests/alienwars/fleet_endurance_test.c ocean/alienwars/flecs_runtime.c \
  -lm -o build/patrol-loop-endurance
AW_MISSION_MODELS=outputs/shared/selected \
  ./build/patrol-loop-endurance 73 6500 loop
```

Omit `loop` to retain the no-respawn endurance mode used for navigation evaluation.
The respawn clears recurrent and stale sensor history without creating a false
odometry jump, preserves equipped modules and counters, and rejects occupied
anchors. Only the affected unit is reset; the map and other units continue.
