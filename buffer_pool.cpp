#include "buffer_pool.h"
#include "diskmanager.h"   // Full definition of Page / PAGE_SIZE / INVALID_PAGE_ID
#include <stdexcept>
#include <cstring>
#include <new>

// ── Constructor / Destructor ──────────────────────────────────────────────────

BufferPool::BufferPool(int capacity) : capacity_(capacity) {
    frames_.resize(capacity_);
    for (auto& f : frames_) {
        f.page   = new Page();
        f.pageId = INVALID_PAGE_ID;
        f.dirty  = false;
        f.inUse  = false;
    }
}

BufferPool::~BufferPool() {
    for (auto& f : frames_) delete f.page;
}

// ── Private helpers ──────────────────────────────────────────────────────────

void BufferPool::flushFrame(int fi, std::fstream& file) {
    if (!frames_[fi].inUse || !frames_[fi].dirty) return;
    size_t offset = static_cast<size_t>(frames_[fi].pageId) * PAGE_SIZE;
    file.seekp(static_cast<std::streamoff>(offset));
    file.write(frames_[fi].page->getData(), PAGE_SIZE);
    file.flush();
    frames_[fi].dirty = false;
}

void BufferPool::touch(uint32_t pageId) {
    auto it = lruIter_.find(pageId);
    if (it != lruIter_.end()) {
        lruList_.erase(it->second);
    }
    lruList_.push_front(pageId);
    lruIter_[pageId] = lruList_.begin();
}

int BufferPool::evict(std::fstream& file) {
    for (auto it = lruList_.rbegin(); it != lruList_.rend(); ++it) {
        uint32_t pid = *it;
        int fi = pageTable_[pid];
        flushFrame(fi, file);
        lruList_.erase(lruIter_[pid]);
        lruIter_.erase(pid);
        pageTable_.erase(pid);
        frames_[fi].inUse  = false;
        frames_[fi].dirty  = false;
        frames_[fi].pageId = INVALID_PAGE_ID;
        return fi;
    }
    throw std::runtime_error("BufferPool::evict — pool is empty but full?");
}

int BufferPool::acquireFrame(uint32_t newPageId, std::fstream& file) {
    for (int i = 0; i < capacity_; ++i) {
        if (!frames_[i].inUse) return i;
    }
    return evict(file);
}

// ── Public API ───────────────────────────────────────────────────────────────

Page BufferPool::fetchPage(uint32_t pageId, std::fstream& file) {
    auto it = pageTable_.find(pageId);
    if (it != pageTable_.end()) {
        touch(pageId);
        return *frames_[it->second].page;
    }

    int fi = acquireFrame(pageId, file);

    size_t offset = static_cast<size_t>(pageId) * PAGE_SIZE;
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(frames_[fi].page->getMutableData(), PAGE_SIZE);

    frames_[fi].pageId = pageId;
    frames_[fi].dirty  = false;
    frames_[fi].inUse  = true;
    pageTable_[pageId] = fi;
    touch(pageId);

    return *frames_[fi].page;
}

void BufferPool::storePage(uint32_t pageId, const Page& page, std::fstream& file) {
    auto it = pageTable_.find(pageId);
    int fi;
    if (it != pageTable_.end()) {
        fi = it->second;
    } else {
        fi = acquireFrame(pageId, file);
        frames_[fi].pageId = pageId;
        frames_[fi].inUse  = true;
        pageTable_[pageId] = fi;
    }
    *frames_[fi].page = page;
    frames_[fi].dirty = true;
    touch(pageId);
}

void BufferPool::flushAll(std::fstream& file) {
    for (int i = 0; i < capacity_; ++i) {
        if (frames_[i].inUse && frames_[i].dirty) {
            flushFrame(i, file);
        }
    }
}

void BufferPool::invalidate(uint32_t pageId) {
    auto it = pageTable_.find(pageId);
    if (it == pageTable_.end()) return;
    int fi = it->second;
    if (lruIter_.count(pageId)) {
        lruList_.erase(lruIter_[pageId]);
        lruIter_.erase(pageId);
    }
    pageTable_.erase(it);
    frames_[fi].inUse  = false;
    frames_[fi].dirty  = false;
    frames_[fi].pageId = INVALID_PAGE_ID;
}

int BufferPool::dirtyCount() const {
    int n = 0;
    for (auto& f : frames_) if (f.inUse && f.dirty) ++n;
    return n;
}

int BufferPool::cachedCount() const {
    int n = 0;
    for (auto& f : frames_) if (f.inUse) ++n;
    return n;
}
