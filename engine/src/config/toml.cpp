#include "engine/config/source.h"
#include "engine/config/system.h"

#include "engine/core/error.h"
#include "engine/log/service.h"

#define TOML_HEADER_ONLY 0
#include <toml++/toml.h>

#include <fstream>

using namespace simcoe;
using namespace simcoe::config;

struct TomlNode final : INode {
    TomlNode(toml::node *pNode)
        : pData(pNode)
    { }

    TomlNode(bool value)
        : TomlNode(new toml::value<bool>(value))
    { }

    TomlNode(int64_t value)
        : TomlNode(new toml::value<int64_t>(value))
    { }

    TomlNode(float value)
        : TomlNode(new toml::value<double>(value))
    { }

    TomlNode(std::string value)
        : TomlNode(new toml::value<std::string>(std::move(value)))
    { }

    TomlNode(toml::table value)
        : TomlNode(new toml::table(value))
    { }

    ~TomlNode() override {
        delete pData;
    }

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

    bool get(float &value) const override {
        if (pData->is_floating_point()) {
            value = float(pData->as_floating_point()->get());
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

    ValueType getType() const override {
        switch (pData->type()) {
        case toml::node_type::boolean: return eConfigBool;
        case toml::node_type::integer: return eConfigInt;
        case toml::node_type::floating_point: return eConfigFloat;
        case toml::node_type::string: return eConfigString;
        case toml::node_type::table: return eConfigGroup;
        default: return eConfigError;
        }
    }

    toml::node *getTomlNode() const {
        return pData;
    }

private:
    toml::node *pData;
};

struct TomlSource final : ISource {
    const INode *load(const fs::path &path) override {
        try {
            fs::path cfg = fs::current_path() / path;
            cfg.replace_extension("toml");
            auto str = cfg.string();

            LOG_INFO("loading config file {}", str);

            // TODO: this leaks alot of memory
            return new TomlNode(toml::parse_file(str));
        } catch (const toml::parse_error &e) {
            LOG_ERROR("while parsing toml file {}\n{}", path.string(), e.what());
            return nullptr;
        } catch (const core::Error &e) {
            LOG_ERROR("while loading toml file {}\n{}", path.string(), e.what());
            return nullptr;
        }
    }

    const INode *create(bool value) override {
        return new TomlNode(value);
    }

    const INode *create(int64_t value) override {
        return new TomlNode(value);
    }

    const INode *create(float value) override {
        return new TomlNode(value);
    }

    const INode *create(std::string_view value) override {
        return new TomlNode(std::string(value));
    }

    const INode *create(const NodeMap &value) override {
        toml::table out;
        for (auto &[name, pNode] : value) {
            toml::node *pInnerNode = static_cast<const TomlNode*>(pNode)->getTomlNode();
            out.insert_or_assign(name, *pInnerNode);
        }

        return new TomlNode(out);
    }

    bool save(const fs::path &path, const INode *pNode) override {
        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            LOG_ERROR("failed to open config file {}", path.string());
            return false;
        }

        toml::table *pTable = static_cast<toml::table*>(static_cast<const TomlNode*>(pNode)->getTomlNode());
        ofs << *pTable;
        return true;
    }
};

ISource *config::newTomlSource() {
    return new TomlSource{};
}
