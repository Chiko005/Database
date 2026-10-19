#ifndef PERSISTENT_H
#define PERSISTENT_H

#include "diskmanager.h"
#include <vector>
#include <iostream>
#include <iomanip>

constexpr int MAX_LEAF_ENTRIES = 4;
constexpr int MIN_LEAF_ENTRIES = 2;
constexpr int MAX_INTERNAL_KEYS = 4;
constexpr int MIN_INTERNAL_KEYS = 2;

enum class BPTNodeType : uint8_t {
    LEAF = 1,
    INTERNAL = 2
};

struct LeafNode {
    uint32_t pageId = INVALID_PAGE_ID;
    uint32_t parentPageId = INVALID_PAGE_ID;
    uint32_t nextPageId = INVALID_PAGE_ID;
    uint32_t prevPageId = INVALID_PAGE_ID;
    std::vector<int> keys;
    std::vector<RID> records;

    Page toPage() const;
    static LeafNode fromPage(uint32_t pageId, const Page& page);
};

struct InternalNode {
    uint32_t pageId = INVALID_PAGE_ID;
    uint32_t parentPageId = INVALID_PAGE_ID;
    std::vector<int> keys;
    std::vector<uint32_t> children; // Child PageIDs

    Page toPage() const;
    static InternalNode fromPage(uint32_t pageId, const Page& page);
};

class BPlusTree {
private:
    DiskManager& disk;
    uint32_t rootPageId;

    uint32_t findLeaf(int key);
    void splitLeaf(LeafNode& leaf);
    void splitInternal(InternalNode& internal);
    void insertIntoInternal(uint32_t parentPageId, int key, uint32_t rightChildId);

    void handleLeafUnderflow(LeafNode& leaf);
    void handleInternalUnderflow(InternalNode& node);
    void updateChildParent(uint32_t childPageId, uint32_t newParentId);
    void shrinkRootTo(uint32_t newRootId, uint32_t oldRootId);

public:
    explicit BPlusTree(DiskManager& diskManager);

    // B+ Tree Index Operations
    RID search(int key);
    void insert(int key, RID rid);
    bool remove(int key);
    bool update(int key, RID newRid);
    std::vector<std::pair<int, RID>> rangeSearch(int low, int high);

    // Diagnostics & Visualization
    uint32_t getRootPageId() const { return rootPageId; }
    void printTree();
};

#endif // PERSISTENT_H
