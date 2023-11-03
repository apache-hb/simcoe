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

        bool verifyConfigField(const toml::node& node, toml::node_type expected);

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

    // the base of either an enum or a flags
    template<typename T>
    struct Choice : ISchema {
        using Super = ISchema;
        using Super::Super;

        using NameMap = std::unordered_map<std::string_view, T>;

        constexpr Choice(const FieldInfo& info, const NameMap& names)
            : ISchema(info)
            , values(names)
        { }

    protected:
        std::string validOptions() const {
            std::vector<std::string_view> options;
            for (auto& [choice, _] : values) {
                options.emplace_back(choice);
            }
            return util::join(options, ", ");
        }

        bool findName(std::string_view field, T& dst) const {
            if (auto it = values.find(field); it == values.end()) {
                return false;
            } else {
                dst = it->second;
                return true;
            }
        }

    private:
        const NameMap values;
    };

    template<typename T>
    struct Enum final : Choice<T> {
        using Super = Choice<T>;
        using Super::Super;

        void readNode(ConfigContext& ctx, const toml::node& node) const override {
            if (!ctx.verifyConfigField(node, toml::node_type::string)) {
                return;
            }

            std::string_view id = node.value_or<std::string_view>("");
            if (T value; Super::findName(id, value)) {
                Super::update(ctx, value);
            } else {
                unknownChoice(ctx, id);
            }
        }

    private:
        void unknownChoice(ConfigContext& ctx, std::string_view choice) const {
            ctx.error("invalid enum choice `{}`\nmust be one of `{}`", choice, Super::validOptions());
        }
    };

    template<typename T>
    struct Flags final : Choice<T> {
        using Super = Choice<T>;
        using Super::Super;

        void readNode(ConfigContext& ctx, const toml::node& node) const override {
            if (!ctx.verifyConfigField(node, toml::node_type::array)) {
                return;
            }

            if (!node.is_homogeneous(toml::node_type::string)) {
                ctx.error("expected array of strings");
                return;
            }

            T value = T();
            for (auto& item : *node.as_array()) {
                if (!ctx.verifyConfigField(item, toml::node_type::string)) {
                    continue;
                }

                std::string_view id = item.value_or<std::string_view>("");
                if (T flag; Super::findName(id, flag)) {
                    value |= flag;
                } else {
                    unknownChoice(ctx, id);
                }
            }

            Super::update(ctx, value);
        }

    private:
        void unknownChoice(ConfigContext& ctx, std::string_view choice) const {
            ctx.error("invalid flag choice `{}`\nmust be one of `{}`", choice, Super::validOptions());
        }
    };

    template<typename T>
    struct Int final : ISchema {
        constexpr Int(const FieldInfo& info)
            : ISchema(info)
        { }

        void readNode(ConfigContext& ctx, const toml::node& node) const override {
            if (!ctx.verifyConfigField(node, toml::node_type::integer)) {
                return;
            }

            T value = node.as_integer()->get();
            update(ctx, value);
        }
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
