#pragma once

#include "engine/config/source.h"

#include "engine/core/bimap.h"
#include "engine/core/panic.h"
#include "engine/core/units.h"
#include "engine/core/mt.h"

#include "engine/threads/mutex.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace simcoe::config {
    struct IConfigEntry;

    using ConfigMap = std::unordered_map<std::string, IConfigEntry*>;
    using ConfigEnumMap = core::BiMap<std::string_view, int64_t>;

    enum ValueFlag {
        eDefault = 0, ///< default

        eDynamic = 1 << 0, ///< this entry can be modified at runtime
    };

    struct ConfigEntryInfo {
        std::string_view name; // name
        std::string_view description; // description
        ValueType type;
        ValueFlag flags;
    };

    struct IConfigEntry {
        // constructor for groups
        IConfigEntry(const ConfigEntryInfo& info);

        // constructor for a child of the config
        IConfigEntry(std::string_view path, const ConfigEntryInfo& info);

        std::string_view getName() const { return entryInfo.name; }
        std::string_view getDescription() const { return entryInfo.description; }

        bool hasFlag(ValueFlag flag) const { return (entryInfo.flags & flag) != 0; }
        ValueType getType() const { return entryInfo.type; }

        virtual bool isModified() const = 0;

        ///
        /// config parsing and unparsing
        ///

        /**
         * @brief parse a value from some other data
         *
         * @param pData the node to parse from
         * @return true if the value was loaded properly
         */
        virtual bool readConfigValue(SM_UNUSED const INode *pNode) { SM_NEVER("readConfigValue {}", getName()); }

        /**
         * @brief parse a value from our save type
         *
         * @param pData
         * @param size
         */
        virtual void parseValue(SM_UNUSED const void *pData, SM_UNUSED size_t size) { SM_NEVER("parseValue {}", getName()); }

        /**
         * @brief unparse a value into a node
         *
         * @param pSource the node source
         * @return const INode* the unparsed node
         */
        virtual void unparseCurrentValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("uparseCurrentValue {}", getName()); }

        /**
         * @brief unparse the default value into a format suitable for saving and displaying to the user
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void unparseDefaultValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("uparseDefaultValue {}", getName()); }



        ///
        /// deals with raw internal data
        ///

        /**
         * @brief save the current value to a buffer
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void saveCurrentValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("saveCurrentValue {}", getName()); }

        /**
         * @brief save the default value to a buffer
         *
         * @param pData the buffer to save to
         * @param size the size of the buffer
         */
        virtual void saveDefaultValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const { SM_NEVER("saveDefaultValue {}", getName()); }

        /**
         * @brief load the current value from a buffer
         *
         * @param pData data to load from
         * @param size the size of the data
         */
        virtual void loadCurrentValue(SM_UNUSED const void *pData, SM_UNUSED size_t size) { SM_NEVER("loadCurrentValue {}", getName()); }



        ///
        /// extra config data
        ///

        virtual const ConfigEnumMap& getEnumOptions() const { SM_NEVER("getEnumOptions {}", getName()); }
        
        virtual const ConfigMap& getChildren() const { SM_NEVER("getChildren {}", getName()); }

    private:
        ConfigEntryInfo entryInfo;
    };

    IConfigEntry *getConfig();
}
