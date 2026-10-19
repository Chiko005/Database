#include "diskmanager.h"

// ============================================================
// Page implementation  (unchanged from original)
// ============================================================

Page::Page() {
    memset(data, 0, PAGE_SIZE);
    PageHeader h{};
    h.pageType     = PAGE_TYPE_EMPTY;
    h.freeSpacePtr = PAGE_SIZE;
    h.nextPageId   = INVALID_PAGE_ID;
    h.prevPageId   = INVALID_PAGE_ID;
    setHeader(h);
}

Page::Page(PageType type) {
    memset(data, 0, PAGE_SIZE);
    PageHeader h{};
    h.pageType     = static_cast<uint16_t>(type);
    h.freeSpacePtr = PAGE_SIZE;
    h.nextPageId   = INVALID_PAGE_ID;
    h.prevPageId   = INVALID_PAGE_ID;
    setHeader(h);
}

PageHeader Page::getHeader() const {
    PageHeader h;
    memcpy(&h, data, sizeof(PageHeader));
    return h;
}
void Page::setHeader(const PageHeader& h) {
    memcpy(data, &h, sizeof(PageHeader));
}

Slot Page::getSlot(uint16_t slotId) const {
    Slot s{0, 0};
    size_t off = sizeof(PageHeader) + slotId * sizeof(Slot);
    if (off + sizeof(Slot) <= PAGE_SIZE) memcpy(&s, data + off, sizeof(Slot));
    return s;
}
void Page::setSlot(uint16_t slotId, const Slot& s) {
    size_t off = sizeof(PageHeader) + slotId * sizeof(Slot);
    if (off + sizeof(Slot) <= PAGE_SIZE) memcpy(data + off, &s, sizeof(Slot));
}

uint16_t Page::getContiguousFreeSpace() const {
    PageHeader h = getHeader();
    uint16_t slotDirEnd = sizeof(PageHeader) + h.numSlots * sizeof(Slot);
    if (h.freeSpacePtr < slotDirEnd) return 0;
    return h.freeSpacePtr - slotDirEnd;
}
uint16_t Page::getTotalFreeSpace() const {
    return getContiguousFreeSpace() + getHeader().wastedBytes;
}
bool Page::isEmpty() const {
    PageHeader h = getHeader();
    return h.numSlots == 0 || h.deletedCount == h.numSlots;
}

void Page::compact() {
    PageHeader h = getHeader();
    char temp[PAGE_SIZE];
    memset(temp, 0, PAGE_SIZE);
    uint16_t newFSP = PAGE_SIZE;

    for (uint16_t i = 0; i < h.numSlots; ++i) {
        Slot s = getSlot(i);
        if (s.size > 0 && s.offset >= h.freeSpacePtr && s.offset + s.size <= PAGE_SIZE) {
            newFSP -= s.size;
            memcpy(temp + newFSP, data + s.offset, s.size);
            Slot ns{newFSP, s.size};
            size_t off = sizeof(PageHeader) + i * sizeof(Slot);
            memcpy(temp + off, &ns, sizeof(Slot));
        } else {
            Slot empty{0, 0};
            size_t off = sizeof(PageHeader) + i * sizeof(Slot);
            memcpy(temp + off, &empty, sizeof(Slot));
        }
    }
    h.freeSpacePtr = newFSP;
    h.wastedBytes  = 0;
    memcpy(temp, &h, sizeof(PageHeader));
    memcpy(data, temp, PAGE_SIZE);
}

