#pragma once
#include "Token.h"
#include <string>
#include <vector>

class Lexer {
private:
    std::string input;
    size_t      position;
    int         line;

    char peek(int offset = 0) const;
    char advance();
    void skipWhitespace();
    Token readIdentifierOrKeyword();
    Token readNumber();
    Token readString();

public:
    explicit Lexer(const std::string& input);
    std::vector<Token> tokenize();
};