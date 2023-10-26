#pragma once

#include <simdjson/simdjson.h>

#include <unordered_map>

namespace simcoe {
    namespace schema {
        using JsonDoc = simdjson::dom::document;
        struct ISchema {
            virtual ~ISchema() = default;

            virtual void save(JsonDoc& doc) = 0;
            virtual void load(const JsonDoc& doc) = 0;
        };

        template<typename T>
        struct Digit : ISchema {
            void save(JsonDoc& doc) override;
            void load(const JsonDoc& doc) override;

        private:
            T *pValue = nullptr;
        };

        struct Bool final : ISchema {
            void save(JsonDoc& doc) override;
            void load(const JsonDoc& doc) override;

        private:
            bool *pValue = nullptr;
        };

        struct Map final : ISchema {
            void save(JsonDoc& doc) override;
            void load(const JsonDoc& doc) override;

            std::unordered_map<std::string_view, ISchema*> schemas;
        };
    }

    struct IConfig {
        virtual ~IConfig() = default;

        schema::ISchema *getSchema() { return pSchema; }

    protected:
        void setSchema(schema::ISchema *pNewSchema) {
            pSchema = pNewSchema;
        }

    private:
        schema::ISchema *pSchema = nullptr;
    };
}
