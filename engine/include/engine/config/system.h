#pragma once

#include "engine/config/source.h"
#include "engine/core/panic.h"
#include "engine/core/units.h"

#include <string>
#include <unordered_map>

namespace simcoe::config {
    struct IConfigEntry;

    using ConfigMap = std::unordered_map<std::string, IConfigEntry*>;
    using ConfigFlagMap = std::unordered_map<std::string_view, int64_t>;
    using ConfigNameMap = std::unordered_map<int64_t, std::string_view>;

    enum ValueFlag {
        eConfigDefault = 0, ///< default

        eConfigReadOnly = 1 << 0, ///< this entry is read only
        eConfigDynamic = 1 << 1, ///< this entry can be modified at runtime
    };

    struct ConfigEntryInfo {
        std::string name; // name
        std::string_view description; // description
        ValueType type;
        ValueFlag flags;
    };

    namespace detail {
        template<typename T>
        struct ConfigValueTraits;

        template<>
        struct ConfigValueTraits<bool> {
            static constexpr ValueType kType = eConfigBool;
            using StorageType = bool;
            using SaveType = bool;
        };

        template<>
        struct ConfigValueTraits<std::string> {
            static constexpr ValueType kType = eConfigString;
            using StorageType = std::string;
            using SaveType = std::string;
        };

        template<std::integral T>
        struct ConfigValueTraits<T> {
            static constexpr ValueType kType = eConfigInt;
            using StorageType = T;
            using SaveType = int64_t;
        };

        template<typename T> requires std::is_enum_v<T>
        struct ConfigValueTraits<T> {
            static constexpr ValueType kType = eConfigEnum;
            using StorageType = T;
            using SaveType = std::string;
        };

        template<>
        struct ConfigValueTraits<float> {
            static constexpr ValueType kType = eConfigFloat;
            using StorageType = float;
            using SaveType = float;
        };
    }

    struct IConfigEntry {
        // constructor for groups
        IConfigEntry(const ConfigEntryInfo& info);

        // constructor for a child of the config
        IConfigEntry(std::string_view path, const ConfigEntryInfo& info);

        std::string_view getName() const { return info.name; }
        std::string_view getDescription() const { return info.description; }

        bool hasFlag(ValueFlag flag) const { return (info.flags & flag) != 0; }
        ValueType getType() const { return info.type; }

        virtual bool isModified() const = 0;

        // these all operate on the inner representation of the value
        template<typename T>
        T getCurrentValue2() const {
            using StorageType = typename detail::ConfigValueTraits<T>::StorageType;
            constexpr auto kExpectedType = detail::ConfigValueTraits<T>::kType;
            SM_ASSERTF(getType() == kExpectedType, "invalid type for config value (expected {}, got {})", kExpectedType, getType());

            StorageType value;
            saveCurrentValue(&value, sizeof(StorageType));
            return T(value);
        }

        template<typename T>
        T getDefaultValue2() const {
            using StorageType = typename detail::ConfigValueTraits<T>::StorageType;
            constexpr auto kExpectedType = detail::ConfigValueTraits<T>::kType;
            SM_ASSERTF(getType() == kExpectedType, "invalid type for config value (expected {}, got {})", kExpectedType, getType());

            StorageType value;
            saveDefaultValue(&value, sizeof(StorageType));
            return T(value);
        }

        ///
        /// config parsing and unparsing
        ///

        /**
         * @brief parse a value from some other data
         *
         * @param pData the node to parse from
         * @return true if the value was loaded properly
         */
        virtual bool parseConfigValue(SM_UNUSED const INode *pNode) { SM_NEVER("parseConfigValue"); }

        /**
         * @brief parse a value from our save type
         *
         * @param pData
         * @param size
         */
        virtual void parseValue(SM_UNUSED const void *pData, SM_UNUSED size_t size) { SM_NEVER("parseValue"); }

        /**
         * @brief unparse a value into a node
         *
         * @param pSource the node source
         * @return const INode* the unparsed node
         */
        virtual void unparseCurrentValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("uparseCurrentValue"); }

        /**
         * @brief unparse the default value into a format suitable for saving and displaying to the user
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void unparseDefaultValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("uparseDefaultValue"); }



        ///
        /// deals with raw internal data
        ///

        /**
         * @brief save the current value to a buffer
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void saveCurrentValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("saveCurrentValue"); }

        /**
         * @brief save the default value to a buffer
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void saveDefaultValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("saveDefaultValue"); }

        /**
         * @brief load the current value from a buffer
         *
         * @param pData data to load from
         * @param size the size of the data
         */
        virtual void loadCurrentValue(SM_UNUSED const void *pData, SM_UNUSED size_t size) { SM_NEVER("loadCurrentValue"); }



