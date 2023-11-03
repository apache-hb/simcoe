#include "engine/config/source.h"

#include "engine/log/service.h"

#include <toml++/toml.h>

using namespace simcoe;
using namespace simcoe::config;

struct TomlNode final : INode {
    TomlNode(toml::node *pNode)
        : pData(pNode)
    { }

    bool get(bool &value) const override {
        if (pData->is_boolean()) {
            value = pData->as_boolean()->get();
            return true;
        }

        return false;
    }

    bool get(int64_t &value) const override {
        if (pData->is_integer()) {
            value = pData->as_integer()->get();
            return true;
        }

        return false;
    }

    bool get(std::string &value) const override {
        if (pData->is_string()) {
            value = pData->as_string()->get();
            return true;
        }

        return false;
    }

    bool get(NodeMap &value) const override {
        if (pData->is_table()) {
            auto table = pData->as_table();
            for (auto &[key, node] : *table) {
                value.emplace(key, new TomlNode{&node});
            }

            return true;
        }

        return false;
    }

    bool get(NodeVec &value) const override {
        if (pData->is_array()) {
            auto array = pData->as_array();
            for (auto &node : *array) {
                value.emplace_back(new TomlNode{&node});
            }

            return true;
        }

        return false;
    }

    NodeType getType() const override {
        switch (pData->type()) {
        case toml::node_type::boolean: return eBool;
        case toml::node_type::integer: return eInt;
        case toml::node_type::floating_point: return eFloat;
        case toml::node_type::string: return eString;
        case toml::node_type::table: return eTable;
        case toml::node_type::array: return eArray;
        default: return eUnkonwn;
        }
    }

private:
    toml::node *pData;
};

struct TomlSource final : ISource {
    TomlSource(toml::table root)
        : root(std::move(root))
    { }

    INode *load() override {
        return new TomlNode{&root};
    }

private:
    toml::table root;
};

ISource *config::loadToml(const fs::path &path) try {
    fs::path cfg = fs::current_path() / path;
    cfg.replace_extension("toml");
    auto str = cfg.string();

    LOG_INFO("loading config file {}", str);

    auto table = toml::parse_file(str);
    return new TomlSource{std::move(table)};
} catch (const toml::parse_error &e) {
    LOG_ERROR("while parsing toml file {}\n{}", path.string(), e.what());
    return nullptr;
} catch (const std::exception &e) {
    LOG_ERROR("while loading toml file {}\n{}", path.string(), e.what());
    return nullptr;
}
