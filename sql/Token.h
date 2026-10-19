#pragma once
#include <string>

enum class TokenType {
    // DML Keywords
    SELECT, FROM, WHERE,
    INSERT, INTO, VALUES,
    UPDATE, SET,
    DELETE,

    // Condition / Logical Keywords
    BETWEEN, AND, OR, NOT,

    // Literals & Identifiers
    IDENTIFIER, NUMBER, STRING,

    // Punctuation
    COMMA, SEMICOLON, STAR,
    LPAREN, RPAREN,

    // Comparison Operators
    EQUAL,         // =
    NOT_EQUAL,     // != or <>
    GREATER,       // >
    LESS,          // <
    GREATER_EQUAL, // >=
    LESS_EQUAL,    // <=

    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType   type;
    std::string value;
    int         line = 1;
};
