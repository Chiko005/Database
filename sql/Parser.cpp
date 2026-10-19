#include "Parser.h"
#include <sstream>

Parser::Parser(const std::vector<Token>& toks) : tokens(toks), pos(0) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Token& Parser::current() {
    return tokens[pos];
}

Token& Parser::peekAt(size_t offset) {
    size_t idx = pos + offset;
    if (idx >= tokens.size()) return tokens.back();
    return tokens[idx];
}

Token Parser::consume() {
    Token t = tokens[pos];
    if (pos + 1 < tokens.size()) ++pos;
    return t;
}

Token Parser::expect(TokenType type, const std::string& context) {
    if (current().type != type) {
        std::ostringstream oss;
        oss << "Syntax error";
        if (!context.empty()) oss << " near " << context;
        oss << ": unexpected token '" << current().value << "'";
        throw ParseError(oss.str());
    }
    return consume();
}

bool Parser::check(TokenType type) const {
    return tokens[pos].type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { consume(); return true; }
    return false;
}

// Parse a literal value: NUMBER, STRING, or bare IDENTIFIER used as value
std::string Parser::parseValue() {
    if (check(TokenType::NUMBER) || check(TokenType::STRING) || check(TokenType::IDENTIFIER)) {
        return consume().value;
    }
    throw ParseError("Expected a value (number or string), got '" + current().value + "'");
}

// ---------------------------------------------------------------------------
// WHERE condition
// ---------------------------------------------------------------------------

Condition Parser::parseCondition() {
    Condition cond;
    cond.column = expect(TokenType::IDENTIFIER, "WHERE clause").value;

    if      (check(TokenType::EQUAL))         { consume(); cond.op = "="; }
    else if (check(TokenType::NOT_EQUAL))     { consume(); cond.op = "!="; }
    else if (check(TokenType::GREATER_EQUAL)) { consume(); cond.op = ">="; }
    else if (check(TokenType::LESS_EQUAL))    { consume(); cond.op = "<="; }
    else if (check(TokenType::GREATER))       { consume(); cond.op = ">"; }
    else if (check(TokenType::LESS))          { consume(); cond.op = "<"; }
    else if (check(TokenType::BETWEEN)) {
        consume();
        cond.op     = "BETWEEN";
        cond.value  = parseValue();
        expect(TokenType::AND, "BETWEEN…AND");
        cond.value2 = parseValue();
        return cond;
    }
    else {
        throw ParseError("Expected comparison operator, got '" + current().value + "'");
    }

    cond.value = parseValue();
    return cond;
}

// ---------------------------------------------------------------------------
// Statement parsers
// ---------------------------------------------------------------------------

Statement Parser::parseSelect() {
    Statement stmt;
    stmt.type = StmtType::SELECT;
    SelectStmt& sel = stmt.select;

    if (match(TokenType::STAR)) {
        sel.selectAll = true;
    } else {
        sel.selectAll = false;
        sel.columns.push_back(expect(TokenType::IDENTIFIER, "SELECT list").value);
        while (match(TokenType::COMMA))
            sel.columns.push_back(expect(TokenType::IDENTIFIER, "SELECT list").value);
    }

    expect(TokenType::FROM, "SELECT statement");
    sel.table = expect(TokenType::IDENTIFIER, "FROM clause").value;

    if (match(TokenType::WHERE)) {
        sel.hasWhere = true;
        sel.where    = parseCondition();
    }

    match(TokenType::SEMICOLON);
    return stmt;
}

Statement Parser::parseInsert() {
    Statement stmt;
    stmt.type = StmtType::INSERT;
    InsertStmt& ins = stmt.insert;

    expect(TokenType::INTO,   "INSERT statement");
    ins.table = expect(TokenType::IDENTIFIER, "INSERT INTO").value;
    expect(TokenType::VALUES, "INSERT statement");
    expect(TokenType::LPAREN, "VALUES clause");

    ins.values.push_back(parseValue());
    while (match(TokenType::COMMA))
        ins.values.push_back(parseValue());

    expect(TokenType::RPAREN, "VALUES clause");
    match(TokenType::SEMICOLON);
    return stmt;
}

Statement Parser::parseUpdate() {
    Statement stmt;
    stmt.type = StmtType::UPDATE;
    UpdateStmt& upd = stmt.update;

    upd.table = expect(TokenType::IDENTIFIER, "UPDATE statement").value;
    expect(TokenType::SET, "UPDATE statement");

    auto parseSetClause = [&]() -> SetClause {
        SetClause sc;
        sc.column = expect(TokenType::IDENTIFIER, "SET clause").value;
        expect(TokenType::EQUAL, "SET clause");
        sc.value  = parseValue();
        return sc;
    };

    upd.setClauses.push_back(parseSetClause());
    while (match(TokenType::COMMA))
        upd.setClauses.push_back(parseSetClause());

    if (match(TokenType::WHERE)) {
        upd.hasWhere = true;
        upd.where    = parseCondition();
    }

    match(TokenType::SEMICOLON);
    return stmt;
}

Statement Parser::parseDelete() {
    Statement stmt;
    stmt.type = StmtType::DELETE;
    DeleteStmt& del = stmt.deleteStmt;

    expect(TokenType::FROM, "DELETE statement");
    del.table = expect(TokenType::IDENTIFIER, "FROM clause").value;

    if (match(TokenType::WHERE)) {
        del.hasWhere = true;
        del.where    = parseCondition();
    }

    match(TokenType::SEMICOLON);
    return stmt;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

Statement Parser::parse() {
    if (check(TokenType::SELECT)) { consume(); return parseSelect(); }
    if (check(TokenType::INSERT)) { consume(); return parseInsert(); }
    if (check(TokenType::UPDATE)) { consume(); return parseUpdate(); }
    if (check(TokenType::DELETE)) { consume(); return parseDelete(); }

    throw ParseError("Unknown statement: '" + current().value +
                     "'. Expected SELECT, INSERT, UPDATE, or DELETE.");
}
