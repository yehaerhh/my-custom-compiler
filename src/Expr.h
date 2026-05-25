#pragma once
#include "Token.h"
#include <memory>
#include <utility>

// The base class that all expressions will inherit from
struct Expr {
    // A virtual destructor ensures proper cleanup of the tree from the bottom up
    virtual ~Expr() = default;
};

// 1. Binary Expressions (e.g., a + b, x == y)
struct Binary : public Expr {
    std::unique_ptr<Expr> left;
    Token op;
    std::unique_ptr<Expr> right;

    Binary(std::unique_ptr<Expr> left, Token op, std::unique_ptr<Expr> right)
        : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
};

// 2. Unary Expressions (e.g., -a, !b)
struct Unary : public Expr {
    Token op;
    std::unique_ptr<Expr> right;

    Unary(Token op, std::unique_ptr<Expr> right)
        : op(std::move(op)), right(std::move(right)) {}
};

// 3. Literal Expressions (e.g., 100.5, "hello", true)
struct Literal : public Expr {
    LiteralValue value;

    explicit Literal(LiteralValue value) 
        : value(std::move(value)) {}
};

// 4. Grouping Expressions (e.g., the parentheses in (a + b) )
struct Grouping : public Expr {
    std::unique_ptr<Expr> expression;

    explicit Grouping(std::unique_ptr<Expr> expression)
        : expression(std::move(expression)) {}
};

// 5. Struct Access Expressions (e.g., my_car->speed)
struct StructAccess : public Expr {
    std::unique_ptr<Expr> object; // The left side (e.g., my_car)
    Token property;               // The right side (e.g., speed)

    StructAccess(std::unique_ptr<Expr> object, Token property)
        : object(std::move(object)), property(std::move(property)) {}
};

// 6. Variable Expressions (e.g., my_car)
struct Variable : public Expr {
    Token name;

    explicit Variable(Token name)
        : name(std::move(name)) {}
};


// 7. Assignment Expressions (e.g., a = 6)
struct Assign : public Expr {
    Token name; // The variable being assigned to
    std::unique_ptr<Expr> value; // The new value

    Assign(Token name, std::unique_ptr<Expr> value)
        : name(std::move(name)), value(std::move(value)) {}
};