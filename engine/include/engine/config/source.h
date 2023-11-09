#pragma once

#include "engine/core/filesystem.h"
#include "engine/core/units.h"
#include "engine/core/panic.h"

#include <string>
#include <unordered_map>
#include <format>

namespace simcoe::config {
    struct INode;
    struct IConfigEntry;

    using NodeMap = std::unordered_map<std::string, const INode*>;

    enum ValueType {
        eConfigBool,
        eConfigString,
        eConfigInt,
        eConfigFloat,

        eConfigEnum,
        eConfigFlags,

        eConfigGroup,

        eConfigError,

        eConfigCount
    };

    struct INode {
        virtual ~INode() = default;

        // reading api
        virtual ValueType getType() const = 0;

        template<typename T>
        T getUnchecked() const {
            T value;
            SM_ASSERT(get(value));
            return value;
        }

        template<std::integral T>
        bool get(T& value) const {
            if (int64_t v; get(v)) {
                value = static_cast<T>(v);
                return true;
            }

            return false;
        }

        virtual bool get(bool& value) const = 0;
        virtual bool get(int64_t& value) const = 0;
        virtual bool get(float& value) const = 0;
        virtual bool get(std::string& value) const = 0;
        virtual bool get(NodeMap &value) const = 0;
    };

    struct ISource {
        virtual ~ISource() = default;

        virtual const INode *load(const fs::path& path) = 0;
        virtual bool save(const fs::path& path, const INode *pNode) = 0;

        virtual const INode *create(bool value) = 0;
        virtual const INode *create(int64_t value) = 0;
        virtual const INode *create(float value) = 0;
        virtual const INode *create(std::string_view value) = 0;
        virtual const INode *create(const NodeMap &value) = 0;
    };

    ISource *newTomlSource();
}

template<typename T>
struct std::formatter<simcoe::config::ValueType, T> : std::formatter<std::string_view, T> {
    auto format(simcoe::config::ValueType type, auto& ctx) {
        using f = std::formatter<std::string_view, T>;
        switch (type) {
        case simcoe::config::eConfigBool: return f::format("bool", ctx);
        case simcoe::config::eConfigInt: return f::format("int", ctx);
        case simcoe::config::eConfigFloat: return f::format("float", ctx);
        case simcoe::config::eConfigString: return f::format("string", ctx);
        case simcoe::config::eConfigGroup: return f::format("table", ctx);
        default: return f::format("unknown", ctx);
        }
    }
};
