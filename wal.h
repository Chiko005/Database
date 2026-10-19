#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

// ---------------------------------------------------------------------------
// Write-Ahead Log  (REDO-only, single-writer, auto-commit per operation)
//
// Format on disk:
//   [WAL_MAGIC : 4 bytes  ]   "WAL1"
//   [nextLSN   : 8 bytes  ]   monotonically increasing log sequence number
//   Records (sequential):
//     [type     : 1 byte  ]   WALRecordType
//     [lsn      : 8 bytes ]
//     WRITE record:
//       [pageId : 4 bytes ]
//       [image  : PAGE_SIZE bytes]   full after-image of the page
//     COMMIT / CHECKPOINT:
//       (no extra data)
//
// Recovery (called in DiskManager constructor):
//   - Scan WAL sequentially
//   - Collect all WRITE records
//   - Apply them to the main DB file up to the last COMMIT entry
//   - Truncate WAL (checkpoint)
// ---------------------------------------------------------------------------

constexpr uint16_t WAL_PAGE_SIZE = 4096; // must match PAGE_SIZE in diskmanager.h

enum class WALRecordType : uint8_t {
    WRITE      = 1,
    COMMIT     = 2,
    CHECKPOINT = 3
};

class WAL {
public:
    explicit WAL(const std::string& walPath);
    ~WAL();

    // Log a full page after-image before it is applied to disk
    uint64_t logWrite(uint32_t pageId, const char* pageData);

    // Mark end of one logical operation (auto-commit)
    void logCommit();

    // Call after flushAll() — marks that all pages are safely on disk
    void logCheckpoint();

    // Replay all committed writes into the DB file on startup
    // Returns number of pages re-applied
    int recover(std::fstream& dbFile);

    // Truncate WAL to empty (keep magic + LSN header)
    void truncate();

    void  flush();
    bool  isEmpty();
    uint64_t currentLSN() const { return nextLSN_; }

private:
    std::string  walPath_;
    std::fstream file_;
    uint64_t     nextLSN_ = 1;

    static constexpr uint32_t MAGIC = 0x57414C31; // "WAL1"

    void openOrCreate();
    void writeHeader();
    void readHeader();
    uint64_t allocLSN() { return nextLSN_++; }
};
