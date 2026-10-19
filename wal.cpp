#include "wal.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

// Header layout in the WAL file:
//   [0..3]   magic  (uint32_t)
//   [4..11]  nextLSN (uint64_t)

static constexpr size_t WAL_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint64_t);
static constexpr size_t WRITE_RECORD_SIZE =
    sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint32_t) + WAL_PAGE_SIZE; // type+lsn+pageId+image
static constexpr size_t SHORT_RECORD_SIZE =
    sizeof(uint8_t) + sizeof(uint64_t);                                      // type+lsn

WAL::WAL(const std::string& walPath) : walPath_(walPath) {
    openOrCreate();
}

WAL::~WAL() {
    flush();
    if (file_.is_open()) file_.close();
}

void WAL::openOrCreate() {
    file_.open(walPath_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        // Create new WAL file
        file_.clear();
        file_.open(walPath_, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(walPath_, std::ios::in | std::ios::out | std::ios::binary);
        nextLSN_ = 1;
        writeHeader();
    } else {
        file_.seekg(0, std::ios::end);
        auto sz = file_.tellg();
        if (sz < static_cast<std::streampos>(WAL_HEADER_SIZE)) {
            nextLSN_ = 1;
            writeHeader();
        } else {
            readHeader();
        }
    }
}

void WAL::writeHeader() {
    file_.seekp(0);
    uint32_t magic = MAGIC;
    file_.write(reinterpret_cast<const char*>(&magic),   sizeof(magic));
    file_.write(reinterpret_cast<const char*>(&nextLSN_), sizeof(nextLSN_));
    file_.flush();
}

void WAL::readHeader() {
    file_.seekg(0);
    uint32_t magic = 0;
    file_.read(reinterpret_cast<char*>(&magic),   sizeof(magic));
    file_.read(reinterpret_cast<char*>(&nextLSN_), sizeof(nextLSN_));
    if (magic != MAGIC) {
        // Corrupt or wrong file — start fresh
        nextLSN_ = 1;
        truncate();
    }
}

// ---------------------------------------------------------------------------
// Append records
// ---------------------------------------------------------------------------

uint64_t WAL::logWrite(uint32_t pageId, const char* pageData) {
    uint64_t lsn = allocLSN();
    file_.seekp(0, std::ios::end);
    uint8_t type = static_cast<uint8_t>(WALRecordType::WRITE);
    file_.write(reinterpret_cast<const char*>(&type),   sizeof(type));
    file_.write(reinterpret_cast<const char*>(&lsn),    sizeof(lsn));
    file_.write(reinterpret_cast<const char*>(&pageId), sizeof(pageId));
    file_.write(pageData, WAL_PAGE_SIZE);
    writeHeader();   // update nextLSN_ in header
    return lsn;
}

void WAL::logCommit() {
    uint64_t lsn = allocLSN();
    file_.seekp(0, std::ios::end);
    uint8_t type = static_cast<uint8_t>(WALRecordType::COMMIT);
    file_.write(reinterpret_cast<const char*>(&type), sizeof(type));
    file_.write(reinterpret_cast<const char*>(&lsn),  sizeof(lsn));
    writeHeader();
    file_.flush();
}

void WAL::logCheckpoint() {
    uint64_t lsn = allocLSN();
    file_.seekp(0, std::ios::end);
    uint8_t type = static_cast<uint8_t>(WALRecordType::CHECKPOINT);
    file_.write(reinterpret_cast<const char*>(&type), sizeof(type));
    file_.write(reinterpret_cast<const char*>(&lsn),  sizeof(lsn));
    writeHeader();
    file_.flush();
}

// ---------------------------------------------------------------------------
// Recovery — REDO all writes up to the last COMMIT
// ---------------------------------------------------------------------------

int WAL::recover(std::fstream& dbFile) {
    file_.seekg(0, std::ios::end);
    auto fileSize = file_.tellg();
    if (fileSize <= static_cast<std::streampos>(WAL_HEADER_SIZE)) return 0;

    file_.seekg(WAL_HEADER_SIZE); // skip header

    struct WriteEntry { uint32_t pageId; char image[WAL_PAGE_SIZE]; };
    std::vector<WriteEntry>  pending;   // collected since last committed batch
    std::vector<WriteEntry>  committed; // latest committed batch

    int applied = 0;

    while (true) {
        uint8_t  type = 0;
        uint64_t lsn  = 0;
        if (!file_.read(reinterpret_cast<char*>(&type), sizeof(type))) break;
        if (!file_.read(reinterpret_cast<char*>(&lsn),  sizeof(lsn)))  break;

        WALRecordType wtype = static_cast<WALRecordType>(type);

        if (wtype == WALRecordType::WRITE) {
            WriteEntry e;
            if (!file_.read(reinterpret_cast<char*>(&e.pageId), sizeof(e.pageId))) break;
            if (!file_.read(e.image, WAL_PAGE_SIZE)) break;
            pending.push_back(e);
        } else if (wtype == WALRecordType::COMMIT) {
            // Move pending writes to committed
            for (auto& e : pending) committed.push_back(e);
            pending.clear();
        } else if (wtype == WALRecordType::CHECKPOINT) {
            // Everything before this was already flushed to disk — clear
            committed.clear();
            pending.clear();
        }
    }

    // Apply committed writes to the DB file
    for (auto& e : committed) {
        size_t offset = static_cast<size_t>(e.pageId) * WAL_PAGE_SIZE;
        dbFile.seekp(static_cast<std::streamoff>(offset));
        dbFile.write(e.image, WAL_PAGE_SIZE);
        ++applied;
    }
    if (applied > 0) {
        dbFile.flush();
        std::cout << "[WAL] Recovery: applied " << applied << " page(s) from WAL.\n";
    }

    return applied;
}

// ---------------------------------------------------------------------------
// Truncate & flush
// ---------------------------------------------------------------------------

void WAL::truncate() {
    file_.close();
    // Reopen truncated to zero, then write fresh header
    file_.open(walPath_, std::ios::out | std::ios::binary | std::ios::trunc);
    file_.close();
    file_.open(walPath_, std::ios::in | std::ios::out | std::ios::binary);
    writeHeader();
}

void WAL::flush() {
    if (file_.is_open()) file_.flush();
}

bool WAL::isEmpty() {
    file_.seekg(0, std::ios::end);
    return file_.tellg() <= static_cast<std::streampos>(WAL_HEADER_SIZE);
}
