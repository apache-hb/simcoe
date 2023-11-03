#pragma once

#include "engine/config/schema.h"

#include "engine/core/panic.h"
#include "engine/core/strings.h"
#include "engine/core/units.h"

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

        void readNode(ConfigContext& ctx, const INode *pNode) const override;
    };

    struct Bool final : ISchema<bool> {
        constexpr Bool(std::string_view name, bool *pValue)
            : ISchema(name, pValue)
        { }

        void readNode(ConfigContext& ctx, const INode *pNode) const override;
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

        void readNode(ConfigContext& ctx, const INode *pNode) const override {
            if (!ctx.verifyConfigField(pNode, eString)) {
                return;
            }

            std::string id = pNode->getUnchecked<std::string>();
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

        void readNode(ConfigContext& ctx, const INode *pNode) const override {
            if (!ctx.verifyConfigField(pNode, eArray)) {
                return;
            }

            auto vec = pNode->getUnchecked<NodeVec>();
            if (!isArrayAll(vec, eString)) {
                ctx.error("expected array of strings");
                return;
            }

            T value = T();
            for (auto *pItem : vec) {
                std::string id = pItem->getUnchecked<std::string>();
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

        void readNode(ConfigContext& ctx, const INode *pNode) const override {
            if (!ctx.verifyConfigField(pNode, kType)) {
                return;
            }

            auto [value, error] = core::checkedIntCast<T>(pNode->getUnchecked<O>());
            if (error != core::CastError::eNone) {
                ctx.error("value `{}` is out of range for type `{}`", value, kType);
            } else {
                Super::update(ctx, value);
            }
        }

    private:
        constexpr static NodeType kType = std::is_same_v<O, int64_t> ? eInt : eFloat;
    };

    template<std::integral T>
    struct Int : DigitBase<T, int64_t> {
        using Super = DigitBase<T, int64_t>;
        using Super::Super;
    };

    struct Table : ISchemaBase {
        using Fields = std::unordered_map<std::string_view, const ISchemaBase*>;

        Table(std::string_view name, const Fields& items)
            : ISchemaBase(name)
            , schemas(items)
        { }

        void readNode(ConfigContext& ctx, const INode *pNode) const override;
    private:
        Fields schemas;
    };
}
