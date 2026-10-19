#include "Executor.h"
#include <climits>
#include <stdexcept>
#include <chrono>
#include <sstream>

Executor::Executor(Table& t) : table(t) {}

// ---------------------------------------------------------------------------
// In-memory condition filter (used for non-id columns)
// ---------------------------------------------------------------------------

bool Executor::matchesCondition(const Record& rec, const Condition& cond) const {
    if (cond.column == "id") {
        int val = std::stoi(cond.value);
        if (cond.op == "=")       return rec.id == val;
        if (cond.op == "!=")      return rec.id != val;
        if (cond.op == ">")       return rec.id >  val;
        if (cond.op == ">=")      return rec.id >= val;
        if (cond.op == "<")       return rec.id <  val;
        if (cond.op == "<=")      return rec.id <= val;
        if (cond.op == "BETWEEN") {
            int lo = std::stoi(cond.value), hi = std::stoi(cond.value2);
            return rec.id >= lo && rec.id <= hi;
        }
    } else if (cond.column == "name") {
        if (cond.op == "=")  return rec.name == cond.value;
        if (cond.op == "!=") return rec.name != cond.value;
        if (cond.op == ">")  return rec.name >  cond.value;
        if (cond.op == "<")  return rec.name <  cond.value;
        if (cond.op == ">=") return rec.name >= cond.value;
        if (cond.op == "<=") return rec.name <= cond.value;
    } else if (cond.column == "age") {
        int val = std::stoi(cond.value);
        if (cond.op == "=")       return rec.age == val;
        if (cond.op == "!=")      return rec.age != val;
        if (cond.op == ">")       return rec.age >  val;
        if (cond.op == ">=")      return rec.age >= val;
        if (cond.op == "<")       return rec.age <  val;
        if (cond.op == "<=")      return rec.age <= val;
        if (cond.op == "BETWEEN") {
            int lo = std::stoi(cond.value), hi = std::stoi(cond.value2);
            return rec.age >= lo && rec.age <= hi;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// SELECT
// Strategy: use B+ Tree index for "id" predicates; full scan otherwise.
// ---------------------------------------------------------------------------

QueryResult Executor::executeSelect(const SelectStmt& stmt) {
    QueryResult result;
    result.isSelect = true;

    std::vector<Record> candidates;

    if (!stmt.hasWhere) {
        // Full table scan
        candidates = table.rangeQuery(INT_MIN, INT_MAX);
    } else if (stmt.where.column == "id") {
        const Condition& c = stmt.where;
        if (c.op == "=") {
            Record r;
            if (table.get(std::stoi(c.value), r)) candidates.push_back(r);
        } else if (c.op == "BETWEEN") {
            candidates = table.rangeQuery(std::stoi(c.value), std::stoi(c.value2));
        } else if (c.op == ">") {
            candidates = table.rangeQuery(std::stoi(c.value) + 1, INT_MAX);
        } else if (c.op == ">=") {
            candidates = table.rangeQuery(std::stoi(c.value), INT_MAX);
        } else if (c.op == "<") {
            candidates = table.rangeQuery(INT_MIN, std::stoi(c.value) - 1);
        } else if (c.op == "<=") {
            candidates = table.rangeQuery(INT_MIN, std::stoi(c.value));
        } else {
            // != : full scan + filter
            auto all = table.rangeQuery(INT_MIN, INT_MAX);
            for (auto& r : all)
                if (matchesCondition(r, c)) candidates.push_back(r);
        }
    } else {
        // Non-indexed column: full scan + in-memory filter
        auto all = table.rangeQuery(INT_MIN, INT_MAX);
        for (auto& r : all)
            if (matchesCondition(r, stmt.where)) candidates.push_back(r);
    }

    result.rows         = std::move(candidates);
    result.success      = true;
    result.affectedRows = static_cast<int>(result.rows.size());
    return result;
}

// ---------------------------------------------------------------------------
// INSERT
// ---------------------------------------------------------------------------

QueryResult Executor::executeInsert(const InsertStmt& stmt) {
    QueryResult result;

    if (stmt.values.size() < 3) {
        result.message = "INSERT expects exactly 3 values: (id, 'name', age)";
        return result;
    }

    try {
        Record r;
        r.id   = std::stoi(stmt.values[0]);
        r.name = stmt.values[1];
        r.age  = std::stoi(stmt.values[2]);

        if (table.insert(r)) {
            result.success      = true;
            result.affectedRows = 1;
            result.message      = "Query OK, 1 row affected";
        } else {
            result.message = "Duplicate key: a record with id=" +
                             stmt.values[0] + " already exists";
        }
    } catch (const std::exception& e) {
        result.message = std::string("Type error: ") + e.what();
    }

    return result;
}

// ---------------------------------------------------------------------------
// UPDATE
// ---------------------------------------------------------------------------

QueryResult Executor::executeUpdate(const UpdateStmt& stmt) {
    QueryResult result;

    // Gather candidates
    std::vector<Record> candidates;
    if (!stmt.hasWhere) {
        candidates = table.rangeQuery(INT_MIN, INT_MAX);
    } else if (stmt.where.column == "id" && stmt.where.op == "=") {
        Record r;
        if (table.get(std::stoi(stmt.where.value), r)) candidates.push_back(r);
    } else {
        auto all = table.rangeQuery(INT_MIN, INT_MAX);
        for (auto& r : all)
            if (matchesCondition(r, stmt.where)) candidates.push_back(r);
    }

    int updated = 0;
    for (auto& rec : candidates) {
        Record modified = rec;
        for (const auto& sc : stmt.setClauses) {
            if (sc.column == "name") {
                modified.name = sc.value;
            } else if (sc.column == "age") {
                try { modified.age = std::stoi(sc.value); }
                catch (...) { result.message = "Invalid age value: " + sc.value; return result; }
            } else if (sc.column == "id") {
                result.message = "Cannot update primary key 'id'";
                return result;
            } else {
                result.message = "Unknown column: '" + sc.column + "'";
                return result;
            }
        }
        if (table.update(rec.id, modified)) ++updated;
    }

    result.success      = true;
    result.affectedRows = updated;
    result.message      = "Query OK, " + std::to_string(updated) + " row(s) affected";
    return result;
}

// ---------------------------------------------------------------------------
// DELETE
// ---------------------------------------------------------------------------

QueryResult Executor::executeDelete(const DeleteStmt& stmt) {
    QueryResult result;

    std::vector<int> toDelete;

    if (!stmt.hasWhere) {
        auto all = table.rangeQuery(INT_MIN, INT_MAX);
        for (auto& r : all) toDelete.push_back(r.id);
    } else if (stmt.where.column == "id" && stmt.where.op == "=") {
        toDelete.push_back(std::stoi(stmt.where.value));
    } else {
        auto all = table.rangeQuery(INT_MIN, INT_MAX);
        for (auto& r : all)
            if (matchesCondition(r, stmt.where)) toDelete.push_back(r.id);
    }

    int deleted = 0;
    for (int id : toDelete)
        if (table.remove(id)) ++deleted;

    result.success      = true;
    result.affectedRows = deleted;
    result.message      = "Query OK, " + std::to_string(deleted) + " row(s) affected";
    return result;
}

// ---------------------------------------------------------------------------
// Dispatch + timing
// ---------------------------------------------------------------------------

QueryResult Executor::execute(const Statement& stmt) {
    using Clock = std::chrono::high_resolution_clock;
    auto start  = Clock::now();

    QueryResult result;
    switch (stmt.type) {
        case StmtType::SELECT: result = executeSelect(stmt.select);         break;
        case StmtType::INSERT: result = executeInsert(stmt.insert);         break;
        case StmtType::UPDATE: result = executeUpdate(stmt.update);         break;
        case StmtType::DELETE: result = executeDelete(stmt.deleteStmt);     break;
    }

    auto end       = Clock::now();
    result.timeMs  = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}
