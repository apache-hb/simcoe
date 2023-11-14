#pragma once

// TODO: flecs needs to be configured inside the project itself rather than the header
//       this will require making changes to the flecs mesonbuild file
// #if SM_DEBUG
// #   define FLECS_DEBUG
// #   define FLECS_SANITIZE
// #   define FLECS_ACCURATE_COUNTERS
// #endif

#pragma warning(push, 0)
#pragma warning(disable: 4127)

#include <flecs.h>

#pragma warning(pop)
