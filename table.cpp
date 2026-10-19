#include "table.h"
#include <iostream>
#include <iomanip>

std::vector<char> serializeRecord(const Record& record) {
    std::vector<char> buffer;
    uint32_t nameLength = static_cast<uint32_t>(record.name.size());

    const char* idPtr = reinterpret_cast<const char*>(&record.id);
    buffer.insert(buffer.end(), idPtr, idPtr + sizeof(record.id));

    const char* lenPtr = reinterpret_cast<const char*>(&nameLength);
    buffer.insert(buffer.end(), lenPtr, lenPtr + sizeof(nameLength));

    buffer.insert(buffer.end(), record.name.begin(), record.name.end());

    const char* agePtr = reinterpret_cast<const char*>(&record.age);
    buffer.insert(buffer.end(), agePtr, agePtr + sizeof(record.age));

    return buffer;
}

Record deserializeRecord(const std::vector<char>& buffer) {
    Record record{0, "", 0};
    if (buffer.size() < sizeof(int) + sizeof(uint32_t) + sizeof(int)) {
        return record;
    }

    size_t offset = 0;
    memcpy(&record.id, buffer.data() + offset, sizeof(record.id));
    offset += sizeof(record.id);

    uint32_t nameLength = 0;
    memcpy(&nameLength, buffer.data() + offset, sizeof(nameLength));
    offset += sizeof(nameLength);

    if (offset + nameLength <= buffer.size()) {
        record.name.assign(buffer.data() + offset, nameLength);
        offset += nameLength;
    }

    if (offset + sizeof(record.age) <= buffer.size()) {
        memcpy(&record.age, buffer.data() + offset, sizeof(record.age));
    }

    return record;
}

Table::Table(const std::string& dbFilename)
    : disk(dbFilename), index(disk) {}

Table::~Table() {
    disk.flush();
}

void Table::unlinkDataPage(uint32_t pageId) {
    uint32_t curr = disk.getFirstDataPageId();
    uint32_t prev = INVALID_PAGE_ID;

    while (curr != INVALID_PAGE_ID) {
        Page currPage = disk.readPage(curr);
        uint32_t next = currPage.getHeader().nextPageId;

        if (curr == pageId) {
            if (prev == INVALID_PAGE_ID) {
                disk.setFirstDataPageId(next);
            } else {
                Page prevPage = disk.readPage(prev);
                PageHeader prevHeader = prevPage.getHeader();
                prevHeader.nextPageId = next;
                prevPage.setHeader(prevHeader);
                disk.writePage(prev, prevPage);
            }
            break;
        }
        prev = curr;
        curr = next;
    }
}

bool Table::insert(const Record& record) {
    // Check if key already exists
    RID existing = index.search(record.id);
    if (existing.isValid()) {
        return update(record.id, record);
    }

    std::vector<char> bytes = serializeRecord(record);
    uint16_t reqSize = static_cast<uint16_t>(bytes.size() + sizeof(Slot));

    uint32_t targetPageId = INVALID_PAGE_ID;
    int slotId = -1;

    // First, try to find an existing data page with space
    uint32_t curr = disk.getFirstDataPageId();
    while (curr != INVALID_PAGE_ID) {
        Page p = disk.readPage(curr);
        if (p.getTotalFreeSpace() >= reqSize) {
            slotId = p.insertRecord(bytes);
            if (slotId != -1) {
                targetPageId = curr;
                disk.writePage(curr, p);
                break;
            }
        }
        curr = p.getHeader().nextPageId;
    }

    // If no existing data page had room, allocate a new data page
    if (targetPageId == INVALID_PAGE_ID) {
        targetPageId = disk.allocatePage(PAGE_TYPE_DATA);
        Page newPage(PAGE_TYPE_DATA);
        PageHeader h = newPage.getHeader();
        h.nextPageId = disk.getFirstDataPageId();
        newPage.setHeader(h);

        slotId = newPage.insertRecord(bytes);
        if (slotId == -1) {
            disk.deallocatePage(targetPageId);
            return false;
        }
        disk.writePage(targetPageId, newPage);
        disk.setFirstDataPageId(targetPageId);
    }

    RID rid{targetPageId, static_cast<uint16_t>(slotId)};
    index.insert(record.id, rid);
    return true;
}

bool Table::get(int key, Record& result) {
    RID rid = index.search(key);
    if (!rid.isValid()) {
        return false;
    }

    Page page = disk.readPage(rid.pageId);
    std::vector<char> bytes = page.getRecord(rid.slotId);
    if (bytes.empty()) {
        return false;
    }

    result = deserializeRecord(bytes);
    return true;
}

