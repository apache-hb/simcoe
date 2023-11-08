#pragma once

#include "engine/config/source.h"
#include "engine/core/panic.h"
#include "engine/core/units.h"

#include <string>
#include <unordered_map>

namespace simcoe::config {
    struct ConfigEntry;

    using ConfigMap = std::unordered_map<std::string, ConfigEntry*>;
    using ConfigFlagMap = std::unordered_map<std::string_view, int64_t>;
    using ConfigNameMap = std::unordered_map<int64_t, std::string_view>;

    enum ValueFlag {
        eConfigDefault = 0, ///< default

        eConfigReadOnly = 1 << 0, ///< this entry is read only
    };

    struct ConfigEntryInfo {
        std::string name; // name
        std::string_view description; // description
        ValueType type;
        ValueFlag flags;
    };

    struct ConfigEntry {
        // constructor for groups
        ConfigEntry(const ConfigEntryInfo& info);

        // constructor for a child of the config
        ConfigEntry(std::string_view path, const ConfigEntryInfo& info);

        std::string_view getName() const { return info.name; }
        std::string_view getDescription() const { return info.description; }

        bool hasFlag(ValueFlag flag) const { return (info.flags & flag) != 0; }
        ValueType getType() const { return info.type; }

        virtual bool isModified() const = 0;

        virtual const void *getCurrentValue() const { SM_NEVER("getCurrentValue"); }
        virtual const void *getDefaultValue() const { SM_NEVER("getDefaultValue"); }

        /**
         * @brief Set the Current Value object
         * @note should only be called from c++ code to set stuff
         * @param pData the same type as stored
         */
        virtual void setCurrentValue(SM_UNUSED const void *pData) = 0;

        /**
         * @brief parse a value from some other data
         * @note used for loading from config files
         * @param pData the node to parse from
         * @return true if the value was parsed
         */
        virtual bool parseValue(SM_UNUSED const INode *pNode) = 0;

        /**
         * @brief save the value to some other data
         *
         * @param pData
         */
        virtual void saveValue(SM_UNUSED void *pData, size_t size) const = 0;

        virtual const ConfigFlagMap& getEnumFlags() const { SM_NEVER("getFlags"); }
        virtual const ConfigNameMap& getEnumNames() const { SM_NEVER("getNames"); }
        virtual const ConfigMap& getChildren() const { SM_NEVER("getChildren"); }

    private:
        ConfigEntryInfo info;
    };

    namespace detail {
        template<typename T>
        struct ConfigValueTraits;

        template<>
        struct ConfigValueTraits<bool> {
            static constexpr ValueType kType = eConfigBool;
            using Type = bool;
        };

        template<>
        struct ConfigValueTraits<std::string> {
            static constexpr ValueType kType = eConfigString;
            using Type = std::string;
        };

        template<std::integral T>
        struct ConfigValueTraits<T> {
            static constexpr ValueType kType = eConfigInt;
            using Type = int64_t;
        };

        template<typename T> requires std::is_enum_v<T>
        struct ConfigValueTraits<T> {
            static constexpr ValueType kType = eConfigEnum;
            using Type = int64_t;
        };

        template<>
        struct ConfigValueTraits<float> {
            static constexpr ValueType kType = eConfigFloat;
            using Type = float;
        };
    }

    template<typename T>
    struct ConfigValue : ConfigEntry {
        using Traits = detail::ConfigValueTraits<T>;
        using SaveType = typename Traits::Type;
        using InnerType = T;

        ConfigValue(std::string_view path, std::string name, std::string_view description, InnerType defaultValue, ValueFlag flags = eConfigDefault)
            : ConfigValue(path, defaultValue, {
                .name = name,
                .description = description,
                .type = Traits::kType,
                .flags = flags
            })
        { }

        InnerType getValue() const { return currentValue; }
        void setValue(InnerType update) { currentValue = update; }

        template<typename O>
        O getValueAs() const { return core::intCast<O>(currentValue); }

        const void *getCurrentValue() const override { return &currentValue; }
        const void *getDefaultValue() const override { return &defaultValue; }

        bool isModified() const override { return currentValue != defaultValue; }

        void setCurrentValue(const void *pData) override {
            SaveType temp = *static_cast<const SaveType*>(pData);
            currentValue = T(temp);
            // TODO: notify listeners
        }

        bool parseValue(const INode *pNode) override {
            if (SaveType value; pNode->get(value)) {
                currentValue = T(value);
                return true;
            }

            return false;
            // TODO: notify listeners
        }

        void saveValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config value (expected {}, got {})", sizeof(SaveType), size);
            *static_cast<SaveType*>(pData) = currentValue;
        }

    protected:
        ConfigValue(std::string_view path, InnerType defaultValue, const ConfigEntryInfo& info)
            : ConfigEntry(path, info)
            , defaultValue(defaultValue)
            , currentValue(defaultValue)
        { }

    private:
        const InnerType defaultValue;
        InnerType currentValue;
    };

    template<typename T>
    struct ConfigEnumValue final : public ConfigValue<T> {
        using Super = ConfigValue<T>;
        using Super::Super;

        ConfigEnumValue(std::string_view path, std::string name, std::string_view description, T defaultValue, ConfigFlagMap opts, ValueFlag flags = eConfigDefault)
            : Super(path, defaultValue, {
                .name = name,
                .description = description,
                .type = eConfigEnum,
                .flags = flags
            })
            , opts(opts)
        {
            for (auto& [key, value] : opts) {
                saveOpts[value] = key;
            }
        }

        // pData is a std::string here
        bool parseValue(const INode *pData) override {
            if (std::string opt; pData->get(opt)) {
                if (auto it = opts.find(opt); it != opts.end()) {
                    Super::setCurrentValue(&it->second);
                    return true;
                }
            }

            return false;
        }

        void saveValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(std::string), "invalid size for config value (expected {}, got {})", sizeof(std::string), size);

            if (auto it = saveOpts.find(Super::getValue()); it != saveOpts.end()) {
                *static_cast<std::string*>(pData) = it->second;
            }
        }

        const ConfigFlagMap& getEnumFlags() const override { return opts; }
        const ConfigNameMap& getEnumNames() const override { return saveOpts; }
    private:
        ConfigFlagMap opts;
        ConfigNameMap saveOpts;
    };

    ConfigEntry *getConfig();
}
