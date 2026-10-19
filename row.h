#pragma once
#include <string>
#include <vector>
#include <variant>
#include <cstdint>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Column type & Schema
// ---------------------------------------------------------------------------

enum class ColType : uint8_t {
    INT  = 0,
    TEXT = 1
};

struct Column {
    std::string name;
    ColType     type;
    bool        isPrimaryKey = false;
};

struct Schema {
    std::string         tableName;
    std::vector<Column> columns;

    /** Index of the PRIMARY KEY column, or -1 if none explicitly set */
    int primaryKeyIndex() const {
        for (int i = 0; i < (int)columns.size(); ++i)
            if (columns[i].isPrimaryKey) return i;
        return firstIntIndex();   // fallback: first INT column
    }

    /** Index of the first INT column, -1 if none */
    int firstIntIndex() const {
        for (int i = 0; i < (int)columns.size(); ++i)
            if (columns[i].type == ColType::INT) return i;
        return -1;
    }

    bool hasColumn(const std::string& name) const {
        for (auto& c : columns)
            if (c.name == name) return true;
        return false;
    }

    int columnIndex(const std::string& name) const {
        for (int i = 0; i < (int)columns.size(); ++i)
            if (columns[i].name == name) return i;
        return -1;
    }
};

// ---------------------------------------------------------------------------
// Dynamic value & record
// ---------------------------------------------------------------------------

using Value = std::variant<int32_t, std::string>;

struct DynamicRecord {
    std::vector<Value> values;

    int32_t getInt(int idx) const {
        return std::get<int32_t>(values.at(idx));
    }
    const std::string& getText(int idx) const {
        return std::get<std::string>(values.at(idx));
    }
    std::string toString(int idx) const {
        if (values[idx].index() == 0)
            return std::to_string(std::get<int32_t>(values[idx]));
        return std::get<std::string>(values[idx]);
    }
};

// ---------------------------------------------------------------------------
// Serialization helpers
// ---------------------------------------------------------------------------

/** Serialise a DynamicRecord to a byte buffer according to its Schema. */
std::vector<char> serializeDynamic(const DynamicRecord& rec, const Schema& schema);

/** Deserialise a byte buffer back to a DynamicRecord using its Schema. */
DynamicRecord deserializeDynamic(const std::vector<char>& buf, const Schema& schema);

/** Extract primary key (int) from a record; throws if no PK column exists. */
int getPrimaryKey(const DynamicRecord& rec, const Schema& schema);
