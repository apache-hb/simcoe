#pragma once

#include "engine/config/config.h"

template<typename T, typename U> T getFieldType(T U::*);
#define FIELD_TYPE(T, F) decltype(getFieldType(&T::F))

#define CFG_DECLARE(SUPER, NAME, ...) \
    []() -> config::ISchemaBase* { \
        using ConfigType = SUPER; \
        return new simcoe::config::Table(NAME, __VA_ARGS__); \
    }()

#define CFG_FIELD_ENUM(NAME, FIELD, NAMES) \
    { NAME, new simcoe::config::Enum<FIELD_TYPE(ConfigType, FIELD)>({ NAME, offsetof(ConfigType, FIELD) }, NAMES) }
