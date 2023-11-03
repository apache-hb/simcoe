#pragma once

#include "engine/config/config.h"

template<typename T, typename U> T getFieldType(T U::*);
#define FIELD_TYPE(T, F) decltype(getFieldType(&T::F))

#define CFG_DECLARE(SUPER, NAME, ...) \
    []() -> config::ISchemaBase* { \
        using ConfigType = SUPER; \
        return new simcoe::config::Table(NAME, __VA_ARGS__); \
    }()

#define CFG_FIELD_ENUM(NAME, FIELD, ...) \
    { NAME, new simcoe::config::Enum<FIELD_TYPE(ConfigType, FIELD)>({ NAME, offsetof(ConfigType, FIELD) }, __VA_ARGS__) }

#define CFG_FIELD_FLAGS(NAME, FIELD, ...) \
    { NAME, new simcoe::config::Flags<FIELD_TYPE(ConfigType, FIELD)>({ NAME, offsetof(ConfigType, FIELD) }, __VA_ARGS__) }

#define CFG_FIELD_INT(NAME, FIELD) \
    { NAME, new simcoe::config::Int<FIELD_TYPE(ConfigType, FIELD)>({ NAME, offsetof(ConfigType, FIELD) }) }

#define CFG_CASE(NAME, CASE) \
    { NAME, CASE }
