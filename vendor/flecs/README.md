# Flecs 4.1.6

Vendored from https://github.com/SanderMertens/flecs at release `v4.1.6`, commit
`fb55f3c25660425cfe1bc4cf5e6bff8b3f18a9b8`. `version.json` records SHA-256 hashes
of the unmodified upstream distribution and MIT license.

AlienWars compiles `ocean/alienwars/flecs_runtime.c` as C. Its configuration enables
the core ECS, platform OS implementation and low-footprint storage configuration;
rendering, scheduling and RL remain
owned by the existing fixed-step application. Edit our integration, not these
upstream files. Updates require native/WASM behavior, allocation and trainer checks.
