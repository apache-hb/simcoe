#include "engine/config/schema.h"

#include "engine/core/strings.h"

#include "engine/log/service.h"

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

bool ConfigContext::verifyConfigField(const toml::node& node, toml::node_type expected) {
    if (node.type() != expected) {
        error("expected field {} to be of type {}, got {}", path.back(), expected, node.type());
        return false;
    }

    return true;
}

// config loaders

void String::readNode(ConfigContext& ctx, const toml::node& node) const {
    if (!ctx.verifyConfigField(node, toml::node_type::string)) {
        return;
    }

    std::string str = node.as_string()->get();
    update(ctx, str);
}

void Table::readNode(ConfigContext& ctx, const toml::node& node) const {
    if (!ctx.verifyConfigField(node, toml::node_type::table)) {
        return;
    }

    toml::table table = *node.as_table();

    for (auto& [id, pSchema] : schemas) {
        if (auto field = table[id]; !field) {
            ctx.error("missing field {}", id);
        } else {
            pSchema->load(ctx, *field.node());
        }
    }
}
