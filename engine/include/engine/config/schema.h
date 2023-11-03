#pragma once

#include "engine/core/filesystem.h"
#include "engine/core/panic.h"
#include "engine/core/strings.h"

#include <toml++/toml.h>

#include <unordered_map>
#include <format>

template<typename T>
struct std::formatter<toml::node_type, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(toml::node_type type, FormatContext& ctx) {
        using f = std::formatter<std::string_view, T>;
        switch (type) {
        case toml::node_type::none:
            return f::format("none", ctx);
        case toml::node_type::table:
            return f::format("table", ctx);
        case toml::node_type::array:
            return f::format("array", ctx);
        case toml::node_type::string:
            return f::format("string", ctx);
        case toml::node_type::integer:
            return f::format("integer", ctx);
        case toml::node_type::floating_point:
            return f::format("floating_point", ctx);
        case toml::node_type::boolean:
            return f::format("boolean", ctx);
        case toml::node_type::date:
            return f::format("date", ctx);
        case toml::node_type::time:
            return f::format("time", ctx);
        case toml::node_type::date_time:
            return f::format("date_time", ctx);
        default:
            return f::format("unknown", ctx);
        }
    }
};

namespace simcoe::config {
    // context to handle object pointers
    struct ConfigContext final {
        ConfigContext(std::string_view file, void *pObjectPtr);

        void enter(std::string_view name);
        void leave();

        template<typename... A>
        void error(std::string_view msg, A&&... args) const {
            error(std::vformat(msg, std::make_format_args(args...)));
        }

        void error(std::string_view msg) const;

        template<typename T>
        bool get(const toml::node& node, T& dst) const {
            constexpr toml::node_type expected = toml::impl::value_traits<T>::type;
            if (node.type() == expected) {
                dst = node.as<T>()->get();
                return true;
            }

            // TODO: unstable api but who cares :^)
            error("expected a {}, found a {}", expected, node.type());
            return false;
        }

        // table is a special case (why?)
        template<>
        bool get<toml::table>(const toml::node& node, toml::table& dst) const {
            if (node.type() == toml::node_type::table) {
                dst = *node.as<toml::table>();
                return true;
            }

            error("expected a table, found a {}", node.type());
            return false;
        }

        template<typename T>
        void writeField(uintptr_t offset, const T& value) const {
            *reinterpret_cast<T*>(offset + uintptr_t(pObjectPtr)) = value;
        }

    private:
        void *pObjectPtr;

        std::string_view file;
        std::vector<std::string_view> path;
    };

    struct FieldInfo {
        std::string_view name;
        uintptr_t offset;
    };

    struct ISchemaBase {
        constexpr ISchemaBase(std::string_view name)
            : name(name)
        { }

        constexpr std::string_view getName() const { return name; }

        virtual ~ISchemaBase() = default;

    protected:
        virtual void readNode(ConfigContext& ctx, const toml::node& node) const = 0;

        auto region(ConfigContext& ctx) const {
            struct ConfigRegion {
                ConfigRegion(ConfigContext& ctx, std::string_view name) : ctx(ctx) {
                    ctx.enter(name);
                }
                ~ConfigRegion() {
                    ctx.leave();
                }

            private:
                ConfigContext& ctx;
            };

            return ConfigRegion(ctx, getName());
        }

    public:
        void load(ConfigContext& ctx, const toml::node& node) const {
            auto r = region(ctx);
            readNode(ctx, node);
        }

    private:
        std::string_view name;
    };

    struct IFieldOffset {
        constexpr IFieldOffset(uintptr_t offset)
            : offset(offset)
        { }

    protected:
        constexpr uintptr_t getOffset() const { return offset; }

    private:
        uintptr_t offset;
    };

    struct ISchema : ISchemaBase, IFieldOffset {
        virtual ~ISchema() = default;
        constexpr ISchema(const FieldInfo& info)
            : ISchemaBase(info.name)
            , IFieldOffset(info.offset)
        { }

    protected:
        template<typename T>
        void update(const ConfigContext& ctx, const T& value) const {
            const auto dataOffset = getOffset();
            ASSERTF(dataOffset != UINTPTR_MAX, "attempted to write to a non-existent field `{}`", getName());
            ctx.writeField(dataOffset, value);
        }
    };

    struct String final : ISchema {
        constexpr String(const FieldInfo& info)
            : ISchema(info)
        { }

        void readNode(ConfigContext& ctx, const toml::node& node) const override;
    };

    template<typename T>
    struct Enum final : ISchema {
        using NameMap = std::unordered_map<std::string_view, T>;

        constexpr Enum(const FieldInfo& info, const NameMap& names)
            : ISchema(info)
            , values(names)
        { }

        void readNode(ConfigContext& ctx, const toml::node& node) const override {
            auto r = region(ctx);
            if (std::string str; ctx.get(node, str)) {
                if (auto it = values.find(str); it == values.end()) {
                    unknownChoice(ctx, str);
                } else {
                    update(ctx, it->second);
                }
            }
        }

    private:
        void unknownChoice(ConfigContext& ctx, std::string_view choice) const {
            ctx.error("invalid config value, got {}\nexpected one of `{}`", choice, validOptions());
        }

        std::string validOptions() const {
            std::vector<std::string_view> options;
            for (auto& [choice, _] : values) {
                options.emplace_back(choice);
            }
            return util::join(options, ", ");
        }

        NameMap values;
    };

    struct Table final : ISchemaBase {
        using Fields = std::unordered_map<std::string_view, ISchemaBase*>;

        Table(std::string_view name, const Fields& items)
            : ISchemaBase(name)
            , schemas(items)
        { }

        void readNode(ConfigContext& ctx, const toml::node& config) const override;
    private:
        Fields schemas;
    };
}

