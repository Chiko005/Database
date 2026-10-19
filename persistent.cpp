#include "persistent.h"
#include <queue>

// ---------------- LeafNode Methods ----------------

Page LeafNode::toPage() const {
    Page page(PAGE_TYPE_INDEX_LEAF);
    char* buf = page.getMutableData();
    size_t offset = 0;

    uint8_t type = static_cast<uint8_t>(BPTNodeType::LEAF);
    memcpy(buf + offset, &type, sizeof(type));
    offset += sizeof(type);

    uint16_t count = static_cast<uint16_t>(keys.size());
    memcpy(buf + offset, &count, sizeof(count));
    offset += sizeof(count);

    memcpy(buf + offset, &parentPageId, sizeof(parentPageId));
    offset += sizeof(parentPageId);

    memcpy(buf + offset, &nextPageId, sizeof(nextPageId));
    offset += sizeof(nextPageId);

    memcpy(buf + offset, &prevPageId, sizeof(prevPageId));
    offset += sizeof(prevPageId);

    for (size_t i = 0; i < keys.size(); ++i) {
        int k = keys[i];
        memcpy(buf + offset, &k, sizeof(k));
        offset += sizeof(k);

        uint32_t pid = records[i].pageId;
        memcpy(buf + offset, &pid, sizeof(pid));
        offset += sizeof(pid);

        uint16_t sid = records[i].slotId;
        memcpy(buf + offset, &sid, sizeof(sid));
        offset += sizeof(sid);
    }
    return page;
}

LeafNode LeafNode::fromPage(uint32_t pageId, const Page& page) {
    LeafNode leaf;
    leaf.pageId = pageId;
    const char* buf = page.getData();
    size_t offset = 0;

    uint8_t type;
    memcpy(&type, buf + offset, sizeof(type));
    offset += sizeof(type);

    uint16_t count;
    memcpy(&count, buf + offset, sizeof(count));
    offset += sizeof(count);

    memcpy(&leaf.parentPageId, buf + offset, sizeof(leaf.parentPageId));
    offset += sizeof(leaf.parentPageId);

    memcpy(&leaf.nextPageId, buf + offset, sizeof(leaf.nextPageId));
    offset += sizeof(leaf.nextPageId);

    memcpy(&leaf.prevPageId, buf + offset, sizeof(leaf.prevPageId));
    offset += sizeof(leaf.prevPageId);

    for (uint16_t i = 0; i < count; ++i) {
        int k;
        memcpy(&k, buf + offset, sizeof(k));
        offset += sizeof(k);

        RID rid;
        memcpy(&rid.pageId, buf + offset, sizeof(rid.pageId));
        offset += sizeof(rid.pageId);

        memcpy(&rid.slotId, buf + offset, sizeof(rid.slotId));
        offset += sizeof(rid.slotId);

        leaf.keys.push_back(k);
        leaf.records.push_back(rid);
    }
    return leaf;
}

// ---------------- InternalNode Methods ----------------

Page InternalNode::toPage() const {
    Page page(PAGE_TYPE_INDEX_INTERNAL);
    char* buf = page.getMutableData();
    size_t offset = 0;

    uint8_t type = static_cast<uint8_t>(BPTNodeType::INTERNAL);
    memcpy(buf + offset, &type, sizeof(type));
    offset += sizeof(type);

    uint16_t count = static_cast<uint16_t>(keys.size());
    memcpy(buf + offset, &count, sizeof(count));
    offset += sizeof(count);

    memcpy(buf + offset, &parentPageId, sizeof(parentPageId));
    offset += sizeof(parentPageId);

    for (size_t i = 0; i < keys.size(); ++i) {
        int k = keys[i];
        memcpy(buf + offset, &k, sizeof(k));
        offset += sizeof(k);
    }

    for (size_t i = 0; i < children.size(); ++i) {
        uint32_t cid = children[i];
        memcpy(buf + offset, &cid, sizeof(cid));
        offset += sizeof(cid);
    }
    return page;
}

