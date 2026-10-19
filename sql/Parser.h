#pragma once
#include "Token.h"
#include <string>
#include <vector>
#include <stdexcept>

// ---------------------------------------------------------------------------
// AST node types
// ---------------------------------------------------------------------------

enum class StmtType { SELECT, INSERT, UPDATE, DELETE };

/** A single WHERE predicate, e.g. "id = 5" or "id BETWEEN 10 AND 50" */
struct Condition {
    std::string column;
    std::string op;     // "=", ">", "<", ">=", "<=", "!=", "BETWEEN"
    std::string value;
    std::string value2; // only for BETWEEN
};

struct SelectStmt {
    std::string              table;
    bool                     selectAll = true;
    std::vector<std::string> columns;
    bool                     hasWhere = false;
    Condition                where;
};

struct InsertStmt {
    std::string              table;
    std::vector<std::string> values; // positional: id, name, age
};

struct SetClause {
    std::string column;
    std::string value;
};

struct UpdateStmt {
    std::string            table;
    std::vector<SetClause> setClauses;
    bool                   hasWhere = false;
    Condition              where;
};

struct DeleteStmt {
    std::string table;
    bool        hasWhere = false;
    Condition   where;
};

struct Statement {
    StmtType   type;
    SelectStmt select;
    InsertStmt insert;
    UpdateStmt update;
    DeleteStmt deleteStmt;
};

// ---------------------------------------------------------------------------
// Parse error exception
// ---------------------------------------------------------------------------

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

// ---------------------------------------------------------------------------
// Parser – recursive descent
// ---------------------------------------------------------------------------

class Parser {
private:
    std::vector<Token> tokens;
    size_t             pos = 0;

    Token& current();
    Token& peekAt(size_t offset = 1);
    Token  consume();
    Token  expect(TokenType type, const std::string& context = "");
    bool   check(TokenType type) const;
    bool   match(TokenType type);

    Statement parseSelect();
    Statement parseInsert();
    Statement parseUpdate();
    Statement parseDelete();
    Condition parseCondition();
    std::string parseValue(); // NUMBER | STRING | IDENTIFIER literal

public:
    explicit Parser(const std::vector<Token>& tokens);
    Statement parse();
};