int Page::insertRecord(const std::vector<char>& record) {
    PageHeader h = getHeader();
    uint16_t sz  = static_cast<uint16_t>(record.size());

    bool     reuse   = false;
    uint16_t tgtSlot = h.numSlots;
    for (uint16_t i = 0; i < h.numSlots; ++i) {
        if (getSlot(i).size == 0) { reuse = true; tgtSlot = i; break; }
    }

    uint16_t overhead = reuse ? 0 : static_cast<uint16_t>(sizeof(Slot));
    uint16_t needed   = sz + overhead;

    if (getContiguousFreeSpace() < needed) {
        if (getTotalFreeSpace() >= needed) { compact(); h = getHeader(); }
        else return -1;
    }
    if (getContiguousFreeSpace() < needed) return -1;

    h.freeSpacePtr -= sz;
    memcpy(data + h.freeSpacePtr, record.data(), sz);

    Slot s{h.freeSpacePtr, sz};
    setSlot(tgtSlot, s);

    if (reuse) { if (h.deletedCount > 0) --h.deletedCount; }
    else        { ++h.numSlots; }
    setHeader(h);
    return tgtSlot;
}

std::vector<char> Page::getRecord(uint16_t slotId) const {
    PageHeader h = getHeader();
    if (slotId >= h.numSlots) return {};
    Slot s = getSlot(slotId);
    if (s.size == 0) return {};
    std::vector<char> r(s.size);
    memcpy(r.data(), data + s.offset, s.size);
    return r;
}

bool Page::deleteRecord(uint16_t slotId) {
    PageHeader h = getHeader();
    if (slotId >= h.numSlots) return false;
    Slot s = getSlot(slotId);
    if (s.size == 0) return false;
    h.wastedBytes += s.size;
    ++h.deletedCount;
    s.offset = 0; s.size = 0;
    setSlot(slotId, s);
    if (h.deletedCount == h.numSlots) {
        h.numSlots = 0; h.freeSpacePtr = PAGE_SIZE;
        h.deletedCount = 0; h.wastedBytes = 0;
    }
    setHeader(h);
    return true;
}

bool Page::updateRecord(uint16_t slotId, const std::vector<char>& newRec) {
    PageHeader h = getHeader();
    if (slotId >= h.numSlots) return false;
    Slot s = getSlot(slotId);
    if (s.size == 0) return false;

    uint16_t oldSz = s.size;
    uint16_t newSz = static_cast<uint16_t>(newRec.size());

    if (newSz <= oldSz) {
        memcpy(data + s.offset, newRec.data(), newSz);
        h.wastedBytes += (oldSz - newSz);
        s.size = newSz;
        setSlot(slotId, s);
        setHeader(h);
        return true;
    }

    uint16_t avail = getTotalFreeSpace() + oldSz;
    if (avail < newSz) return false;

    h.wastedBytes += oldSz;
    s.size = 0; s.offset = 0;
    setSlot(slotId, s);
    setHeader(h);

    if (getContiguousFreeSpace() < newSz) { compact(); h = getHeader(); }
    if (getContiguousFreeSpace() < newSz) return false;

    h.freeSpacePtr -= newSz;
    memcpy(data + h.freeSpacePtr, newRec.data(), newSz);
    s.offset = h.freeSpacePtr; s.size = newSz;
    setSlot(slotId, s);
    setHeader(h);
    return true;
}

// ============================================================
// DiskManager implementation
// ============================================================

