#include "engine/config/system.h"

using namespace simcoe;
using namespace simcoe::config;

// names are in the format of `name/category` or `name`

namespace {
    std::string parseName(std::string_view it) {
        auto pos = it.find_last_of('/');
        if (pos == std::string::npos) {
            return std::string(it);
        }
        return std::string(it.substr(pos + 1));
    }

    std::string parseCategory(std::string_view it) {
        auto pos = it.find_last_of('/');
        if (pos == std::string::npos) {
            return "";
        }
        return std::string(it.substr(0, pos));
    }
}

ConfigValueBase::ConfigValueBase(std::string id, std::string description)
    : name(parseName(id))
    , category(parseCategory(id))
    , description(std::move(description))
{
    SM_ASSERT(!id.empty(), "config value id cannot be empty");
    SM_ASSERT(!name.empty(), "config value name cannot be empty");
}
