#pragma once

#include "engine/config/schema.h"

namespace simcoe::config {
    struct IConfig {
        virtual ~IConfig() = default;

        const ISchemaBase *getSchema() const {
            return pSchema;
        }

    protected:
        void setSchema(const ISchemaBase *pNewSchema) {
            pSchema = pNewSchema;
        }

    private:
        const ISchemaBase *pSchema = nullptr;
    };

    struct ConfigFile {
        ConfigFile(const fs::path& path);

        void load(std::string_view sectionName, const IConfig *pConfig) const;

    private:
        std::string name;
        ISource *pSource;
        INode *pRoot;
    };
}