DiskManager::DiskManager(const std::string& filename, int bufPoolCapacity)
    : dbFilename_(filename),
      walFilename_(filename + ".wal"),
      pool_(bufPoolCapacity),
      wal_(filename + ".wal")
{
    file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    bool isNew = false;

    if (!file_.is_open()) {
        file_.clear();
        file_.open(filename, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        isNew = true;
    } else {
        file_.seekg(0, std::ios::end);
        if (file_.tellg() < static_cast<std::streampos>(PAGE_SIZE)) isNew = true;
    }

    if (isNew) {
        dbHeader_.magic[0] = 'B'; dbHeader_.magic[1] = 'P';
        dbHeader_.magic[2] = 'T'; dbHeader_.magic[3] = 'D';
        dbHeader_.totalPages      = 1;
        dbHeader_.rootPageId      = INVALID_PAGE_ID;
        dbHeader_.firstFreePageId = INVALID_PAGE_ID;
        dbHeader_.firstDataPageId = INVALID_PAGE_ID;
        writeHeader();
    } else {
        // Replay WAL first (crash recovery)
        recoverFromWAL();
        readHeader();
        if (dbHeader_.magic[0] != 'B' || dbHeader_.magic[1] != 'P' ||
            dbHeader_.magic[2] != 'T' || dbHeader_.magic[3] != 'D') {
            dbHeader_ = {};
            dbHeader_.magic[0] = 'B'; dbHeader_.magic[1] = 'P';
            dbHeader_.magic[2] = 'T'; dbHeader_.magic[3] = 'D';
            dbHeader_.totalPages      = 1;
            dbHeader_.rootPageId      = INVALID_PAGE_ID;
            dbHeader_.firstFreePageId = INVALID_PAGE_ID;
            dbHeader_.firstDataPageId = INVALID_PAGE_ID;
            writeHeader();
        }
    }
}

DiskManager::~DiskManager() {
    flush();
    if (file_.is_open()) file_.close();
}

void DiskManager::recoverFromWAL() {
    int n = wal_.recover(file_);
    if (n > 0) {
        // WAL replay done; truncate WAL
        wal_.truncate();
    }
}

void DiskManager::readHeader() {
    // Read directly from disk (bypass pool — header must always be on disk after init)
    file_.seekg(0);
    char buf[PAGE_SIZE];
    file_.read(buf, PAGE_SIZE);
    memcpy(&dbHeader_, buf, sizeof(DBHeader));
}

void DiskManager::writeHeader() {
    Page p(PAGE_TYPE_HEADER);
    memcpy(p.getMutableData(), &dbHeader_, sizeof(DBHeader));
    // Write header page through pool + WAL
    writePage(0, p);
    file_.flush();
}

// ---------------------------------------------------------------------------
// readPage — check pool first, load from disk on miss
// ---------------------------------------------------------------------------
Page DiskManager::readPage(uint32_t pageId) {
    return pool_.fetchPage(pageId, file_);
}

// ---------------------------------------------------------------------------
// writePage — log to WAL, then store in pool (dirty, not yet on disk)
// ---------------------------------------------------------------------------
void DiskManager::writePage(uint32_t pageId, const Page& page) {
    wal_.logWrite(pageId, page.getData());
    pool_.storePage(pageId, page, file_);
}

// ---------------------------------------------------------------------------
// flush — flush pool to disk, then checkpoint WAL
// ---------------------------------------------------------------------------
void DiskManager::flush() {
    if (!file_.is_open()) return;
    pool_.flushAll(file_);
    wal_.logCheckpoint();
    wal_.flush();
    file_.flush();
}

// ---------------------------------------------------------------------------
// allocatePage / deallocatePage
// ---------------------------------------------------------------------------
uint32_t DiskManager::allocatePage(PageType type) {
    if (dbHeader_.firstFreePageId != INVALID_PAGE_ID) {
        uint32_t pid = dbHeader_.firstFreePageId;
        Page fp = readPage(pid);
        dbHeader_.firstFreePageId = fp.getHeader().nextPageId;
        writeHeader();
        Page np(type);
        writePage(pid, np);
        wal_.logCommit();
        return pid;
    }
    uint32_t pid = dbHeader_.totalPages++;
    writeHeader();
    Page np(type);
    writePage(pid, np);
    wal_.logCommit();
    return pid;
}

void DiskManager::deallocatePage(uint32_t pageId) {
    if (pageId == 0 || pageId >= dbHeader_.totalPages) return;
    Page fp(PAGE_TYPE_FREE);
    PageHeader h = fp.getHeader();
    h.nextPageId = dbHeader_.firstFreePageId;
    fp.setHeader(h);
    writePage(pageId, fp);
    pool_.invalidate(pageId);
    dbHeader_.firstFreePageId = pageId;
    writeHeader();
    wal_.logCommit();
}

void DiskManager::setRootPageId(uint32_t id) {
    dbHeader_.rootPageId = id;
    writeHeader();
    wal_.logCommit();
}

void DiskManager::setFirstDataPageId(uint32_t id) {
    dbHeader_.firstDataPageId = id;
    writeHeader();
    wal_.logCommit();
}