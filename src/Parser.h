#pragma once
#include <vector>
#include <stdexcept>
#include <iostream>
#include <memory>
#include "Token.h"
#include "Expr.h"
#include "Stmt.h"

class Parser {
private:
    std::vector<Token> tokens;
    int current = 0; // Tracks which token we are currently looking at

    // --- 1. Navigation Helpers ---
    
    // Look at the current token without consuming it
    Token peek() const {
        return tokens[current];
    }

    // Look at the most recently consumed token
    Token previous() const {
        return tokens[current - 1];
    }

    bool isAtEnd() const {
        return peek().type == TokenType::END_OF_FILE;
    }

    // Consume the current token and move forward
    Token advance() {
        if (!isAtEnd()) current++;
        return previous();
    }

    // --- 2. Matching Helpers ---

    // Check if the current token is of a specific type
    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    // If the current token matches ANY of the provided types, consume it and return true.
    bool match(const std::vector<TokenType>& types) {
        for (TokenType type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }
        return false;
    }

    // --- 3. Error Handling Scaffold ---
    
    // We use a custom exception to instantly break out of deep recursive loops if syntax is bad
    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& message) : std::runtime_error(message) {}
    };

    // "I expect the next token to be X. If it is, consume it. If not, blow up."
    Token consume(TokenType type, const std::string& message) {
        if (check(type)) return advance();

        Token badToken = peek();
        std::cerr << "[Line " << badToken.line << "] Syntax Error: " << message << std::endl;
        throw ParseError(message);
    }

    std::unique_ptr<Stmt> declaration() {
        try {
            // If we match 'let', jump to variable declarations!
            if (match({TokenType::KW_FN})) return functionDeclaration("function");
            if (match({TokenType::KW_STRUCT})) return structDeclaration();
            if (match({TokenType::KW_LET})) return varDeclaration();
            
            return statement();
        } catch (ParseError& error) {
            synchronize();
            return nullptr;
        }
    }


    std::unique_ptr<Stmt> statement() {
        if (match({TokenType::KW_RETURN})) return returnStatement();
        if (match({TokenType::KW_FOR})) return forStatement(); // Catch loop syntax
        if (match({TokenType::KW_IF})) return ifStatement();
        if (match({TokenType::KW_PRINT})) return printStatement();
        
        if (match({TokenType::LEFT_BRACKET})) {
            return std::make_unique<Block>(block());
        }
        
        return expressionStatement();
    }

    
    std::unique_ptr<Stmt> varDeclaration() {
        // We already matched 'let', so the next token MUST be the variable identifier name
        Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");

        std::unique_ptr<Expr> initializer = nullptr;
        
        // If there is an '=', parse the right-hand side expression
        if (match({TokenType::EQUAL})) {
            initializer = expression();
        }

        // Variable statements must end with a semicolon
        consume(TokenType::SEMICOLON, "Expect ';' after variable declaration.");
        return std::make_unique<VarStmt>(std::move(name), std::move(initializer));
    }

    std::unique_ptr<Stmt> functionDeclaration(std::string kind) {
        Token name = consume(TokenType::IDENTIFIER, "Expect " + kind + " name.");
        consume(TokenType::LEFT_PAREN, "Expect '(' after " + kind + " name.");
        
        std::vector<Token> parameters;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                parameters.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name."));
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
        consume(TokenType::LEFT_BRACKET, "Expect '[' before " + kind + " body.");
        
        // Parse the block body (your existing block() method)
        std::unique_ptr<Stmt> body = std::make_unique<Block>(block());
        
        return std::make_unique<FunctionStmt>(std::move(name), std::move(parameters), std::move(body));
    }

    std::unique_ptr<Stmt> returnStatement() {
        Token keyword = previous();
        std::unique_ptr<Expr> value = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            value = expression();
        }
        consume(TokenType::SEMICOLON, "Expect ';' after return value.");
        return std::make_unique<ReturnStmt>(std::move(keyword), std::move(value));
    }

    std::unique_ptr<Stmt> printStatement() {
        std::unique_ptr<Expr> value = expression();
        consume(TokenType::SEMICOLON, "Expect ';' after print value.");
        return std::make_unique<PrintStmt>(std::move(value));
    }

    std::unique_ptr<Stmt> expressionStatement() {
        std::unique_ptr<Expr> expr = expression();
        consume(TokenType::SEMICOLON, "Expect ';' after expression.");
        return std::make_unique<ExpressionStmt>(std::move(expr));
    }

    // Task 34: Parse custom square-bracket blocks
    std::vector<std::unique_ptr<Stmt>> block() {
        std::vector<std::unique_ptr<Stmt>> statements;

        // Keep eating statements until we see the closing bracket ']' or hit EOF
        while (!check(TokenType::RIGHT_BRACKET) && !isAtEnd()) {
            std::unique_ptr<Stmt> decl = declaration();
            if (decl != nullptr) {
                statements.push_back(std::move(decl));
            }
        }

        consume(TokenType::RIGHT_BRACKET, "Expect ']' after block.");
        return statements;
    }

    // Task 35: Parse If and If-Else Statements
    std::unique_ptr<Stmt> ifStatement() {
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
        std::unique_ptr<Expr> condition = expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");

        std::unique_ptr<Stmt> thenBranch = statement();
        std::unique_ptr<Stmt> elseBranch = nullptr;

        // If the very next token is 'else', grab its dependent execution block
        if (match({TokenType::KW_ELSE})) {
            elseBranch = statement();
        }

        return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
    }

    // Task 37: Parse and Desugar For Loops
    std::unique_ptr<Stmt> forStatement() {
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");

        // 1. Parse the Initialization clause
        std::unique_ptr<Stmt> initializer;
        if (match({TokenType::SEMICOLON})) {
            initializer = nullptr; // Empty initialization: for (; ...
        } else if (match({TokenType::KW_LET})) {
            initializer = varDeclaration(); // Declares a new loop variable: for (let i = 0; ...
        } else {
            initializer = expressionStatement(); // Mutates an existing variable: for (i = 0; ...
        }

        // 2. Parse the Condition clause
        std::unique_ptr<Expr> condition = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            condition = expression();
        }
        consume(TokenType::SEMICOLON, "Expect ';' after loop condition.");

        // 3. Parse the Increment clause
        std::unique_ptr<Expr> increment = nullptr;
        if (!check(TokenType::RIGHT_PAREN)) {
            increment = expression();
        }
        consume(TokenType::RIGHT_PAREN, "Expect ')' after for clauses.");

        // 4. Parse the loop Body block
        std::unique_ptr<Stmt> body = statement();

        // --- THE DESUGARING ASSEMBLY ---
        
        // A: If there is an increment clause, execute it at the end of every loop cycle body
        if (increment != nullptr) {
            std::vector<std::unique_ptr<Stmt>> bodyStmts;
            bodyStmts.push_back(std::move(body));
            bodyStmts.push_back(std::make_unique<ExpressionStmt>(std::move(increment)));
            body = std::make_unique<Block>(std::move(bodyStmts));
        }

        // B: If there is no condition clause, make it default to an infinite 'true' loop check
        if (condition == nullptr) {
            condition = std::make_unique<Literal>(true);
        }
        body = std::make_unique<WhileStmt>(std::move(condition), std::move(body));

        // C: If there is an initializer statement, wrap everything inside a parent block scope
        if (initializer != nullptr) {
            std::vector<std::unique_ptr<Stmt>> outerStmts;
            outerStmts.push_back(std::move(initializer));
            outerStmts.push_back(std::move(body));
            body = std::make_unique<Block>(std::move(outerStmts));
        }

        return body;
    }

    std::unique_ptr<Stmt> structDeclaration() {
        // 1. Consume the identifier name of the struct (e.g., "Point")
        Token name = consume(TokenType::IDENTIFIER, "Expect struct name.");
        
        // 2. Consume the opening bracket '['
        consume(TokenType::LEFT_BRACKET, "Expect '[' before struct body.");
        
        // 3. Loop through the body and collect all the property names!
        std::vector<Token> properties;
        while (!check(TokenType::RIGHT_BRACKET) && !isAtEnd()) {
            consume(TokenType::KW_LET, "Expect 'let' in struct.");
            Token propName = consume(TokenType::IDENTIFIER, "Expect property name.");
            properties.push_back(propName); // Save 'x', 'y', etc.
            consume(TokenType::SEMICOLON, "Expect ';' after property declaration.");
        }
        
        // 4. Consume the closing bracket ']'
        consume(TokenType::RIGHT_BRACKET, "Expect ']' after struct body.");
        
        // 5. Consume the trailing semicolon (since your previous test required it!)
        consume(TokenType::SEMICOLON, "Expect ';' after struct declaration.");
        
        // 6. Pass the ACTUAL list of properties to the AST node
        return std::make_unique<StructDecl>(name, properties);
    }

    // Panic Recovery: If a syntax error happens, skip tokens until we find a 
    // statement boundary (like a semicolon) so the compiler can keep finding other errors.
    void synchronize() {
        advance();

        while (!isAtEnd()) {
            if (previous().type == TokenType::SEMICOLON) return;

            switch (peek().type) {
                case TokenType::KW_STRUCT:
                case TokenType::KW_FN:
                case TokenType::KW_LET:
                case TokenType::KW_IF:
                case TokenType::KW_FOR:
                case TokenType::KW_RETURN:
                case TokenType::KW_PRINT:
                    return;
                default:
                    break;
            }

            advance();
        }
    }

    // --- 4. Recursive Descent Parsing Rules ---

    // Temporary stub! We will expand this in Tasks 24-26 to handle math operators.
    std::unique_ptr<Expr> expression() {
        return assignment();
    }

    // Task 33: Parse Assignment Expressions (a = 6)
    std::unique_ptr<Expr> assignment() {
        // First, parse the left-hand side as if it's a normal expression
        std::unique_ptr<Expr> expr = logicalOr();

        // If we see an assignment operator '=', we intercept it!
        if (match({TokenType::EQUAL})) {
            Token equals = previous();
            std::unique_ptr<Expr> value = assignment(); // Recursively call assignment to support right-associativity (e.g., a = b = 6)

            // Look back at the left-hand expression 'expr' to verify it is a valid target
            if (Variable* v = dynamic_cast<Variable*>(expr.get())) {
                Token name = v->name;
                return std::make_unique<Assign>(name, std::move(value));
            }
            else if (StructAccess* access = dynamic_cast<StructAccess*>(expr.get())) {
                // Extract the object and property from the parsed access node, and build a SET node!
                return std::make_unique<StructSet>(std::move(access->object), access->property, std::move(value));
            }

            // If it's not a valid storage variable, throw an error but don't crash the parser panic loop
            std::cerr << "[Line " << equals.line << "] Syntax Error: Invalid assignment target." << std::endl;
        }

        return expr;
    }

    // Task 26: Parse Comparison operators (<, >, <=, >=)
    std::unique_ptr<Expr> comparison() {
        std::unique_ptr<Expr> expr = term(); // Jump up to addition/subtraction

        while (match({TokenType::LESS, TokenType::LESS_EQUAL, TokenType::GREATER, TokenType::GREATER_EQUAL})) {
            Token op = previous();
            std::unique_ptr<Expr> right = term();
            expr = std::make_unique<Binary>(std::move(expr), std::move(op), std::move(right));
        }

        return expr;
    }

    // Task 36: Parse Logical AND expressions (&)
    std::unique_ptr<Expr> logicalAnd() {
        std::unique_ptr<Expr> expr = equality(); // Jump up to equality operators

        while (match({TokenType::AMPERSAND})) { // Assuming AMPERSAND is your token type for '&'
            Token op = previous();
            std::unique_ptr<Expr> right = equality();
            expr = std::make_unique<Binary>(std::move(expr), std::move(op), std::move(right));
        }

        return expr;
    }

    // Task 36: Parse Logical OR expressions (|)
    std::unique_ptr<Expr> logicalOr() {
        std::unique_ptr<Expr> expr = logicalAnd(); // Jump up to logical AND

        while (match({TokenType::PIPE})) { // Assuming PIPE is your token type for '|'
            Token op = previous();
            std::unique_ptr<Expr> right = logicalAnd();
            expr = std::make_unique<Binary>(std::move(expr), std::move(op), std::move(right));
        }

        return expr;
    }

    // Task 26: Parse Equality operators (==, !=)
    std::unique_ptr<Expr> equality() {
        std::unique_ptr<Expr> expr = comparison(); // Jump up to comparisons

        while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
            Token op = previous();
            std::unique_ptr<Expr> right = comparison();
            expr = std::make_unique<Binary>(std::move(expr), std::move(op), std::move(right));
        }

        return expr;
    }

    // Task 25: Parse Multiplication and Division (*, /)
    std::unique_ptr<Expr> factor() {
        std::unique_ptr<Expr> expr = unary(); // Jump up to the next precedence level

        // While we keep seeing * or /, keep chaining them together as Binary nodes
        while (match({TokenType::STAR, TokenType::SLASH})) {
            Token op = previous();
            std::unique_ptr<Expr> right = unary();
            expr = std::make_unique<Binary>(std::move(expr), std::move(op), std::move(right));
        }

        return expr;
    }

    // Task 25: Parse Addition and Subtraction (+, -)
    std::unique_ptr<Expr> term() {
        std::unique_ptr<Expr> expr = factor(); // Jump up to multiplication/division

        // While we keep seeing + or -, keep chaining them together as Binary nodes
        while (match({TokenType::PLUS, TokenType::MINUS})) {
            Token op = previous();
            std::unique_ptr<Expr> right = factor();
            expr = std::make_unique<Binary>(std::move(expr), std::move(op), std::move(right));
        }

        return expr;
    }

    // Task 28: Parse Struct Access (my_car->speed)
    std::unique_ptr<Expr> access() {
        std::unique_ptr<Expr> expr = primary(); // Grab the object first (e.g., my_car)

        // While we keep seeing '->', keep chaining them (e.g., my_car->engine->cylinders)
        while (match({TokenType::ARROW})) {
            // If we see an arrow, the VERY NEXT token MUST be a variable name
            Token property = consume(TokenType::IDENTIFIER, "Expect property name after '->'.");
            expr = std::make_unique<StructAccess>(std::move(expr), std::move(property));
        }

        return expr;
    }

    // Task 24: Parse Unary operators (!, -)
    // Task 24: Parse Unary operators (!, -)
    std::unique_ptr<Expr> unary() {
        // If the current token is a ! or -, consume it and build a Unary node
        if (match({TokenType::BANG, TokenType::MINUS})) {
            Token op = previous();
            std::unique_ptr<Expr> right = unary(); // Recursive call allows stacking like !!true
            return std::make_unique<Unary>(std::move(op), std::move(right));
        }

        // 1. Unary falls through directly to the combined Call/Access level!
        return call(); 
    }

    // Task 28 & 52: Parse function calls () and struct access ->
    std::unique_ptr<Expr> call() {
        // 2. Grab the base object first (e.g., 'Engine' or 'my_car')
        std::unique_ptr<Expr> expr = primary(); 

        // 3. Keep looping to chain ANY combination of () and -> together
        while (true) {
            if (match({TokenType::LEFT_PAREN})) {
                // We saw a '(', parse it as a function/struct instantiation
                expr = finishCall(std::move(expr));
            } 
            else if (match({TokenType::ARROW})) {
                // We saw a '->', parse it as a property lookup
                Token property = consume(TokenType::IDENTIFIER, "Expect property name after '->'.");
                expr = std::make_unique<StructAccess>(std::move(expr), std::move(property));
            } 
            else {
                // Neither '(' nor '->', we are done with this chain!
                break; 
            }
        }

        return expr;
    }

    // Helper to parse the arguments inside the brackets ()
    std::unique_ptr<Expr> finishCall(std::unique_ptr<Expr> callee) {
        std::vector<std::unique_ptr<Expr>> arguments;
        
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                arguments.push_back(expression());
            } while (match({TokenType::COMMA}));
        }
        
        Token paren = consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
        
        // Emits the Call node
        return std::make_unique<Call>(std::move(callee), paren, std::move(arguments));
    }
    // Task 23: Parse basic terminal expressions
    std::unique_ptr<Expr> primary() {
        // 1. Boolean and Null Literals
        if (match({TokenType::KW_FALSE})) return std::make_unique<Literal>(false);
        if (match({TokenType::KW_TRUE}))  return std::make_unique<Literal>(true);
        if (match({TokenType::KW_NULL}))  return std::make_unique<Literal>(std::monostate{});

        // 2. Number and String Literals
        if (match({TokenType::NUMBER, TokenType::STRING})) {
            return std::make_unique<Literal>(previous().literal);
        }

        // 3. Parentheses (Grouping)
        if (match({TokenType::LEFT_PAREN})) {
            // If we see a '(', parse whatever is inside it as an expression
            std::unique_ptr<Expr> expr = expression();
            
            // We MUST see a ')' immediately after, otherwise it's a syntax error
            consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
            
            return std::make_unique<Grouping>(std::move(expr));
        }

        // 3.5 Identifiers (Variables)
        if (match({TokenType::IDENTIFIER})) {
            return std::make_unique<Variable>(previous());
        }

        // 4. Error Handling
        // If we hit this line, the token doesn't start any valid expression
        Token badToken = peek();
        std::cerr << "[Line " << badToken.line << "] Syntax Error: Expect expression." << std::endl;
        throw ParseError("Expect expression.");
    }



public:
    Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}
    // The main entry point that main.cpp calls

    // Upgraded Task 31 Entry Point
    std::vector<std::unique_ptr<Stmt>> parse() {
        std::vector<std::unique_ptr<Stmt>> statements;
        
        while (!isAtEnd()) {
            std::unique_ptr<Stmt> decl = declaration();
            if (decl != nullptr) {
                statements.push_back(std::move(decl));
            }
        }
        
        return statements;
    }
};