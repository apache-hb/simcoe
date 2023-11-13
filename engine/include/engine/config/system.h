#pragma once

#include "engine/config/config.h"

namespace simcoe::config {
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

    // TODO: this is a catastrophe

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

        using Self = ConfigValue<std::string>;

        using VisibleType = typename Super::VisibleType;
        using StorageType = typename Super::StorageType;
        using SaveType = typename Super::SaveType;

        ConfigValue(std::string_view path, std::string_view name, std::string_view description, VisibleType defaultValue, ValueFlag flags = eDefault)
            : Self(path, { .name = name, .description = description, .defaultValue = defaultValue, .flags = flags })
        { }

        ConfigValue(std::string_view path, const ConfigValueInfo<std::string>& info)
            : ConfigValueBase(path, Traits::kType, info)
            , mutex(info.name)
        { }

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
            mt::ReadLock lock(mutex);
            return Super::getCurrentValue();
        }

        void setCurrentValue(VisibleType update) override {
            Super::notifyUpdate(Super::getCurrentValue(), update);

            mt::WriteLock lock(mutex);
            Super::updateCurrentValue(update);
        }

    private:
        mutable mt::SharedMutex mutex;
    };

    template<typename T> requires std::is_enum_v<T>
    struct ConfigValue<T> final : public ConfigValueBase<T> {
        using Super = ConfigValueBase<T>;
        using Super::Super;

        using VisibleType = typename Super::VisibleType;
        using StorageType = typename Super::StorageType;
        using SaveType = typename Super::SaveType;

        ConfigValue(std::string_view path, std::string_view name, std::string_view description, VisibleType defaultValue, ConfigEnumMap opts, ValueFlag flags = eDefault)
            : Super(path, eConfigEnum, {
                .name = name,
                .description = description,
                .defaultValue = defaultValue,
                .flags = flags
            })
            , opts(opts)
        { }

        // our config rep is a std::string
        bool readConfigValue(const INode *pData) override {
            if (SaveType opt; pData->get(opt)) {
                if (opts.hasKey(opt)) {
                    Super::setCurrentValue(T(opts.getValue(opt)));
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
            *static_cast<SaveType*>(pData) = getNameByValue(Super::getCurrentValue());
        }

        void unparseDefaultValue(void *pData, size_t size) const override {
            SM_ASSERTF(size == sizeof(SaveType), "invalid size for config enum value {} (expected {}, got {})", Super::getName(), sizeof(SaveType), size);
            *static_cast<SaveType*>(pData) = getNameByValue(Super::getDefaultValue());
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

        const ConfigEnumMap& getEnumOptions() const override { return opts; }

    private:
        const std::string_view& getNameByValue(T value) const {
            return opts.getKey(core::enumCast<StorageType>(value));
        }

        VisibleType getValueByName(std::string_view name) const {
            return core::enumCast<VisibleType>(opts.getValue(name));
        }

        ConfigEnumMap opts;
    };

    IConfigEntry *getConfig();
}
