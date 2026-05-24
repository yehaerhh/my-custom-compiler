#pragma once
#include <string>
#include <vector>
#include <unordered_map> // Add this line!
#include "Token.h"

class Scanner {
private:
    std::string source;
    std::vector<Token> tokens;

    // Tracking our position in the source code
    int start = 0;
    int current = 0;
    int line = 1;

    // Task 19: The Keyword Dictionary
    std::unordered_map<std::string, TokenType> keywords = {
        {"let",    TokenType::KW_LET},
        {"if",     TokenType::KW_IF},
        {"else",   TokenType::KW_ELSE},
        {"for",    TokenType::KW_FOR},
        {"fn",     TokenType::KW_FN},
        {"struct", TokenType::KW_STRUCT},
        {"return", TokenType::KW_RETURN},
        {"print",  TokenType::KW_PRINT},
        {"true",   TokenType::KW_TRUE},
        {"false",  TokenType::KW_FALSE},
        {"null",   TokenType::KW_NULL}
    };

    // --- Helper Methods ---
    
    // Have we consumed all the characters?
    bool isAtEnd() const {
        return current >= source.length();
    }

    // Consumes the current character and moves the pointer forward
    char advance() {
        return source[current++];
    }

    // Looks at the next character. If it matches what we expect, consume it.
    bool match(char expected) {
        if (isAtEnd()) return false;
        if (source[current] != expected) return false;
        current++; // It's a match! Consume the second character.
        return true;
    }

    // Looks at the current character without consuming it
    char peek() const {
        if (isAtEnd()) return '\0';
        return source[current];
    }

    // Checks if a character is a number between 0 and 9
    bool isDigit(char c) const {
        return c >= '0' && c <= '9';
    }

    // Checks if a character is a letter or an underscore
    bool isAlpha(char c) const {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
                c == '_';
    }

    // Checks if a character is valid INSIDE a variable name
    bool isAlphaNumeric(char c) const {
        return isAlpha(c) || isDigit(c);
    }

    // Looks two characters ahead (needed for decimals)
    char peekNext() const {
        if (current + 1 >= source.length()) return '\0';
        return source[current + 1];
    }

    // Adds a token with no literal value (like a '+' or ']')
    void addToken(TokenType type) {
        tokens.push_back(Token(type, source.substr(start, current - start), std::monostate{}, line));
    }

    // Adds a token with a literal value (like a number or string)
    void addToken(TokenType type, LiteralValue literal) {
        tokens.push_back(Token(type, source.substr(start, current - start), literal, line));
    }

    // Task 16: String Literal Scanning
    void stringLiteral() {
        // Keep advancing until we hit the closing quote OR the end of the file
        while (peek() != '"' && !isAtEnd()) {
            if (peek() == '\n') line++; // Support multi-line strings!
            advance();
        }

        // If we hit EOF before finding the closing quote, throw an error
        if (isAtEnd()) {
            std::cerr << "Lexical error: Unterminated string at line " << line << std::endl;
            return;
        }

        // Consume the closing quote '"'
        advance();

        // Extract the actual text WITHOUT the surrounding quotes
        // 'start' is the opening quote. 'current' is right after the closing quote.
        // So we start at (start + 1) and grab a length of (current - start - 2).
        std::string value = source.substr(start + 1, current - start - 2);
        
        addToken(TokenType::STRING, value);
    }

    // Task 17: Numeric Literal Scanning
    void number() {
        // Keep eating numbers
        while (isDigit(peek())) advance();

        // Look for a fractional part (a decimal point followed by MORE numbers)
        if (peek() == '.' && isDigit(peekNext())) {
            // Consume the "."
            advance();

            // Keep eating the fractional numbers
            while (isDigit(peek())) advance();
        }

        // Extract the raw text (e.g., "100.52")
        std::string valueStr = source.substr(start, current - start);
        
        // Convert the string to a C++ 64-bit double
        double value = std::stod(valueStr);
        
        addToken(TokenType::NUMBER, value);
    }

// Task 18 & 19: Identifier and Keyword Scanning
    void identifier() {
        // Keep eating characters
        while (isAlphaNumeric(peek())) advance();

        // Extract the exact text
        std::string text = source.substr(start, current - start);
        
        // See if the text exists in our keyword dictionary
        TokenType type;
        auto match = keywords.find(text);
        
        if (match != keywords.end()) {
            type = match->second; // It's a reserved keyword!
        } else {
            type = TokenType::IDENTIFIER; // It's just a user-defined variable
        }
        
        addToken(type, text);
    }

    void scanToken() {
        char c = advance();
        switch (c) {
            // --- Task 13: Single-character tokens ---
            case '[': addToken(TokenType::LEFT_BRACKET); break; // Your custom block syntax!
            case ']': addToken(TokenType::RIGHT_BRACKET); break;
            case '(': addToken(TokenType::LEFT_PAREN); break;
            case ')': addToken(TokenType::RIGHT_PAREN); break;
            case ',': addToken(TokenType::COMMA); break;
            case ';': addToken(TokenType::SEMICOLON); break;
            case '+': addToken(TokenType::PLUS); break;
            case '*': addToken(TokenType::STAR); break;
            case '&': addToken(TokenType::AMPERSAND); break;
            case '|': addToken(TokenType::PIPE); break;

            // --- Task 14: One-or-two character tokens ---
            case '-':
                // Is it your custom struct access '->' or just a minus sign?
                if (match('>')) {
                    addToken(TokenType::ARROW);
                } else {
                    addToken(TokenType::MINUS);
                }
                break;
            case '!':
                addToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
                break;
            case '=':
                addToken(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
                break;
            case '<':
                addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
                break;
            case '>':
                addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
                break;

            case '/':
                if (match('/')) {
                    // It is a comment! Keep advancing until we hit a newline or EOF.
                    while (peek() != '\n' && !isAtEnd()) {
                        advance();
                    }
                    // Note: We do NOT call addToken() here because we want the 
                    // compiler to completely ignore the comment text.
                } else {
                    // It didn't match a second '/', so it's just a division symbol.
                    addToken(TokenType::SLASH);
                }
                break;

            case ' ':
            case '\r':
            case '\t':
                // Ignore whitespace
                break;
            case '\n':
                line++; // Increment line counter for accurate error messages
                break;

            // ... (previous cases) ...

            case '"': 
                stringLiteral(); 
                break;

            default:
                if (isDigit(c)) {
                    number();
                } else if (isAlpha(c)) {
                    identifier();
                } else {
                    std::cerr << "Lexical error: Unexpected character '" << c << "' at line " << line << std::endl;
                }
                break;
        }
    }

public:
    bool hasError = false;
    Scanner(std::string source) : source(std::move(source)) {}

    // Task 12: The Main Scanner Loop
    std::vector<Token> scanTokens() {
        while (!isAtEnd()) {
            // We are at the beginning of the next lexeme.
            start = current;
            scanToken();
        }

        // Always append an EOF token at the very end so the parser knows to stop
        tokens.push_back(Token(TokenType::END_OF_FILE, "", std::monostate{}, line));
        return tokens;
    }
};