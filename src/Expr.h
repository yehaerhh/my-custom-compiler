#pragma once
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <functional>
#include "Token.h"

// 1. FORWARD DECLARATIONS (CRITICAL)
struct StructBlueprint;
struct StructInstance;
struct NativeFunction;
struct RuntimeFunction; // <-- YOU WERE MISSING THIS
class Environment;

// 2. DEFINE OBJECT (Variant knows about the pointers now)
using Object = std::variant<
    std::monostate, 
    double, 
    std::string, 
    bool,
    std::shared_ptr<StructBlueprint>,
    std::shared_ptr<StructInstance>,
    std::shared_ptr<RuntimeFunction>,
    std::shared_ptr<NativeFunction>
>;

// 3. INCLUDE CHUNK NOW (Because Chunk needs Object, which is now defined)
#include "Chunk.h"

// 4. FULLY DEFINE RUNTIMEFUNCTION (Because it needs Chunk)
struct RuntimeFunction {
    int arity;
    std::string name;

    bool isMarked = false; // <--- ADD THIS

    RuntimeFunction(std::string name, int arity)
        : arity(arity), name(name) {} // Match the order here too
    
    std::string toString() const { return "<fn " + name + ">"; }
};

// 5. NOW WE CAN INCLUDE STMT
#include "Stmt.h"

// Forward declarations for the visitor
struct Binary;
struct Unary;
struct Literal;
struct Grouping;
struct StructAccess;
struct Variable;
struct Assign;
struct Call;
struct StructSet;

// Task 38: The Expression Visitor Interface
struct ExprVisitor {
    virtual Object visitBinaryExpr(const Binary* expr) = 0;
    virtual Object visitUnaryExpr(const Unary* expr) = 0;
    virtual Object visitLiteralExpr(const Literal* expr) = 0;
    virtual Object visitGroupingExpr(const Grouping* expr) = 0;
    virtual Object visitStructAccessExpr(const StructAccess* expr) = 0;
    virtual Object visitVariableExpr(const Variable* expr) = 0;
    virtual Object visitAssignExpr(const Assign* expr) = 0;
    virtual Object visitCallExpr(const Call* expr) = 0;
    virtual Object visitStructSetExpr(const StructSet* expr) = 0;
    virtual ~ExprVisitor() = default;
};

// The base class now enforces that every node can "accept" a visitor
struct Expr {
    virtual Object accept(ExprVisitor* visitor) const = 0;
    virtual ~Expr() = default;
};

// ... KEEP ALL YOUR AST STRUCTS BELOW THIS LINE (Binary, Unary, Literal, etc) ...

 // 1. Binary Expressions (e.g., a + b, x == y)

struct Binary : public Expr {
    std::unique_ptr<Expr> left;
    Token op;
    std::unique_ptr<Expr> right;

    Binary(std::unique_ptr<Expr> left, Token op, std::unique_ptr<Expr> right)
        : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}

    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitBinaryExpr(this);
    }
};

 // 2. Unary Expressions (e.g., -a, !b)
struct Unary : public Expr {
    Token op;
    std::unique_ptr<Expr> right;

    Unary(Token op, std::unique_ptr<Expr> right)
        : op(std::move(op)), right(std::move(right)) {}
    
    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitUnaryExpr(this);
    }
};

 // 3. Literal Expressions (e.g., 100.5, "hello", true)
 // 3. Literal Expressions (e.g., 100.5, "hello", true)
struct Literal : public Expr {
    Object value;

    // Single constructor using perfect forwarding templates to accept any type 
    // that fits into our Object variant without ambiguity!
    template<typename T>
    explicit Literal(T&& val) : value(std::forward<T>(val)) {}

    // Special overload if a token primitive variant is passed directly
    explicit Literal(std::variant<std::monostate, double, std::string, bool> token_literal) {
        value = std::visit([](auto&& arg) -> Object { 
            return arg; 
        }, token_literal);
    }

    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitLiteralExpr(this);
    }
};