bool Table::update(int key, const Record& record) {
    RID rid = index.search(key);
    if (!rid.isValid()) {
        return false;
    }

    Page page = disk.readPage(rid.pageId);
    std::vector<char> newBytes = serializeRecord(record);

    // Try in-place/defragmented update on current page
    bool updated = page.updateRecord(rid.slotId, newBytes);
    if (updated) {
        disk.writePage(rid.pageId, page);
        return true;
    }

    // Record grew larger than available page capacity; migrate to another page
    page.deleteRecord(rid.slotId);
    if (page.isEmpty()) {
        unlinkDataPage(rid.pageId);
        disk.deallocatePage(rid.pageId);
    } else {
        disk.writePage(rid.pageId, page);
    }

    // Allocate / find room in another data page
    uint16_t reqSize = static_cast<uint16_t>(newBytes.size() + sizeof(Slot));
    uint32_t targetPageId = INVALID_PAGE_ID;
    int newSlotId = -1;

    uint32_t curr = disk.getFirstDataPageId();
    while (curr != INVALID_PAGE_ID) {
        Page p = disk.readPage(curr);
        if (p.getTotalFreeSpace() >= reqSize) {
            newSlotId = p.insertRecord(newBytes);
            if (newSlotId != -1) {
                targetPageId = curr;
                disk.writePage(curr, p);
                break;
            }
        }
        curr = p.getHeader().nextPageId;
    }

    if (targetPageId == INVALID_PAGE_ID) {
        targetPageId = disk.allocatePage(PAGE_TYPE_DATA);
        Page newPage(PAGE_TYPE_DATA);
        PageHeader h = newPage.getHeader();
        h.nextPageId = disk.getFirstDataPageId();
        newPage.setHeader(h);

        newSlotId = newPage.insertRecord(newBytes);
        disk.writePage(targetPageId, newPage);
        disk.setFirstDataPageId(targetPageId);
    }

    RID newRid{targetPageId, static_cast<uint16_t>(newSlotId)};
    index.update(key, newRid);
    return true;
}

bool Table::remove(int key) {
    RID rid = index.search(key);
    if (!rid.isValid()) {
        return false;
    }

    Page page = disk.readPage(rid.pageId);
    bool deleted = page.deleteRecord(rid.slotId);
    if (!deleted) {
        return false;
    }

    if (page.isEmpty()) {
        unlinkDataPage(rid.pageId);
        disk.deallocatePage(rid.pageId);
    } else {
        disk.writePage(rid.pageId, page);
    }

    index.remove(key);
    return true;
}

std::vector<Record> Table::rangeQuery(int low, int high) {
    std::vector<Record> records;
    auto pairs = index.rangeSearch(low, high);
    for (const auto& p : pairs) {
        Page page = disk.readPage(p.second.pageId);
        std::vector<char> bytes = page.getRecord(p.second.slotId);
        if (!bytes.empty()) {
            records.push_back(deserializeRecord(bytes));
        }
    }
    return records;
}

void Table::printIndexTree() {
    std::cout << "\n=== B+ Tree Index Hierarchy ===";
    index.printTree();
}

void Table::printTableStats() {
    std::cout << "\n================ Database & Storage Statistics ================\n";
    std::cout << "Total Pages in File: " << disk.getTotalPages() << " ("
              << (disk.getTotalPages() * PAGE_SIZE) / 1024 << " KB)\n";
    std::cout << "B+ Tree Root Page ID: "
              << (disk.getRootPageId() == INVALID_PAGE_ID ? "None" : std::to_string(disk.getRootPageId())) << "\n";

    // Trace Free Pages
    uint32_t freeCurr = disk.getFirstFreePageId();
    int freeCount = 0;
    std::vector<uint32_t> freeIds;
    while (freeCurr != INVALID_PAGE_ID && freeCount < 100) {
        freeIds.push_back(freeCurr);
        freeCount++;
        Page p = disk.readPage(freeCurr);
        freeCurr = p.getHeader().nextPageId;
    }
    std::cout << "Free / Recycled Pages (" << freeCount << "): [";
    for (size_t i = 0; i < freeIds.size(); ++i) {
        std::cout << freeIds[i] << (i + 1 < freeIds.size() ? ", " : "");
    }
    std::cout << "]\n";

    // Trace Data Pages
    uint32_t dataCurr = disk.getFirstDataPageId();
    int dataPageCount = 0;
    std::cout << "\n--- Data Pages (Slotted Pages & Fragmentation) ---\n";
    while (dataCurr != INVALID_PAGE_ID) {
        dataPageCount++;
        Page p = disk.readPage(dataCurr);
        PageHeader h = p.getHeader();
        uint16_t activeSlots = h.numSlots - h.deletedCount;

        std::cout << "  Page ID " << dataCurr << ": "
                  << activeSlots << " active records, "
                  << h.deletedCount << " deleted slots, "
                  << h.wastedBytes << " bytes wasted/fragmented, "
                  << p.getContiguousFreeSpace() << " bytes contiguous free, "
                  << p.getTotalFreeSpace() << " bytes total free\n";

        dataCurr = h.nextPageId;
    }
    if (dataPageCount == 0) {
        std::cout << "  (No data pages currently allocated)\n";
    }
    std::cout << "===============================================================\n\n";
}