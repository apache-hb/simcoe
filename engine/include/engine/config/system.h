#pragma once

#include <string>

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
            , value(defaultValue)
        { }

        operator T() const {
            return value;
        }
    private:
        T value;
    };
}
