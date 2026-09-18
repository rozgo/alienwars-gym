# Official Flecs Explorer frontend

Source: https://github.com/flecs-hub/explorer at the commit in `version.json`.
`frontend.tar.gz` contains unmodified `etc/` frontend assets: Vue components,
styles, icons, fonts and runtime dependencies. Unused example WASM worlds,
legacy frontends and Ace language packs are omitted. The archive is hash-checked
before extraction by `scripts/build_explorer.py`. The build applies read-only
UI adaptations and the in-process parent-window bridge; upstream sources remain
unchanged in the archive. The Explorer MIT license and third-party notices are
included here and copied into the public distribution.

Bundled dependencies: Vue (MIT), vue3-sfc-loader (MIT), Chart.js (MIT),
Ace (BSD), Codicons (CC-BY-4.0 icons / MIT code), and Inter (SIL OFL).
See `licenses/` for upstream terms. Codicons icons are by Microsoft and Inter is
by Rasmus Andersson. All embedded JavaScript copyright headers are preserved.
