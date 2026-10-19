#include "Lexer.h"
#include <cctype>
#include <unordered_map>
#include <algorithm>

Lexer::Lexer(const std::string& input)
    : input(input), position(0), line(1) {}

char Lexer::peek(int offset) const {
    size_t idx = position + offset;
    if (idx >= input.size()) return '\0';
    return input[idx];
}

char Lexer::advance() {
    if (position >= input.size()) return '\0';
    char c = input[position++];
    if (c == '\n') line++;
    return c;
}

void Lexer::skipWhitespace() {
    while (std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

Token Lexer::readIdentifierOrKeyword() {
    std::string value;
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        value += advance();
    }

    // Case-insensitive keyword lookup
    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"SELECT",  TokenType::SELECT},
        {"FROM",    TokenType::FROM},
        {"WHERE",   TokenType::WHERE},
        {"INSERT",  TokenType::INSERT},
        {"INTO",    TokenType::INTO},
        {"VALUES",  TokenType::VALUES},
        {"UPDATE",  TokenType::UPDATE},
        {"SET",     TokenType::SET},
        {"DELETE",  TokenType::DELETE},
        {"BETWEEN", TokenType::BETWEEN},
        {"AND",     TokenType::AND},
        {"OR",      TokenType::OR},
        {"NOT",     TokenType::NOT},
    };

    auto it = keywords.find(upper);
    if (it != keywords.end()) {
        return {it->second, upper, line};
    }
    return {TokenType::IDENTIFIER, value, line};
}

Token Lexer::readNumber() {
    std::string value;
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        value += advance();
    }
    return {TokenType::NUMBER, value, line};
}

Token Lexer::readString() {
    char quote = advance(); // consume opening quote
    std::string value;
    while (peek() != quote && peek() != '\0') {
        value += advance();
    }
    if (peek() == quote) advance(); // consume closing quote
    return {TokenType::STRING, value, line};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (position < input.size()) {
        skipWhitespace();
        if (position >= input.size()) break;

        char cur = peek();
        if (cur == '\0') break;

        if (std::isalpha(static_cast<unsigned char>(cur)) || cur == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(cur))) {
            tokens.push_back(readNumber());
            continue;
        }
        if (cur == '\'' || cur == '"') {
            tokens.push_back(readString());
            continue;
        }

        switch (cur) {
            case ',': advance(); tokens.push_back({TokenType::COMMA,     ",", line}); break;
            case ';': advance(); tokens.push_back({TokenType::SEMICOLON, ";", line}); break;
            case '*': advance(); tokens.push_back({TokenType::STAR,      "*", line}); break;
            case '(': advance(); tokens.push_back({TokenType::LPAREN,    "(", line}); break;
            case ')': advance(); tokens.push_back({TokenType::RPAREN,    ")", line}); break;
            case '=': advance(); tokens.push_back({TokenType::EQUAL,     "=", line}); break;
            case '!':
                advance();
                if (peek() == '=') { advance(); tokens.push_back({TokenType::NOT_EQUAL, "!=", line}); }
                else                             tokens.push_back({TokenType::UNKNOWN,   "!",  line});
                break;
            case '<':
                advance();
                if (peek() == '=') { advance(); tokens.push_back({TokenType::LESS_EQUAL, "<=", line}); }
                else if (peek() == '>') { advance(); tokens.push_back({TokenType::NOT_EQUAL, "<>", line}); }
                else                             tokens.push_back({TokenType::LESS,       "<",  line});
                break;
            case '>':
                advance();
                if (peek() == '=') { advance(); tokens.push_back({TokenType::GREATER_EQUAL, ">=", line}); }
                else                             tokens.push_back({TokenType::GREATER,        ">",  line});
                break;
            default:
                tokens.push_back({TokenType::UNKNOWN, std::string(1, advance()), line});
        }
    }

    tokens.push_back({TokenType::END_OF_FILE, "", line});
    return tokens;
}