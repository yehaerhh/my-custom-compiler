#pragma once
#include <memory>
#include <vector>
#include "Token.h"
#include "Expr.h"

// Forward declarations
struct ExpressionStmt;
struct PrintStmt;
struct StructDecl;
struct VarStmt;
struct Block;
struct IfStmt;
struct WhileStmt;

// Task 38: The Statement Visitor Interface
struct StmtVisitor {
    virtual void visitExpressionStmt(const ExpressionStmt* stmt) = 0;
    virtual void visitPrintStmt(const PrintStmt* stmt) = 0;
    virtual void visitStructDeclStmt(const StructDecl* stmt) = 0;
    virtual void visitVarStmt(const VarStmt* stmt) = 0;
    virtual void visitBlockStmt(const Block* stmt) = 0;
    virtual void visitIfStmt(const IfStmt* stmt) = 0;
    virtual void visitWhileStmt(const WhileStmt* stmt) = 0;
    virtual ~StmtVisitor() = default;
};

struct Stmt {
    virtual void accept(StmtVisitor* visitor) const = 0;
    virtual ~Stmt() = default;
};

// 1. Expression Statement (e.g., "1 + 2;" or "my_car->speed;")
// This exists so we can write an expression as a standalone line of code.
struct ExpressionStmt : public Stmt {
    std::unique_ptr<Expr> expression;

    explicit ExpressionStmt(std::unique_ptr<Expr> expression)
        : expression(std::move(expression)) {}

    void accept(StmtVisitor* visitor) const override { visitor->visitExpressionStmt(this); }
};

// 2. Print Statement (e.g., "print 100;")
struct PrintStmt : public Stmt {
    std::unique_ptr<Expr> expression;

    explicit PrintStmt(std::unique_ptr<Expr> expression)
        : expression(std::move(expression)) {}

    void accept(StmtVisitor* visitor) const override { visitor->visitPrintStmt(this); }
};

// 3. Struct Declaration (e.g., "struct Player [ let health; let speed; ]")
struct StructDecl : public Stmt {
    Token name; // The name of the struct (e.g., 'Player')
    
    // For now, we will store the properties just as a list of variable name tokens
    std::vector<Token> properties; 

    StructDecl(Token name, std::vector<Token> properties)
        : name(std::move(name)), properties(std::move(properties)) {}

    void accept(StmtVisitor* visitor) const override { visitor->visitStructDeclStmt(this); }
};

// 4. Variable Declaration Statement (e.g., "let a = 5;")
struct VarStmt : public Stmt {
    Token name;                       // The variable's name token
    std::unique_ptr<Expr> initializer; // The value expression (can be nullptr if uninitialized)

    VarStmt(Token name, std::unique_ptr<Expr> initializer)
        : name(std::move(name)), initializer(std::move(initializer)) {}

    void accept(StmtVisitor* visitor) const override { visitor->visitVarStmt(this); }
};

// 5. Block Statement (e.g., [ let local = 10; print local; ])
struct Block : public Stmt {
    std::vector<std::unique_ptr<Stmt>> statements; // The nested statements inside the block

    explicit Block(std::vector<std::unique_ptr<Stmt>> statements)
        : statements(std::move(statements)) {}

    void accept(StmtVisitor* visitor) const override { visitor->visitBlockStmt(this); }
};

// 6. If Statement (e.g., if (x > 5) [ print 1; ] else [ print 0; ])
struct IfStmt : public Stmt {
    std::unique_ptr<Expr> condition; // The truth-condition expression
    std::unique_ptr<Stmt> thenBranch; // Executed if true
    std::unique_ptr<Stmt> elseBranch; // Executed if false (can be nullptr)

    IfStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> thenBranch, std::unique_ptr<Stmt> elseBranch)
        : condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}
    
    void accept(StmtVisitor* visitor) const override { visitor->visitIfStmt(this); }
};

// 7. While Statement (Used as the core execution engine for loops)
struct WhileStmt : public Stmt {
    std::unique_ptr<Expr> condition; // The loop check expression
    std::unique_ptr<Stmt> body;      // The block to execute

    WhileStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body)
        : condition(std::move(condition)), body(std::move(body)) {}
    
    void accept(StmtVisitor* visitor) const override { visitor->visitWhileStmt(this); }
};