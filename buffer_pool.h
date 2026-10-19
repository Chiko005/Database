#pragma once
#include <unordered_map>
#include <list>
#include <vector>
#include <cstdint>
#include <fstream>

// Forward declarations to break the circular dependency with diskmanager.h
// (Page and INVALID_PAGE_ID are defined there; buffer_pool.h is #include'd
//  inside diskmanager.h after those definitions, so the forward decl lets
//  the Frame struct compile.)
class Page;
constexpr uint32_t BP_INVALID_PAGE_ID = 0xFFFFFFFF;

// ---------------------------------------------------------------------------
// LRU Buffer Pool
//
// Sits between DiskManager and raw disk I/O.
// Keeps up to `capacity` pages in memory; evicts the Least-Recently-Used
// dirty frame to disk when the pool is full.
// ---------------------------------------------------------------------------

class BufferPool {
public:
    static constexpr int DEFAULT_CAPACITY = 64;

private:
    struct Frame {
        Page*    page    = nullptr;  // heap-allocated to avoid incomplete-type issues
        uint32_t pageId  = BP_INVALID_PAGE_ID;
        bool     dirty   = false;
        bool     inUse   = false;
    };

    int                                             capacity_;
    std::vector<Frame>                              frames_;
    std::unordered_map<uint32_t, int>               pageTable_;  // pageId → frame index
    std::list<uint32_t>                             lruList_;    // front = MRU, back = LRU
    std::unordered_map<uint32_t,
        std::list<uint32_t>::iterator>              lruIter_;    // pageId → iterator in lruList_

    // Write frame's page to disk file at the correct offset
    void flushFrame(int fi, std::fstream& file);

    // Touch a page (move to front of LRU list)
    void touch(uint32_t pageId);

    // Evict the LRU dirty frame to disk; returns evicted frame index
    int evict(std::fstream& file);

    // Find a free frame (evicting if needed); returns frame index
    int acquireFrame(uint32_t newPageId, std::fstream& file);

public:
    explicit BufferPool(int capacity = DEFAULT_CAPACITY);
    ~BufferPool();

    // Fetch a page (load from disk on miss)
    Page  fetchPage(uint32_t pageId, std::fstream& file);

    // Store a page into the pool (marks dirty; NOT immediately written to disk)
    void  storePage(uint32_t pageId, const Page& page, std::fstream& file);

    // Flush all dirty frames to disk
    void  flushAll(std::fstream& file);

    // Remove page from pool (used when a page is deallocated)
    void  invalidate(uint32_t pageId);

    int   dirtyCount() const;
    int   cachedCount() const;
};
