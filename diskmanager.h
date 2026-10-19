#ifndef DISKMANAGER_H
#define DISKMANAGER_H

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

#include "buffer_pool.h"
#include "wal.h"

constexpr uint16_t PAGE_SIZE = 4096;
constexpr uint32_t INVALID_PAGE_ID = 0xFFFFFFFF;

enum PageType : uint16_t {
    PAGE_TYPE_EMPTY    = 0,
    PAGE_TYPE_DATA     = 1,
    PAGE_TYPE_INDEX_LEAF     = 2,
    PAGE_TYPE_INDEX_INTERNAL = 3,
    PAGE_TYPE_FREE     = 4,
    PAGE_TYPE_HEADER   = 5
};

struct RID {
    uint32_t pageId;
    uint16_t slotId;

    bool isValid() const {
        return pageId != INVALID_PAGE_ID && slotId != 0xFFFF;
    }
    bool operator==(const RID& o) const {
        return pageId == o.pageId && slotId == o.slotId;
    }
};

struct Slot {
    uint16_t offset;
    uint16_t size;
};

struct PageHeader {
    uint16_t pageType;
    uint16_t numSlots;
    uint16_t freeSpacePtr;
    uint16_t deletedCount;
    uint16_t wastedBytes;
    uint32_t nextPageId;
    uint32_t prevPageId;
};

struct DBHeader {
    char     magic[4];           // "BPTD"
    uint32_t totalPages;
    uint32_t rootPageId;
    uint32_t firstFreePageId;
    uint32_t firstDataPageId;
};

// ============================================================
// Page
// ============================================================
class Page {
private:
    char data[PAGE_SIZE];
public:
    Page();
    explicit Page(PageType type);

    PageHeader getHeader() const;
    void       setHeader(const PageHeader& header);

    Slot  getSlot(uint16_t slotId) const;
    void  setSlot(uint16_t slotId, const Slot& slot);

    int                  insertRecord(const std::vector<char>& record);
    std::vector<char>    getRecord(uint16_t slotId) const;
    bool                 deleteRecord(uint16_t slotId);
    bool                 updateRecord(uint16_t slotId, const std::vector<char>& newRecord);

    void compact();

    uint16_t getContiguousFreeSpace() const;
    uint16_t getTotalFreeSpace() const;
    bool     isEmpty() const;

    const char* getData()        const { return data; }
    char*       getMutableData()       { return data; }
};

// ============================================================
// DiskManager  (owns BufferPool + WAL)
// ============================================================
class DiskManager {
private:
    std::string  dbFilename_;
    std::string  walFilename_;
    std::fstream file_;
    DBHeader     dbHeader_;
    BufferPool   pool_;
    WAL          wal_;

    void readHeader();
    void writeHeader();
    void recoverFromWAL();

public:
    explicit DiskManager(const std::string& filename,
                         int bufPoolCapacity = BufferPool::DEFAULT_CAPACITY);
    ~DiskManager();

    // Page I/O  (go through buffer pool + WAL)
    void writePage(uint32_t pageId, const Page& page);
    Page readPage(uint32_t pageId);

    // Page allocation / recycling
    uint32_t allocatePage(PageType type = PAGE_TYPE_DATA);
    void     deallocatePage(uint32_t pageId);

    // Metadata
    uint32_t getRootPageId()      const { return dbHeader_.rootPageId; }
    void     setRootPageId(uint32_t id);

    uint32_t getFirstDataPageId() const { return dbHeader_.firstDataPageId; }
    void     setFirstDataPageId(uint32_t id);

    uint32_t getTotalPages()      const { return dbHeader_.totalPages; }
    uint32_t getFirstFreePageId() const { return dbHeader_.firstFreePageId; }

    // Flush buffer pool → disk, then checkpoint WAL
    void flush();

    // Buffer pool / WAL stats for display
    int  cachedPages()  const { return pool_.cachedCount(); }
    int  dirtyPages()   const { return pool_.dirtyCount();  }
    uint64_t walLSN()   const { return wal_.currentLSN();   }
};

#endif // DISKMANAGER_H
