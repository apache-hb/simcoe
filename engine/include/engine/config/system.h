#pragma once

#include <string>
#include <unordered_map>

namespace simcoe::config {
    struct ConfigValueBase {
        ConfigValueBase(std::string id, std::string description);

        std::string_view getName() const { return name; }
        std::string_view getCategory() const { return category; }
        std::string_view getDescription() const { return description; }

    private:
        std::string name;
        std::string category;
        std::string description;
    };

    template<typename T>
    struct ConfigValue : ConfigValueBase {
        ConfigValue(std::string name, std::string description, T defaultValue)
            : ConfigValueBase(name, description)
            , defaultValue(defaultValue)
            , currentValue(defaultValue)
        { }

        T getValue() const { return currentValue; }
        void setValue(T update) { currentValue = update; }

    private:
        const T defaultValue;
        T currentValue;
    };
}
