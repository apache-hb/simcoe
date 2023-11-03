#pragma once

#include "engine/core/filesystem.h"
#include "engine/core/units.h"
#include "engine/core/panic.h"

#include <string>
#include <unordered_map>
#include <format>

namespace simcoe::config {
    struct INode;

    using NodeMap = std::unordered_map<std::string, INode*>;
    using NodeVec = std::vector<INode*>;

    enum NodeType {
        eBool,
        eInt,
        eFloat,
        eString,
        eTable,
        eArray,

        eUnkonwn
    };

    struct INode {
        virtual ~INode() = default;

        virtual NodeType getType() const = 0;

        template<typename T>
        T getUnchecked() const {
            T value;
            ASSERTF(get(value), "failed to get value");
            return value;
        }

        virtual bool get(bool& value) const = 0;
        virtual bool get(int64_t& value) const = 0;
        virtual bool get(std::string& value) const = 0;
        virtual bool get(NodeMap &value) const = 0;
        virtual bool get(NodeVec &value) const = 0;
    };

    bool isArrayAll(const NodeVec& nodes, NodeType type);

    struct ISource {
        virtual ~ISource() = default;

        virtual INode *load() = 0;
    };

    ISource *loadToml(const fs::path& path);
}

template<typename T>
struct std::formatter<simcoe::config::NodeType, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(simcoe::config::NodeType type, FormatContext& ctx) {
        using f = std::formatter<std::string_view, T>;
        switch (type) {
        case simcoe::config::eBool: return f::format("bool", ctx);
        case simcoe::config::eInt: return f::format("int", ctx);
        case simcoe::config::eFloat: return f::format("float", ctx);
        case simcoe::config::eString: return f::format("string", ctx);
        case simcoe::config::eTable: return f::format("table", ctx);
        case simcoe::config::eArray: return f::format("array", ctx);
        default: return f::format("unknown", ctx);
        }
    }
};