InternalNode InternalNode::fromPage(uint32_t pageId, const Page& page) {
    InternalNode internal;
    internal.pageId = pageId;
    const char* buf = page.getData();
    size_t offset = 0;

    uint8_t type;
    memcpy(&type, buf + offset, sizeof(type));
    offset += sizeof(type);

    uint16_t count;
    memcpy(&count, buf + offset, sizeof(count));
    offset += sizeof(count);

    memcpy(&internal.parentPageId, buf + offset, sizeof(internal.parentPageId));
    offset += sizeof(internal.parentPageId);

    for (uint16_t i = 0; i < count; ++i) {
        int k;
        memcpy(&k, buf + offset, sizeof(k));
        offset += sizeof(k);
        internal.keys.push_back(k);
    }

    uint16_t childCount = (count > 0) ? static_cast<uint16_t>(count + 1) : 0;
    for (uint16_t i = 0; i < childCount; ++i) {
        uint32_t cid;
        memcpy(&cid, buf + offset, sizeof(cid));
        offset += sizeof(cid);
        internal.children.push_back(cid);
    }
    return internal;
}

// ---------------- BPlusTree Implementation ----------------

BPlusTree::BPlusTree(DiskManager& diskManager)
    : disk(diskManager), rootPageId(diskManager.getRootPageId()) {}

void BPlusTree::updateChildParent(uint32_t childPageId, uint32_t newParentId) {
    if (childPageId == INVALID_PAGE_ID) return;
    Page page = disk.readPage(childPageId);
    BPTNodeType type = static_cast<BPTNodeType>(page.getData()[0]);
    if (type == BPTNodeType::LEAF) {
        LeafNode leaf = LeafNode::fromPage(childPageId, page);
        leaf.parentPageId = newParentId;
        disk.writePage(childPageId, leaf.toPage());
    } else {
        InternalNode internal = InternalNode::fromPage(childPageId, page);
        internal.parentPageId = newParentId;
        disk.writePage(childPageId, internal.toPage());
    }
}

void BPlusTree::shrinkRootTo(uint32_t newRootId, uint32_t oldRootId) {
    rootPageId = newRootId;
    disk.setRootPageId(rootPageId);
    updateChildParent(newRootId, INVALID_PAGE_ID);
    disk.deallocatePage(oldRootId);
}

uint32_t BPlusTree::findLeaf(int key) {
    if (rootPageId == INVALID_PAGE_ID) {
        return INVALID_PAGE_ID;
    }

    uint32_t currPageId = rootPageId;
    while (true) {
        Page page = disk.readPage(currPageId);
        BPTNodeType type = static_cast<BPTNodeType>(page.getData()[0]);
        if (type == BPTNodeType::LEAF) {
            return currPageId;
        }

        InternalNode internal = InternalNode::fromPage(currPageId, page);
        size_t i = 0;
        while (i < internal.keys.size() && key >= internal.keys[i]) {
            i++;
        }
        if (i < internal.children.size()) {
            currPageId = internal.children[i];
        } else {
            currPageId = internal.children.back();
        }
    }
}

RID BPlusTree::search(int key) {
    uint32_t leafId = findLeaf(key);
    if (leafId == INVALID_PAGE_ID) {
        return {INVALID_PAGE_ID, 0xFFFF};
    }

    LeafNode leaf = LeafNode::fromPage(leafId, disk.readPage(leafId));
    for (size_t i = 0; i < leaf.keys.size(); ++i) {
        if (leaf.keys[i] == key) {
            return leaf.records[i];
        }
    }
    return {INVALID_PAGE_ID, 0xFFFF};
}

bool BPlusTree::update(int key, RID newRid) {
    uint32_t leafId = findLeaf(key);
    if (leafId == INVALID_PAGE_ID) {
        return false;
    }

    LeafNode leaf = LeafNode::fromPage(leafId, disk.readPage(leafId));
    for (size_t i = 0; i < leaf.keys.size(); ++i) {
        if (leaf.keys[i] == key) {
            leaf.records[i] = newRid;
            disk.writePage(leafId, leaf.toPage());
            return true;
        }
    }
    return false;
}

