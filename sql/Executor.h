#pragma once
#include "Parser.h"
#include "../table.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Result returned to the REPL
// ---------------------------------------------------------------------------

struct QueryResult {
    bool               success      = false;
    std::string        message;
    std::vector<Record> rows;
    int                affectedRows = 0;
    double             timeMs       = 0.0;
    bool               isSelect     = false;
};

// ---------------------------------------------------------------------------
// Executor – walks the AST and calls the Table API
// ---------------------------------------------------------------------------

class Executor {
private:
    Table& table;

    QueryResult executeSelect(const SelectStmt& stmt);
    QueryResult executeInsert(const InsertStmt& stmt);
    QueryResult executeUpdate(const UpdateStmt& stmt);
    QueryResult executeDelete(const DeleteStmt& stmt);

    /** In-memory filter for non-indexed columns */
    bool matchesCondition(const Record& rec, const Condition& cond) const;

public:
    explicit Executor(Table& t);
    QueryResult execute(const Statement& stmt);
};
