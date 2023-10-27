#pragma once

#include <unordered_map>

namespace simcoe::core {
    template<typename TKey, typename TValue>
    struct BiMap {
        struct Entry {
            TKey key;
            TValue value;
        };

        BiMap(std::initializer_list<Entry> entries) {
            for (const auto& entry : entries) {
                addPair(entry.key, entry.value);
            }
        }

        void addPair(TKey key, TValue value) {
            ASSERT(!hasKey(key));
            ASSERT(!hasValue(value));

            keyToValue[key] = value;
            valueToKey[value] = key;
        }

        TValue getValue(TKey key) const { return keyToValue.at(key); }
        TKey getKey(TValue value) const { return valueToKey.at(value); }

        bool hasKey(TKey key) const { return keyToValue.contains(key); }
        bool hasValue(TValue value) const { return valueToKey.contains(value); }

    private:
        std::unordered_map<TKey, TValue> keyToValue;
        std::unordered_map<TValue, TKey> valueToKey;
    };
}