void BPlusTree::insert(int key, RID rid) {
    if (rootPageId == INVALID_PAGE_ID) {
        // Tree is empty; allocate first leaf as root
        uint32_t newLeafId = disk.allocatePage(PAGE_TYPE_INDEX_LEAF);
        LeafNode leaf;
        leaf.pageId = newLeafId;
        leaf.keys.push_back(key);
        leaf.records.push_back(rid);

        disk.writePage(newLeafId, leaf.toPage());
        rootPageId = newLeafId;
        disk.setRootPageId(rootPageId);
        return;
    }

    uint32_t leafId = findLeaf(key);
    LeafNode leaf = LeafNode::fromPage(leafId, disk.readPage(leafId));

    // Find sorted position
    size_t insertIdx = 0;
    while (insertIdx < leaf.keys.size() && leaf.keys[insertIdx] < key) {
        insertIdx++;
    }

    if (insertIdx < leaf.keys.size() && leaf.keys[insertIdx] == key) {
        // Key already exists; update its RID
        leaf.records[insertIdx] = rid;
        disk.writePage(leafId, leaf.toPage());
        return;
    }

    leaf.keys.insert(leaf.keys.begin() + insertIdx, key);
    leaf.records.insert(leaf.records.begin() + insertIdx, rid);
    disk.writePage(leafId, leaf.toPage());

    if (static_cast<int>(leaf.keys.size()) > MAX_LEAF_ENTRIES) {
        splitLeaf(leaf);
    }
}

void BPlusTree::splitLeaf(LeafNode& leaf) {
    uint32_t newLeafId = disk.allocatePage(PAGE_TYPE_INDEX_LEAF);
    LeafNode newLeaf;
    newLeaf.pageId = newLeafId;
    newLeaf.parentPageId = leaf.parentPageId;
    newLeaf.nextPageId = leaf.nextPageId;
    newLeaf.prevPageId = leaf.pageId;

    if (leaf.nextPageId != INVALID_PAGE_ID) {
        Page oldNextPage = disk.readPage(leaf.nextPageId);
        LeafNode oldNextLeaf = LeafNode::fromPage(leaf.nextPageId, oldNextPage);
        oldNextLeaf.prevPageId = newLeafId;
        disk.writePage(leaf.nextPageId, oldNextLeaf.toPage());
    }
    leaf.nextPageId = newLeafId;

    size_t mid = leaf.keys.size() / 2;
    for (size_t i = mid; i < leaf.keys.size(); ++i) {
        newLeaf.keys.push_back(leaf.keys[i]);
        newLeaf.records.push_back(leaf.records[i]);
    }
    leaf.keys.resize(mid);
    leaf.records.resize(mid);

    disk.writePage(leaf.pageId, leaf.toPage());
    disk.writePage(newLeaf.pageId, newLeaf.toPage());

    int promotedKey = newLeaf.keys[0];

    if (leaf.parentPageId == INVALID_PAGE_ID) {
        // Leaf was root; allocate new internal node root
        uint32_t newRootId = disk.allocatePage(PAGE_TYPE_INDEX_INTERNAL);
        InternalNode newRoot;
        newRoot.pageId = newRootId;
        newRoot.parentPageId = INVALID_PAGE_ID;
        newRoot.keys.push_back(promotedKey);
        newRoot.children.push_back(leaf.pageId);
        newRoot.children.push_back(newLeaf.pageId);

        leaf.parentPageId = newRootId;
        newLeaf.parentPageId = newRootId;

        disk.writePage(leaf.pageId, leaf.toPage());
        disk.writePage(newLeaf.pageId, newLeaf.toPage());
        disk.writePage(newRootId, newRoot.toPage());

        rootPageId = newRootId;
        disk.setRootPageId(rootPageId);
    } else {
        insertIntoInternal(leaf.parentPageId, promotedKey, newLeaf.pageId);
    }
}

