#include "row.h"
#include <cstring>
#include <stdexcept>

// Binary layout per record:
//   [uint16_t num_cols]
//   For each column:
//     [uint8_t col_type]          (0=INT, 1=TEXT)
//     INT:  [int32_t value]
//     TEXT: [uint32_t len][char* data]

std::vector<char> serializeDynamic(const DynamicRecord& rec, const Schema& schema) {
    if (rec.values.size() != schema.columns.size())
        throw std::runtime_error("serializeDynamic: value count != column count");

    std::vector<char> buf;
    auto appendRaw = [&](const void* src, size_t n) {
        const char* p = static_cast<const char*>(src);
        buf.insert(buf.end(), p, p + n);
    };

    uint16_t ncols = static_cast<uint16_t>(schema.columns.size());
    appendRaw(&ncols, sizeof(ncols));

    for (int i = 0; i < (int)schema.columns.size(); ++i) {
        uint8_t t = static_cast<uint8_t>(schema.columns[i].type);
        appendRaw(&t, sizeof(t));

        if (schema.columns[i].type == ColType::INT) {
            int32_t v = std::get<int32_t>(rec.values[i]);
            appendRaw(&v, sizeof(v));
        } else {
            const std::string& s = std::get<std::string>(rec.values[i]);
            uint32_t len = static_cast<uint32_t>(s.size());
            appendRaw(&len, sizeof(len));
            appendRaw(s.data(), len);
        }
    }
    return buf;
}

DynamicRecord deserializeDynamic(const std::vector<char>& buf, const Schema& schema) {
    DynamicRecord rec;
    size_t off = 0;
    auto readRaw = [&](void* dst, size_t n) {
        if (off + n > buf.size())
            throw std::runtime_error("deserializeDynamic: buffer underflow");
        memcpy(dst, buf.data() + off, n);
        off += n;
    };

    uint16_t ncols = 0;
    readRaw(&ncols, sizeof(ncols));

    for (uint16_t i = 0; i < ncols; ++i) {
        uint8_t t = 0;
        readRaw(&t, sizeof(t));

        if (static_cast<ColType>(t) == ColType::INT) {
            int32_t v = 0;
            readRaw(&v, sizeof(v));
            rec.values.emplace_back(v);
        } else {
            uint32_t len = 0;
            readRaw(&len, sizeof(len));
            std::string s(len, '\0');
            if (len > 0) readRaw(s.data(), len);
            rec.values.emplace_back(std::move(s));
        }
    }
    return rec;
}

int getPrimaryKey(const DynamicRecord& rec, const Schema& schema) {
    int pkIdx = schema.primaryKeyIndex();
    if (pkIdx < 0)
        throw std::runtime_error("Table has no INT primary key column");
    return rec.getInt(pkIdx);
}
