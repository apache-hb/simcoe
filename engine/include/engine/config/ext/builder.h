#pragma once

#include "engine/config/ext/schema.h"

namespace simcoe::config {
    template<typename T>
    ISchemaBase *newInt(std::string_view name, T *pValue) {
        return new Int<T>(name, pValue);
    }

    template<typename T>
    using EnumFields = typename Enum<T>::NameMap;

    template<typename T>
    ISchemaBase *newEnum(std::string_view name, T *pValue, const EnumFields<T>& fields) {
        return new Enum<T>(name, pValue, fields);
    }
}

#define CFG_CASE(ID, CASE) \
    { ID, CASE }

#define CFG_FIELD(ID, FIELD) \
    { ID, FIELD }

#define CFG_ENUM(ID, FIELD, ...) \
    simcoe::config::newEnum(ID, FIELD, { __VA_ARGS__ })

#define CFG_FIELD_ENUM(ID, FIELD, ...) \
    { ID, CFG_ENUM(ID, FIELD, __VA_ARGS__) }

#define CFG_BOOL(ID, FIELD) \
    new simcoe::config::Bool(ID, FIELD)

#define CFG_FIELD_BOOL(ID, FIELD) \
    { ID, CFG_BOOL(ID, FIELD) }

#define CFG_INT(ID, FIELD) \
    simcoe::config::newInt(ID, FIELD)

#define CFG_FIELD_INT(ID, FIELD) \
    { ID, CFG_INT(ID, FIELD) }

#define CFG_STRING(ID, FIELD) \
    new simcoe::config::String(ID, FIELD)

#define CFG_FIELD_STRING(ID, FIELD) \
    { ID, CFG_STRING(ID, FIELD) }

#define CFG_TABLE(ID, ...) \
    new simcoe::config::Table(ID, { __VA_ARGS__ })

#define CFG_FIELD_TABLE(ID, ...) \
    { ID, CFG_TABLE(ID, __VA_ARGS__) }

#define CFG_DECLARE(ID, ...) \
    setSchema(CFG_TABLE(ID, __VA_ARGS__))
