#ifndef TABLE_H
#define TABLE_H

#include "diskmanager.h"
#include "persistent.h"
#include <string>
#include <vector>

struct Record {
    int id;
    std::string name;
    int age;

    bool operator==(const Record& other) const {
        return id == other.id && name == other.name && age == other.age;
    }
};

std::vector<char> serializeRecord(const Record& record);
Record deserializeRecord(const std::vector<char>& buffer);

class Table {
private:
    DiskManager disk;
    BPlusTree index;

    void unlinkDataPage(uint32_t pageId);

public:
    explicit Table(const std::string& dbFilename);
    ~Table();

    // CRUD Operations
    bool insert(const Record& record);
    bool get(int key, Record& result);
    bool update(int key, const Record& record);
    bool remove(int key);

    // Range Queries
    std::vector<Record> rangeQuery(int low, int high);

    // Diagnostics & Inspection
    void printTableStats();
    void printIndexTree();
    DiskManager& getDiskManager() { return disk; }
    BPlusTree& getIndex() { return index; }
};

#endif // TABLE_H
