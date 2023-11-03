#include "engine/config/ext/schema.h"

#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::config;

// stack for debugging config loading

ConfigContext::ConfigContext(std::string_view file)
    : file(file)
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

bool ConfigContext::verifyConfigField(const INode *pNode, NodeType expected) const {
    if (auto type = pNode->getType(); type != expected) {
        error("expected field {} to be of type {}, got {}", path.back(), expected, type);
        return false;
    }

    return true;
}

// schema base
auto ISchemaBase::region(ConfigContext& ctx) const {
    struct ConfigRegion {
        ConfigRegion(ConfigContext& ctx, std::string_view name) : ctx(ctx) {
            ctx.enter(name);
        }
        ~ConfigRegion() {
            ctx.leave();
        }

    private:
        ConfigContext& ctx;
    };

    return ConfigRegion(ctx, getName());
}

void ISchemaBase::load(ConfigContext& ctx, const INode *pNode) const {
    auto r = region(ctx);
    readNode(ctx, pNode);
}

// config loaders

void String::readNode(ConfigContext& ctx, const INode *pNode) const {
    if (!ctx.verifyConfigField(pNode, eString)) {
        return;
    }

    update(ctx, pNode->getUnchecked<std::string>());
}

void Bool::readNode(ConfigContext& ctx, const INode *pNode) const {
    if (!ctx.verifyConfigField(pNode, eBool)) {
        return;
    }

    update(ctx, pNode->getUnchecked<bool>());
}

void Table::readNode(ConfigContext& ctx, const INode *pNode) const {
    if (!ctx.verifyConfigField(pNode, eTable)) {
        return;
    }

    NodeMap table = pNode->getUnchecked<NodeMap>();

    for (auto& [id, pSchema] : schemas) {
        if (auto it = table.find(std::string(id)); it == table.end()) {
            ctx.error("missing field {}", id);
        } else {
            pSchema->load(ctx, it->second);
        }
    }
}
