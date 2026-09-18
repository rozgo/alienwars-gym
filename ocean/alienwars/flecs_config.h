#ifndef ALIENWARS_FLECS_CONFIG_H
#define ALIENWARS_FLECS_CONFIG_H
/* Explicit, identical native/WASM feature selection. Simulation phases are
 * driven by the fixed-step PufferLib/Raylib caller; no ECS worker pool. */
#define FLECS_CUSTOM_BUILD
#define FLECS_OS_API_IMPL
/* Training runs many small independent worlds. Use compact entity pages and
 * lookup tables instead of reserving capacity for a large editor scene. */
#define FLECS_LOW_FOOTPRINT
/* Viewer inspection only. Never enable these in a PufferLib rollout build. */
#ifdef AW_FLECS_EXPLORER
#define FLECS_META
#define FLECS_DOC
#define FLECS_REST
#define FLECS_QUERY_DSL
#endif
#include "../../vendor/flecs/flecs.h"
#endif
