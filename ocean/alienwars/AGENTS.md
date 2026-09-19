# Simulation and renderer changes

The root AGENTS.md applies. Read the relevant sections of
[ENGINEERING_INVARIANTS.md](../../docs/ENGINEERING_INVARIANTS.md) before editing.
It preserves geometry, seed, body, sensor, lifecycle and checkpoint constraints.

Map/physics/sensing are renderer-independent C shared by native training and
WebAssembly. Visual-only changes belong in the viewer/render path. A rendering
change still needs actual browser inspection; a navigation change needs measured
behavior, not just a screenshot. Choose checks using the root validation table.
