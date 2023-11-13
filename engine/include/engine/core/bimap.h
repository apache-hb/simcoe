#pragma once

#include <unordered_map>

namespace simcoe::core {
    template<typename TKey, typename TValue>
    struct BiMap {
        using Pair = std::pair<TKey, TValue>;
        using KeyToValue = std::unordered_map<TKey, TValue>;
        using ValueToKey = std::unordered_map<TValue, TKey>;

        BiMap() = default;

        BiMap(KeyToValue&& keys) 
            : keyToValue(std::move(keys))
        {
            for (const auto& [key, value] : keyToValue) {
                valueToKey[value] = key;
            }
        }

        BiMap(ValueToKey&& values)
            : valueToKey(std::move(values))
        {
            for (const auto& [value, key] : valueToKey) {
                keyToValue[key] = value;
            }
        }

        BiMap(std::initializer_list<Pair> pairs) {
            for (const auto& [key, value] : pairs) {
                add(key, value);
            }
        }

        void add(const TKey& key, const TValue& value) {
            keyToValue[key] = value;
            valueToKey[value] = key;
        }

        auto findKey(const TKey& key) const {
            return keyToValue.find(key);
        }

        auto findValue(const TValue& value) const {
            return valueToKey.find(value);
        }

        bool hasKey(const TKey& key) const {
            return findKey(key) != keyToValue.end();
        }

        bool hasValue(const TValue& value) const {
            return findValue(value) != valueToKey.end();
        }

        const TValue& getValue(const TKey& key) const {
            return keyToValue.at(key);
        }

        const TKey& getKey(const TValue& value) const {
            return valueToKey.at(value);
        }

        const KeyToValue& getKeyToValue() const {
            return keyToValue;
        }

        const ValueToKey& getValueToKey() const {
            return valueToKey;
        }
    private:
        KeyToValue keyToValue;
        ValueToKey valueToKey;
    };
}