        ///
        /// extra config data
        ///

        virtual const ConfigFlagMap& getEnumFlags() const { SM_NEVER("getEnumFlags"); }
        virtual const ConfigNameMap& getEnumNames() const { SM_NEVER("getEnumNames"); }
        virtual const ConfigMap& getChildren() const { SM_NEVER("getChildren"); }

    private:
        ConfigEntryInfo info;
    };

    template<typename T>
    struct ConfigValueBase : public IConfigEntry {
        using Super = IConfigEntry;
        using Super::Super;

        using Traits = detail::ConfigValueTraits<T>;
        using SaveType = typename Traits::SaveType;
        using StorageType = typename Traits::StorageType;

        ConfigValueBase(std::string_view path, std::string name, std::string_view description, StorageType defaultValue, ValueFlag flags = eConfigDefault)
            : ConfigValueBase(path, defaultValue, {
                .name = name,
                .description = description,
                .type = Traits::kType,
                .flags = flags
            })
        { }

        // public api

        StorageType getCurrentValue() const { return currentValue; }
        StorageType getDefaultValue() const { return defaultValue; }

        void setCurrentValue(StorageType update) { currentValue = update; }

        bool isModified() const override { return currentValue != defaultValue; }

        /// saving to buffers
        void saveCurrentValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(StorageType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(StorageType), size);
            *static_cast<StorageType*>(pData) = currentValue;
        }

        void saveDefaultValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(StorageType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(StorageType), size);
            *static_cast<StorageType*>(pData) = defaultValue;
        }

        /// loading from buffers
        void loadCurrentValue(const void *pData, size_t size) override {
            SM_ASSERTF(size == sizeof(StorageType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(StorageType), size);
            currentValue = StorageType(*static_cast<const StorageType*>(pData));
        }

    protected:
        ConfigValueBase(std::string_view path, StorageType defaultValue, const ConfigEntryInfo& info)
            : IConfigEntry(path, info)
            , defaultValue(defaultValue)
            , currentValue(defaultValue)
        { }

    private:
        const StorageType defaultValue;
        StorageType currentValue;
    };

    template<typename T>
    struct ConfigValue final : public ConfigValueBase<T> {
        using Super = ConfigValueBase<T>;
        using Super::Super;

        using StorageType = typename Super::StorageType;
        using SaveType = typename Super::SaveType;

        virtual bool parseConfigValue(const INode *pNode) override {
            if (SaveType value; pNode->get(value)) {
                Super::setCurrentValue(StorageType(value));
                return true;
            }

            return false;
            // TODO: notify listeners
        }

        void unparseCurrentValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            *static_cast<SaveType*>(pData) = Super::getCurrentValue();
        }

        void unparseDefaultValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            *static_cast<SaveType*>(pData) = Super::getDefaultValue();
        }
    };

    template<typename T> requires std::is_enum_v<T>
    struct ConfigValue<T> final : public ConfigValueBase<T> {
        using Super = ConfigValueBase<T>;
        using Super::Super;

        using StorageType = typename Super::StorageType;
        using SaveType = typename Super::SaveType;

        ConfigValue(std::string_view path, std::string name, std::string_view description, StorageType defaultValue, ConfigFlagMap opts, ValueFlag flags = eConfigDefault)
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

        // our config rep is a std::string
        bool parseConfigValue(const INode *pData) override {
            if (SaveType opt; pData->get(opt)) {
                if (auto it = opts.find(opt); it != opts.end()) {
                    Super::setCurrentValue(T(it->second));
                    return true;
                }
            }

            return false;
        }

        void unparseCurrentValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config enum value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            *static_cast<SaveType*>(pData) = getStringValue(Super::getCurrentValue());
        }

        void unparseDefaultValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config enum value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            *static_cast<SaveType*>(pData) = getStringValue(Super::getDefaultValue());
        }

        // we save the string name of the enum
        void saveCurrentValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(StorageType), "invalid size for config enum value {} (expected {}, got {})", Super::getName(), sizeof(StorageType), size);
            *static_cast<StorageType*>(pData) = Super::getCurrentValue();
        }

        void saveDefaultValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(StorageType), "invalid size for config enum value {} (expected {}, got {})", Super::getName(), sizeof(StorageType), size);
            *static_cast<StorageType*>(pData) = Super::getDefaultValue();
        }

        const ConfigFlagMap& getEnumFlags() const override { return opts; }
        const ConfigNameMap& getEnumNames() const override { return saveOpts; }

    private:
        std::string_view getStringValue(T value) const {
            return saveOpts.at(value);
        }

        ConfigFlagMap opts;
        ConfigNameMap saveOpts;
    };

    IConfigEntry *getConfig();
}
