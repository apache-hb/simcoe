#include "engine/config/schema.h"

#include "engine/core/strings.h"

#include "engine/service/logging.h"

using namespace simcoe;
using namespace simcoe::config;

// stack for debugging config loading

ConfigContext::ConfigContext(std::string_view file, void *pObjectPtr)
    : pObjectPtr(pObjectPtr)
    , file(file)
{ }

void ConfigContext::enter(std::string_view name) {
    path.emplace_back(name);
}

void ConfigContext::leave() {
    path.pop_back();
}

void ConfigContext::error(std::string_view msg) const {
    std::string trace = util::join(path, "->");
    LOG_WARN("while loading config {}\ntrace: {}\nerror: {}", file, trace, msg);
}

// config loaders

void String::readNode(ConfigContext& ctx, const toml::node& node) const {
    if (std::string str; ctx.get(node, str)) {
        update(ctx, str);
    }
}

void Table::readNode(ConfigContext& ctx, const toml::node& node) const {
    if (toml::table table; ctx.get(node, table)) {
        for (auto& [id, pSchema] : schemas) {
            if (auto field = table[id]; !field) {
                ctx.error("missing field {}", id);
            } else {
                pSchema->load(ctx, *field.node());
            }
        }
    }
}
