#pragma once

#include "engine/config/schema.h"

#include "engine/core/panic.h"
#include "engine/core/strings.h"
#include "engine/core/units.h"

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
    template<typename T>
    struct ISchema : ISchemaBase {
        virtual ~ISchema() = default;
        constexpr ISchema(std::string_view name, T *pValue)
            : ISchemaBase(name)
            , pValue(pValue)
        { }

    protected:
        void update(const ConfigContext& ctx, const T& value) const {
            if (pValue != nullptr) {
                *pValue = value;
            } else {
                ctx.error("schema has no value pointer");
            }
        }

    private:
        T *pValue = nullptr;
    };

    struct String final : ISchema<std::string> {
        constexpr String(std::string_view name, std::string *pValue)
            : ISchema(name, pValue)
        { }

        void readNode(ConfigContext& ctx, const toml::node& node) const override;
    };

    struct Bool final : ISchema<bool> {
        constexpr Bool(std::string_view name, bool *pValue)
            : ISchema(name, pValue)
        { }

        void readNode(ConfigContext& ctx, const toml::node& node) const override;
    };

    // the base of either an enum or a flags
    template<typename T>
    struct Choice : ISchema<T> {
        using Super = ISchema<T>;
        using Super::Super;

        using NameMap = std::unordered_map<std::string_view, T>;

        constexpr Choice(std::string_view name, T *pValue, const NameMap& names)
            : Super(name, pValue)
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
    struct Enum final : public Choice<T> {
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
    struct Flags final : public Choice<T> {
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

    /**
     * @brief
     *
     * @tparam T the real type of the digit
     * @tparam O the digit toml requires, a bit of a hack
     */
    template<typename T, typename O>
    struct DigitBase : ISchema<T> {
        using Super = ISchema<T>;
        using Super::Super;

        constexpr DigitBase(std::string_view name, T *pValue)
            : Super(name, pValue)
        { }

        void readNode(ConfigContext& ctx, const toml::node& node) const override {
            if (ctx.verifyConfigField(node, kType)) {
                if (auto [value, error] = core::checkedIntCast<T>(node.value_or<O>(0)); error != core::CastError::eNone) {
                    ctx.error("value `{}` is out of range for type `{}`", value, kType);
                } else {
                    Super::update(ctx, value);
                }
            }
        }

    private:
        constexpr static toml::node_type kType = toml::impl::node_type_of<T>;
    };

    template<std::integral T>
    struct Int : DigitBase<T, int64_t> {
        using Super = DigitBase<T, int64_t>;
        using Super::Super;
    };

    template<std::floating_point T>
    struct Decimal : DigitBase<T, double> {
        using Super = DigitBase<T, double>;
        using Super::Super;
    };

    template<typename T>
    struct Digit { /* this should never be reached*/ };

    template<std::floating_point T>
    struct Digit<T> : DigitBase<T, double> {
        using Super = DigitBase<T, double>;
        using Super::Super;
    };

    template<std::integral T>
    struct Digit<T> : DigitBase<T, int64_t> {
        using Super = DigitBase<T, int64_t>;
        using Super::Super;
    };

    struct Table : ISchemaBase {
        using Fields = std::unordered_map<std::string_view, const ISchemaBase*>;

        Table(std::string_view name, const Fields& items)
            : ISchemaBase(name)
            , schemas(items)
        { }

        void readNode(ConfigContext& ctx, const toml::node& config) const override;
    private:
        Fields schemas;
    };
}
