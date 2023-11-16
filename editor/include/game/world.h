#pragma once

#include "engine/core/macros.h"

#include <type_traits>

namespace game {
    enum struct EntitySalt : size_t { };
    SM_ENUM_INT(EntitySalt, EntitySaltType)
}