void BPlusTree::insertIntoInternal(uint32_t parentPageId, int key, uint32_t rightChildId) {
    InternalNode parent = InternalNode::fromPage(parentPageId, disk.readPage(parentPageId));

    size_t insertIdx = 0;
    while (insertIdx < parent.keys.size() && parent.keys[insertIdx] < key) {
        insertIdx++;
    }

    parent.keys.insert(parent.keys.begin() + insertIdx, key);
    parent.children.insert(parent.children.begin() + insertIdx + 1, rightChildId);

    updateChildParent(rightChildId, parentPageId);
    disk.writePage(parentPageId, parent.toPage());

    if (static_cast<int>(parent.keys.size()) > MAX_INTERNAL_KEYS) {
        splitInternal(parent);
    }
}

void BPlusTree::splitInternal(InternalNode& internal) {
    uint32_t newInternalId = disk.allocatePage(PAGE_TYPE_INDEX_INTERNAL);
    InternalNode newInternal;
    newInternal.pageId = newInternalId;
    newInternal.parentPageId = internal.parentPageId;

    size_t mid = internal.keys.size() / 2;
    int promotedKey = internal.keys[mid];

    for (size_t i = mid + 1; i < internal.keys.size(); ++i) {
        newInternal.keys.push_back(internal.keys[i]);
    }
    for (size_t i = mid + 1; i < internal.children.size(); ++i) {
        newInternal.children.push_back(internal.children[i]);
        updateChildParent(internal.children[i], newInternalId);
    }

    internal.keys.resize(mid);
    internal.children.resize(mid + 1);

    disk.writePage(internal.pageId, internal.toPage());
    disk.writePage(newInternal.pageId, newInternal.toPage());

    if (internal.parentPageId == INVALID_PAGE_ID) {
        // Internal was root; allocate new root
        uint32_t newRootId = disk.allocatePage(PAGE_TYPE_INDEX_INTERNAL);
        InternalNode newRoot;
        newRoot.pageId = newRootId;
        newRoot.parentPageId = INVALID_PAGE_ID;
        newRoot.keys.push_back(promotedKey);
        newRoot.children.push_back(internal.pageId);
        newRoot.children.push_back(newInternal.pageId);

        internal.parentPageId = newRootId;
        newInternal.parentPageId = newRootId;

        disk.writePage(internal.pageId, internal.toPage());
        disk.writePage(newInternal.pageId, newInternal.toPage());
        disk.writePage(newRootId, newRoot.toPage());

        rootPageId = newRootId;
        disk.setRootPageId(rootPageId);
    } else {
        insertIntoInternal(internal.parentPageId, promotedKey, newInternal.pageId);
    }
}

std::vector<std::pair<int, RID>> BPlusTree::rangeSearch(int low, int high) {
    std::vector<std::pair<int, RID>> results;
    if (low > high || rootPageId == INVALID_PAGE_ID) {
        return results;
    }

    uint32_t currLeafId = findLeaf(low);
    while (currLeafId != INVALID_PAGE_ID) {
        LeafNode leaf = LeafNode::fromPage(currLeafId, disk.readPage(currLeafId));
        for (size_t i = 0; i < leaf.keys.size(); ++i) {
            if (leaf.keys[i] > high) {
                return results;
            }
            if (leaf.keys[i] >= low) {
                results.push_back({leaf.keys[i], leaf.records[i]});
            }
        }
        currLeafId = leaf.nextPageId;
    }
    return results;
}

bool BPlusTree::remove(int key) {
    uint32_t leafId = findLeaf(key);
    if (leafId == INVALID_PAGE_ID) {
        return false;
    }

    LeafNode leaf = LeafNode::fromPage(leafId, disk.readPage(leafId));
    int keyIdx = -1;
    for (size_t i = 0; i < leaf.keys.size(); ++i) {
        if (leaf.keys[i] == key) {
            keyIdx = static_cast<int>(i);
            break;
        }
    }

    if (keyIdx == -1) {
        return false;
    }

    leaf.keys.erase(leaf.keys.begin() + keyIdx);
    leaf.records.erase(leaf.records.begin() + keyIdx);
    disk.writePage(leafId, leaf.toPage());

    if (leaf.pageId == rootPageId) {
        if (leaf.keys.empty()) {
            disk.deallocatePage(leaf.pageId);
            rootPageId = INVALID_PAGE_ID;
            disk.setRootPageId(rootPageId);
        }
        return true;
    }

    if (static_cast<int>(leaf.keys.size()) < MIN_LEAF_ENTRIES) {
        handleLeafUnderflow(leaf);
    }
    return true;
}

