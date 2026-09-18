/* Compile Flecs as C, including when the caller is the CUDA/C++ trainer. */
/* Strict C11 on Linux hides clock_gettime/nanosleep without this feature level.
 * Define it before any system header, without changing the vendored source. */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "flecs_config.h"
#include "../../vendor/flecs/flecs.c"
