#pragma once

#include "engine/core/filesystem.h"

#include "engine/config/source.h"

namespace simcoe::config {
    // context to handle object pointers
    struct ConfigContext final {
        ConfigContext(std::string_view file);

        void enter(std::string_view name);
        void leave();

        template<typename... A>
        void error(std::string_view msg, A&&... args) const {
            error(std::vformat(msg, std::make_format_args(args...)));
        }

        void error(std::string_view msg) const;

        bool verifyConfigField(const INode *pNode, NodeType expected) const;

    private:
        std::string_view file;
        std::vector<std::string_view> path;
    };

    struct FieldInfo {
        std::string_view name;
    };

    struct ISchemaBase {
        constexpr ISchemaBase(std::string_view name)
            : name(name)
        { }

        constexpr std::string_view getName() const { return name; }

        virtual ~ISchemaBase() = default;

    protected:
        virtual void readNode(ConfigContext& ctx, const INode *pNode) const = 0;

        auto region(ConfigContext& ctx) const;

    public:
        void load(ConfigContext& ctx, const INode *pNode) const;

    private:
        std::string_view name;
    };
}