void BPlusTree::handleLeafUnderflow(LeafNode& leaf) {
    InternalNode parent = InternalNode::fromPage(leaf.parentPageId, disk.readPage(leaf.parentPageId));
    size_t idx = 0;
    while (idx < parent.children.size() && parent.children[idx] != leaf.pageId) {
        idx++;
    }

    // Try borrowing from left sibling
    if (idx > 0) {
        uint32_t leftId = parent.children[idx - 1];
        LeafNode leftSibling = LeafNode::fromPage(leftId, disk.readPage(leftId));
        if (static_cast<int>(leftSibling.keys.size()) > MIN_LEAF_ENTRIES) {
            leaf.keys.insert(leaf.keys.begin(), leftSibling.keys.back());
            leftSibling.keys.pop_back();
            leaf.records.insert(leaf.records.begin(), leftSibling.records.back());
            leftSibling.records.pop_back();

            parent.keys[idx - 1] = leaf.keys[0];

            disk.writePage(leaf.pageId, leaf.toPage());
            disk.writePage(leftSibling.pageId, leftSibling.toPage());
            disk.writePage(parent.pageId, parent.toPage());
            return;
        }
    }

    // Try borrowing from right sibling
    if (idx + 1 < parent.children.size()) {
        uint32_t rightId = parent.children[idx + 1];
        LeafNode rightSibling = LeafNode::fromPage(rightId, disk.readPage(rightId));
        if (static_cast<int>(rightSibling.keys.size()) > MIN_LEAF_ENTRIES) {
            leaf.keys.push_back(rightSibling.keys.front());
            rightSibling.keys.erase(rightSibling.keys.begin());
            leaf.records.push_back(rightSibling.records.front());
            rightSibling.records.erase(rightSibling.records.begin());

            parent.keys[idx] = rightSibling.keys[0];

            disk.writePage(leaf.pageId, leaf.toPage());
            disk.writePage(rightSibling.pageId, rightSibling.toPage());
            disk.writePage(parent.pageId, parent.toPage());
            return;
        }
    }

    // Merge required
    if (idx > 0) {
        // Merge with left sibling
        uint32_t leftId = parent.children[idx - 1];
        LeafNode leftSibling = LeafNode::fromPage(leftId, disk.readPage(leftId));

        for (size_t i = 0; i < leaf.keys.size(); ++i) {
            leftSibling.keys.push_back(leaf.keys[i]);
            leftSibling.records.push_back(leaf.records[i]);
        }
        leftSibling.nextPageId = leaf.nextPageId;
        if (leaf.nextPageId != INVALID_PAGE_ID) {
            LeafNode nextLeaf = LeafNode::fromPage(leaf.nextPageId, disk.readPage(leaf.nextPageId));
            nextLeaf.prevPageId = leftSibling.pageId;
            disk.writePage(leaf.nextPageId, nextLeaf.toPage());
        }

        disk.writePage(leftSibling.pageId, leftSibling.toPage());
        disk.deallocatePage(leaf.pageId);

        parent.keys.erase(parent.keys.begin() + idx - 1);
        parent.children.erase(parent.children.begin() + idx);
        disk.writePage(parent.pageId, parent.toPage());

        if (parent.pageId == rootPageId) {
            if (parent.keys.empty()) {
                shrinkRootTo(leftSibling.pageId, parent.pageId);
            }
        } else if (static_cast<int>(parent.keys.size()) < MIN_INTERNAL_KEYS) {
            handleInternalUnderflow(parent);
        }
    } else {
        // Merge with right sibling
        uint32_t rightId = parent.children[idx + 1];
        LeafNode rightSibling = LeafNode::fromPage(rightId, disk.readPage(rightId));

        for (size_t i = 0; i < rightSibling.keys.size(); ++i) {
            leaf.keys.push_back(rightSibling.keys[i]);
            leaf.records.push_back(rightSibling.records[i]);
        }
        leaf.nextPageId = rightSibling.nextPageId;
        if (rightSibling.nextPageId != INVALID_PAGE_ID) {
            LeafNode nextLeaf = LeafNode::fromPage(rightSibling.nextPageId, disk.readPage(rightSibling.nextPageId));
            nextLeaf.prevPageId = leaf.pageId;
            disk.writePage(rightSibling.nextPageId, nextLeaf.toPage());
        }

        disk.writePage(leaf.pageId, leaf.toPage());
        disk.deallocatePage(rightSibling.pageId);

        parent.keys.erase(parent.keys.begin() + idx);
        parent.children.erase(parent.children.begin() + idx + 1);
        disk.writePage(parent.pageId, parent.toPage());

        if (parent.pageId == rootPageId) {
            if (parent.keys.empty()) {
                shrinkRootTo(leaf.pageId, parent.pageId);
            }
        } else if (static_cast<int>(parent.keys.size()) < MIN_INTERNAL_KEYS) {
            handleInternalUnderflow(parent);
        }
    }
}

