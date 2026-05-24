#pragma once
#include <string>
#include <variant>

// 1. Every single valid piece of grammar in your language
enum class TokenType {
    // Single-character tokens
    LEFT_BRACKET, RIGHT_BRACKET, // Your custom [ ] block syntax
    LEFT_PAREN, RIGHT_PAREN,     // ( )
    COMMA, SEMICOLON,            // , ;
    PLUS, MINUS, STAR, SLASH,    // + - * /
    AMPERSAND, PIPE,             // & |
    BANG, EQUAL, LESS, GREATER,  // ! = < >

    // Two-character tokens
    BANG_EQUAL, EQUAL_EQUAL,     // != ==
    LESS_EQUAL, GREATER_EQUAL,   // <= >=
    ARROW,                       // -> (Your custom struct access)

    // Literals
    IDENTIFIER, STRING, NUMBER,

    // Keywords
    KW_LET, KW_IF, KW_ELSE, KW_FOR, KW_FN, KW_STRUCT, 
    KW_RETURN, KW_PRINT, KW_TRUE, KW_FALSE, KW_NULL,

    END_OF_FILE
};

// 2. A type-safe way to hold either nothing, a 64-bit float, or a string
using LiteralValue = std::variant<std::monostate, double, std::string>;

// 3. The core Token structure
struct Token {
    TokenType type;
    std::string lexeme;   // The exact raw text (e.g., "let", "->", "100")
    LiteralValue literal; // The parsed value (if it's a string or number)
    int line;             // For printing accurate error messages

    // Constructor
    Token(TokenType type, std::string lexeme, LiteralValue literal, int line)
        : type(type), lexeme(std::move(lexeme)), literal(std::move(literal)), line(line) {}
};