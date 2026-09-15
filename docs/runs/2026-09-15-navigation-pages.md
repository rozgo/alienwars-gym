# Navigation and showcase Pages release — September 15, 2026

Merged the navigation worktree into main. Pages now includes the trained
Navigation Lab, a static Training Observatory with five recorded runs and twelve
evaluation summaries, and the final 59-second silent showcase. The checkpoint,
resolved config, recorded metrics, episode evaluations and video masters are
Release assets. Map Lab remains the entry point.

[Training protocol and results](2026-09-15-alienwars-navigation.md) ·
[Release validation and hashes](2026-09-15-navigation-pages.json)

Native navigation ASan/UBSan and adapter checks passed, including reset and
terminal semantics, shared support, submerged beds, and zero step allocations.
Native/WASM task outcomes matched; libm steering drift produced 2051 versus 2055
decisions. A fresh checkpoint smoke evaluation completed 8/8 surface, 8/8 bridge
and 8/8 tunnel episodes. Map Lab debug generation and Minimal's 1024-step smoke
rollout also passed.

Chrome checks exercised the trained checkpoint through an arrival, sensor
controls, recorded-run selection (including the aborted run), and 1080p video
playback and seeking. All four pages fit a 390px viewport. The mobile header and
metrics table layouts were adjusted without changing simulation behavior.

Build manifests identify the authored source commit and file hashes. Map Lab's
JavaScript and WASM are unchanged from the previous release; its HTML gains
project links and a wrapping mobile header. Navigation Lab packages the exact
checkpoint reported in the training results, with no untrained fallback.