// 4. Grouping Expressions (e.g., the parentheses in (a + b) )
struct Grouping : public Expr {
    std::unique_ptr<Expr> expression;

    explicit Grouping(std::unique_ptr<Expr> expression)
        : expression(std::move(expression)) {}

    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitGroupingExpr(this);
    }
};

// 5. Struct Access Expressions (e.g., my_car->speed)
struct StructAccess : public Expr {
    std::unique_ptr<Expr> object; // The left side (e.g., my_car)
    Token property;               // The right side (e.g., speed)

    StructAccess(std::unique_ptr<Expr> object, Token property)
        : object(std::move(object)), property(std::move(property)) {}

    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitStructAccessExpr(this);
    }
};

// Task 51: Struct Field Assignment Expressions (e.g., my_car->speed = 60)
struct StructSet : public Expr {
    std::unique_ptr<Expr> object; // The base instance
    Token property;               // The field name
    std::unique_ptr<Expr> value;  // The new value being written

    StructSet(std::unique_ptr<Expr> object, Token property, std::unique_ptr<Expr> value)
        : object(std::move(object)), property(std::move(property)), value(std::move(value)) {}

    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitStructSetExpr(this);
    }
};

// 6. Variable Expressions (e.g., my_car)
struct Variable : public Expr {
    Token name;

    explicit Variable(Token name)
        : name(std::move(name)) {}

    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitVariableExpr(this);
    }
};

// 7. Assignment Expressions (e.g., a = 6)
struct Assign : public Expr {
    Token name; // The variable being assigned to
    std::unique_ptr<Expr> value; // The new value

    Assign(Token name, std::unique_ptr<Expr> value)
        : name(std::move(name)), value(std::move(value)) {}

    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitAssignExpr(this);
    }
};

// FIXED: 8. ADDED THE MISSING CALL STRUCT NODE DEFINITION HERE
struct Call : public Expr {
    std::unique_ptr<Expr> callee;
    Token paren; // Closing parenthesis used for error lines
    std::vector<std::unique_ptr<Expr>> arguments;

    Call(std::unique_ptr<Expr> callee, Token paren, std::vector<std::unique_ptr<Expr>> arguments)
        : callee(std::move(callee)), paren(std::move(paren)), arguments(std::move(arguments)) {}

    Object accept(ExprVisitor* visitor) const override {
        return visitor->visitCallExpr(this);
    }
};

// Task 49: The runtime blueprint of a structural type
class StructBlueprint {
public:
    std::string name;
    explicit StructBlueprint(std::string name) : name(std::move(name)) {}
    
    std::string toString() const { return name; }
};

// Task 49: The actual allocated object container on the heap
class StructInstance {
public: // Everything must be public for the VM to access it!
    std::shared_ptr<StructBlueprint> blueprint;
    std::unordered_map<std::string, Object> fields;

    bool isMarked = false; // <--- ADD THIS

    // 1. Only ONE constructor
    explicit StructInstance(std::shared_ptr<StructBlueprint> blueprint) 
        : blueprint(std::move(blueprint)) {}

    // 2. Only ONE toString method (with the fixed string concatenation)
    std::string toString() const { 
        if (blueprint) return "<instance " + blueprint->name + ">";
        return "<instance anonymous>"; 
    }

    // 3. Keep the old tree-walk methods just so the old AST nodes don't break
    Object get(const Token& nameToken) {
        if (fields.find(nameToken.lexeme) != fields.end()) {
            return fields[nameToken.lexeme];
        }
        return std::monostate{};
    }

    void set(const Token& nameToken, const Object& value) {
        fields[nameToken.lexeme] = value;
    }
};

// Task 53: A wrapper for underlying C++ functions injected into the environment
class NativeFunction {
public:
    int arity; // How many arguments the native function expects
    std::function<Object(const std::vector<Object>&)> callable;

    NativeFunction(int arity, std::function<Object(const std::vector<Object>&)> callable)
        : arity(arity), callable(std::move(callable)) {}

    std::string toString() const { return "<native fn>"; }
};