void BPlusTree::handleInternalUnderflow(InternalNode& node) {
    if (node.pageId == rootPageId) {
        if (node.keys.empty() && !node.children.empty()) {
            shrinkRootTo(node.children[0], node.pageId);
        }
        return;
    }

    InternalNode parent = InternalNode::fromPage(node.parentPageId, disk.readPage(node.parentPageId));
    size_t idx = 0;
    while (idx < parent.children.size() && parent.children[idx] != node.pageId) {
        idx++;
    }

    // Try borrowing from left sibling
    if (idx > 0) {
        uint32_t leftId = parent.children[idx - 1];
        InternalNode leftSibling = InternalNode::fromPage(leftId, disk.readPage(leftId));
        if (static_cast<int>(leftSibling.keys.size()) > MIN_INTERNAL_KEYS) {
            uint32_t borrowedChild = leftSibling.children.back();
            leftSibling.children.pop_back();
            int borrowedKey = leftSibling.keys.back();
            leftSibling.keys.pop_back();

            int parentKey = parent.keys[idx - 1];
            parent.keys[idx - 1] = borrowedKey;

            node.keys.insert(node.keys.begin(), parentKey);
            node.children.insert(node.children.begin(), borrowedChild);
            updateChildParent(borrowedChild, node.pageId);

            disk.writePage(node.pageId, node.toPage());
            disk.writePage(leftSibling.pageId, leftSibling.toPage());
            disk.writePage(parent.pageId, parent.toPage());
            return;
        }
    }

    // Try borrowing from right sibling
    if (idx + 1 < parent.children.size()) {
        uint32_t rightId = parent.children[idx + 1];
        InternalNode rightSibling = InternalNode::fromPage(rightId, disk.readPage(rightId));
        if (static_cast<int>(rightSibling.keys.size()) > MIN_INTERNAL_KEYS) {
            uint32_t borrowedChild = rightSibling.children.front();
            rightSibling.children.erase(rightSibling.children.begin());
            int borrowedKey = rightSibling.keys.front();
            rightSibling.keys.erase(rightSibling.keys.begin());

            int parentKey = parent.keys[idx];
            parent.keys[idx] = borrowedKey;

            node.keys.push_back(parentKey);
            node.children.push_back(borrowedChild);
            updateChildParent(borrowedChild, node.pageId);

            disk.writePage(node.pageId, node.toPage());
            disk.writePage(rightSibling.pageId, rightSibling.toPage());
            disk.writePage(parent.pageId, parent.toPage());
            return;
        }
    }

    // Merge internal nodes
    if (idx > 0) {
        // Merge with left sibling
        uint32_t leftId = parent.children[idx - 1];
        InternalNode leftSibling = InternalNode::fromPage(leftId, disk.readPage(leftId));

        int parentKey = parent.keys[idx - 1];
        leftSibling.keys.push_back(parentKey);
        for (int k : node.keys) leftSibling.keys.push_back(k);
        for (uint32_t c : node.children) {
            leftSibling.children.push_back(c);
            updateChildParent(c, leftSibling.pageId);
        }

        disk.writePage(leftSibling.pageId, leftSibling.toPage());
        disk.deallocatePage(node.pageId);

        parent.keys.erase(parent.keys.begin() + idx - 1);
        parent.children.erase(parent.children.begin() + idx);
        disk.writePage(parent.pageId, parent.toPage());

        if (parent.pageId == rootPageId) {
            if (parent.keys.empty()) {
                shrinkRootTo(leftSibling.pageId, parent.pageId);
            }
        } else if (static_cast<int>(parent.keys.size()) < MIN_INTERNAL_KEYS) {
            handleInternalUnderflow(parent);
        }
    } else {
        // Merge with right sibling
        uint32_t rightId = parent.children[idx + 1];
        InternalNode rightSibling = InternalNode::fromPage(rightId, disk.readPage(rightId));

        int parentKey = parent.keys[idx];
        node.keys.push_back(parentKey);
        for (int k : rightSibling.keys) node.keys.push_back(k);
        for (uint32_t c : rightSibling.children) {
            node.children.push_back(c);
            updateChildParent(c, node.pageId);
        }

        disk.writePage(node.pageId, node.toPage());
        disk.deallocatePage(rightSibling.pageId);

        parent.keys.erase(parent.keys.begin() + idx);
        parent.children.erase(parent.children.begin() + idx + 1);
        disk.writePage(parent.pageId, parent.toPage());

        if (parent.pageId == rootPageId) {
            if (parent.keys.empty()) {
                shrinkRootTo(node.pageId, parent.pageId);
            }
        } else if (static_cast<int>(parent.keys.size()) < MIN_INTERNAL_KEYS) {
            handleInternalUnderflow(parent);
        }
    }
}

