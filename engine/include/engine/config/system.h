#pragma once

#include "engine/config/source.h"

#include "engine/core/panic.h"
#include "engine/core/units.h"
#include "engine/core/mt.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace simcoe::config {
    struct IConfigEntry;

    using ConfigMap = std::unordered_map<std::string, IConfigEntry*>;
    using ConfigFlagMap = std::unordered_map<std::string_view, int64_t>;
    using ConfigNameMap = std::unordered_map<int64_t, std::string_view>;

    enum ValueFlag {
        eDefault = 0, ///< default

        eDynamic = 1 << 0, ///< this entry can be modified at runtime
    };

    struct ConfigEntryInfo {
        std::string_view name; // name
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

            using VisibleType = bool;
            using StorageType = std::atomic<bool>;
            using SaveType = bool;
        };

        template<>
        struct ConfigValueTraits<std::string> {
            static constexpr ValueType kType = eConfigString;

            using VisibleType = std::string;
            using StorageType = std::string;
            using SaveType = std::string;
        };

        template<std::integral T>
        struct ConfigValueTraits<T> {
            static constexpr ValueType kType = eConfigInt;

            using VisibleType = T;
            using StorageType = std::atomic<T>;
            using SaveType = int64_t;
        };

        template<typename T> requires std::is_enum_v<T>
        struct ConfigValueTraits<T> {
            static constexpr ValueType kType = eConfigEnum;

            using VisibleType = T;
            using StorageType = std::atomic<T>;
            using SaveType = std::string;
        };

        template<>
        struct ConfigValueTraits<float> {
            static constexpr ValueType kType = eConfigFloat;

            using VisibleType = float;
            using StorageType = std::atomic<float>;
            using SaveType = float;
        };
    }

    struct IConfigEntry {
        // constructor for groups
        IConfigEntry(const ConfigEntryInfo& info);

        // constructor for a child of the config
        IConfigEntry(std::string_view path, const ConfigEntryInfo& info);

        std::string_view getName() const { return entryInfo.name; }
        std::string_view getDescription() const { return entryInfo.description; }

        bool hasFlag(ValueFlag flag) const { return (entryInfo.flags & flag) != 0; }
        ValueType getType() const { return entryInfo.type; }

        virtual bool isModified() const = 0;

        ///
        /// config parsing and unparsing
        ///

        /**
         * @brief parse a value from some other data
         *
         * @param pData the node to parse from
         * @return true if the value was loaded properly
         */
        virtual bool readConfigValue(SM_UNUSED const INode *pNode) { SM_NEVER("readConfigValue {}", getName()); }

        /**
         * @brief parse a value from our save type
         *
         * @param pData
         * @param size
         */
        virtual void parseValue(SM_UNUSED const void *pData, SM_UNUSED size_t size) { SM_NEVER("parseValue {}", getName()); }

        /**
         * @brief unparse a value into a node
         *
         * @param pSource the node source
         * @return const INode* the unparsed node
         */
        virtual void unparseCurrentValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("uparseCurrentValue {}", getName()); }

        /**
         * @brief unparse the default value into a format suitable for saving and displaying to the user
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void unparseDefaultValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("uparseDefaultValue {}", getName()); }



        ///
        /// deals with raw internal data
        ///

        /**
         * @brief save the current value to a buffer
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void saveCurrentValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("saveCurrentValue {}", getName()); }

        /**
         * @brief save the default value to a buffer
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void saveDefaultValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("saveDefaultValue {}", getName()); }

        /**
         * @brief load the current value from a buffer
         *
         * @param pData data to load from
         * @param size the size of the data
         */
        virtual void loadCurrentValue(SM_UNUSED const void *pData, SM_UNUSED size_t size) { SM_NEVER("loadCurrentValue {}", getName()); }



        ///
        /// extra config data
        ///

        virtual const ConfigFlagMap& getEnumFlags() const { SM_NEVER("getEnumFlags {}", getName()); }
        virtual const ConfigNameMap& getEnumNames() const { SM_NEVER("getEnumNames {}", getName()); }
        virtual const ConfigMap& getChildren() const { SM_NEVER("getChildren {}", getName()); }

    private:
        ConfigEntryInfo entryInfo;
    };

    template<typename T>
    struct ConfigValueInfo {
        using Traits = detail::ConfigValueTraits<T>;
        using VisibleType = typename Traits::VisibleType;
        using NotifyConfigUpdate = std::function<void(const T& previous, const T& current)>;

        std::string_view name;
        std::string_view description;
        VisibleType defaultValue;

        NotifyConfigUpdate notify;
        ValueFlag flags = eDefault;
    };

    template<typename T>
    struct ConfigValueBase : public IConfigEntry {
        using Super = IConfigEntry;
        using Super::Super;

        using Traits = detail::ConfigValueTraits<T>;
        using VisibleType = typename Traits::VisibleType;
        using SaveType = typename Traits::SaveType;
        using StorageType = typename Traits::StorageType;

        ConfigValueBase(std::string_view path, std::string_view name, std::string_view description, VisibleType defaultValue, ValueFlag flags = eDefault)
            : ConfigValueBase(path, Traits::kType, { .name = name, .description = description, .defaultValue = defaultValue, .flags = flags })
        { }

        ConfigValueBase(std::string_view path, const ConfigValueInfo<T>& info)
            : ConfigValueBase(path, Traits::kType, info)
        { }

        // public api
        bool isModified() const override { return getDefaultValue() != getCurrentValue(); }

        virtual VisibleType getCurrentValue() const { return currentValue; }
        const VisibleType& getDefaultValue() const { return valueInfo.defaultValue; }

        // update value
        virtual void setCurrentValue(VisibleType update) {
            notifyUpdate(currentValue, update);
            updateCurrentValue(update);
        }

        /// saving to buffers
        void saveCurrentValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(VisibleType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(VisibleType), size);
            *static_cast<VisibleType*>(pData) = getCurrentValue();
        }

        void saveDefaultValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(VisibleType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(VisibleType), size);
            *static_cast<VisibleType*>(pData) = getDefaultValue();
        }

        /// loading from buffers
        void loadCurrentValue(const void *pData, size_t size) override {
            SM_ASSERTF(size == sizeof(VisibleType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(VisibleType), size);
            auto newValue = *static_cast<const VisibleType*>(pData);
            setCurrentValue(newValue);
        }

    protected:
        void notifyUpdate(VisibleType previous, VisibleType current) {
            if (valueInfo.notify) { valueInfo.notify(previous, current); }
        }

        void updateCurrentValue(VisibleType update) {
            currentValue = update;
        }

        ConfigValueBase(std::string_view path, ValueType type, const ConfigValueInfo<T>& info)
            : IConfigEntry(path, { .name = info.name, .description = info.description, .type = type, .flags = info.flags })
            , valueInfo(info)
            , currentValue(info.defaultValue)
        { }

    private:
        ConfigValueInfo<T> valueInfo;
        StorageType currentValue;
    };

    template<typename T>
    struct ConfigValue final : public ConfigValueBase<T> {
        using Super = ConfigValueBase<T>;
        using Super::Super;

        using VisibleType = typename Super::VisibleType;
        using StorageType = typename Super::StorageType;
        using SaveType = typename Super::SaveType;

        virtual bool readConfigValue(const INode *pNode) override {
            if (SaveType value; pNode->get(value)) {
                Super::setCurrentValue(VisibleType(value));
                return true;
            }

            return false;
        }

        void parseValue(const void *pData, size_t size) override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            Super::setCurrentValue(VisibleType(*static_cast<const SaveType*>(pData)));
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

    template<>
    struct ConfigValue<std::string> final : public ConfigValueBase<std::string> {
        using Super = ConfigValueBase<std::string>;
        using Super::Super;

        using VisibleType = typename Super::VisibleType;
        using StorageType = typename Super::StorageType;
        using SaveType = typename Super::SaveType;

        // TODO: this is all duplicated from ConfigValue<T>
        virtual bool readConfigValue(const INode *pNode) override {
            if (SaveType value; pNode->get(value)) {
                Super::setCurrentValue(VisibleType(value));
                return true;
            }

            return false;
        }

        void parseValue(const void *pData, size_t size) override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            Super::setCurrentValue(VisibleType(*static_cast<const SaveType*>(pData)));
        }

        void unparseCurrentValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            *static_cast<SaveType*>(pData) = Super::getCurrentValue();
        }

        void unparseDefaultValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            *static_cast<SaveType*>(pData) = Super::getDefaultValue();
        }


        // specialized because strings arent atomic
        VisibleType getCurrentValue() const override {
            mt::read_lock lock(mutex);
            return Super::getCurrentValue();
        }

        void setCurrentValue(VisibleType update) override {
            Super::notifyUpdate(Super::getCurrentValue(), update);

            mt::write_lock lock(mutex);
            Super::updateCurrentValue(update);
        }

    private:
        mutable mt::shared_mutex mutex;
    };

    template<typename T> requires std::is_enum_v<T>
    struct ConfigValue<T> final : public ConfigValueBase<T> {
        using Super = ConfigValueBase<T>;
        using Super::Super;

        using VisibleType = typename Super::VisibleType;
        using StorageType = typename Super::StorageType;
        using SaveType = typename Super::SaveType;

        ConfigValue(std::string_view path, std::string_view name, std::string_view description, VisibleType defaultValue, ConfigFlagMap opts, ValueFlag flags = eDefault)
            : Super(path, eConfigEnum, {
                .name = name,
                .description = description,
                .defaultValue = defaultValue,
                .flags = flags
            })
            , opts(opts)
        {
            for (auto& [key, value] : opts) {
                saveOpts[value] = key;
            }
        }

        // our config rep is a std::string
        bool readConfigValue(const INode *pData) override {
            if (SaveType opt; pData->get(opt)) {
                if (auto it = opts.find(opt); it != opts.end()) {
                    Super::setCurrentValue(T(it->second));
                    return true;
                }
            }

            return false;
        }

        void parseValue(const void *pData, size_t size) override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config enum value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            Super::setCurrentValue(getValueByName(*static_cast<const SaveType*>(pData)));
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
            SM_ASSERTF(size == sizeof(VisibleType), "invalid size for config enum value {} (expected {}, got {})", Super::getName(), sizeof(StorageType), size);
            *static_cast<VisibleType*>(pData) = Super::getCurrentValue();
        }

        void saveDefaultValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(VisibleType), "invalid size for config enum value {} (expected {}, got {})", Super::getName(), sizeof(StorageType), size);
            *static_cast<VisibleType*>(pData) = Super::getDefaultValue();
        }

        const ConfigFlagMap& getEnumFlags() const override { return opts; }
        const ConfigNameMap& getEnumNames() const override { return saveOpts; }

    private:
        std::string_view getStringValue(T value) const {
            return saveOpts.at(value);
        }

        VisibleType getValueByName(std::string_view name) const {
            return core::enumCast<VisibleType>(opts.at(name));
        }

        ConfigFlagMap opts;
        ConfigNameMap saveOpts;
    };

    IConfigEntry *getConfig();
}