void BPlusTree::printTree() {
    if (rootPageId == INVALID_PAGE_ID) {
        std::cout << "  (Tree is empty)\n";
        return;
    }

    std::queue<std::pair<uint32_t, int>> q;
    q.push({rootPageId, 0});
    int currentLevel = -1;

    while (!q.empty()) {
        auto item = q.front();
        q.pop();
        uint32_t pid = item.first;
        int level = item.second;

        if (level != currentLevel) {
            currentLevel = level;
            std::cout << "\n--- Level " << currentLevel << " ---\n";
        }

        Page page = disk.readPage(pid);
        BPTNodeType type = static_cast<BPTNodeType>(page.getData()[0]);

        if (type == BPTNodeType::LEAF) {
            LeafNode leaf = LeafNode::fromPage(pid, page);
            std::cout << "  [Leaf Page " << pid << " | Parent: "
                      << (leaf.parentPageId == INVALID_PAGE_ID ? "None" : std::to_string(leaf.parentPageId))
                      << " | Next: " << (leaf.nextPageId == INVALID_PAGE_ID ? "None" : std::to_string(leaf.nextPageId))
                      << "] Keys: [";
            for (size_t i = 0; i < leaf.keys.size(); ++i) {
                std::cout << leaf.keys[i] << "(P:" << leaf.records[i].pageId << ",S:" << leaf.records[i].slotId << ")";
                if (i + 1 < leaf.keys.size()) std::cout << ", ";
            }
            std::cout << "]\n";
        } else {
            InternalNode internal = InternalNode::fromPage(pid, page);
            std::cout << "  [Internal Page " << pid << " | Parent: "
                      << (internal.parentPageId == INVALID_PAGE_ID ? "None" : std::to_string(internal.parentPageId))
                      << "] Keys: [";
            for (size_t i = 0; i < internal.keys.size(); ++i) {
                std::cout << internal.keys[i];
                if (i + 1 < internal.keys.size()) std::cout << ", ";
            }
            std::cout << "] Children: [";
            for (size_t i = 0; i < internal.children.size(); ++i) {
                std::cout << internal.children[i];
                if (i + 1 < internal.children.size()) std::cout << ", ";
                q.push({internal.children[i], level + 1});
            }
            std::cout << "]\n";
        }
    }
}