#pragma once
#include "Expr.h"
#include "Stmt.h"
#include "Environment.h"
#include <iostream>
#include <vector>
#include <exception>
#include <chrono>


#include <exception>

// Exception used to unwind the C++ call stack when a user's 'return' statement is hit!
class ReturnException : public std::exception {
public:
    Object value;
    explicit ReturnException(Object value) : value(std::move(value)) {}
};

class Interpreter : public ExprVisitor, public StmtVisitor {
public:
    // 1. MOVE THIS HERE: The global environment must be accessible and initialized
    std::shared_ptr<Environment> environment = std::make_shared<Environment>();

    // 2. ADD THIS CONSTRUCTOR: It runs exactly once when the compiler starts up
    Interpreter() {
        // Define the native 'clock' function logic using C++ chrono
        auto clockFn = std::make_shared<NativeFunction>(0, [](const std::vector<Object>& args) -> Object {
            auto now = std::chrono::system_clock::now().time_since_epoch();
            return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()) / 1000.0;
        });

        // Inject the clock function into the global environment before any user code runs!
        environment->define("clock", clockFn);
    }

    // 3. YOUR EXISTING METHOD: The master execution entry point loop
    void interpret(const std::vector<std::unique_ptr<Stmt>>& statements) {
        try {
            for (const auto& statement : statements) {
                execute(statement.get());
            }
        } catch (const std::runtime_error& error) {
            std::cerr << "Runtime Error: " << error.what() << std::endl;
        }
    }
private:
    // Helper to evaluate statements

    void execute(const Stmt* stmt) {
        stmt->accept(this);
    }

    // Helper to evaluate expressions
    Object evaluate(const Expr* expr) {
        return expr->accept(this);
    }

    // --- Statement Visitor Implementations ---
    void visitExpressionStmt(const ExpressionStmt* stmt) override {
        evaluate(stmt->expression.get());
    }

    void visitPrintStmt(const PrintStmt* stmt) override {
        Object value = evaluate(stmt->expression.get());
        
        std::visit([](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                std::cout << "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                std::cout << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, std::shared_ptr<StructBlueprint>>) {
                // Task 49: Safely handle printing the base structural type blueprint
                if (arg) std::cout << "<struct " << arg->toString() << ">";
                else std::cout << "null";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<StructInstance>>) {
                if (arg) std::cout << arg->toString();
                else std::cout << "null";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<RuntimeFunction>>) {
                // Task 52: Safely handle printing function pointers!
                if (arg) std::cout << arg->toString();
                else std::cout << "null";
            }else if constexpr (std::is_same_v<T, std::shared_ptr<NativeFunction>>) {
                if (arg) std::cout << arg->toString();
                else std::cout << "null";
            } else {
                std::cout << arg; // Natively handles double and std::string
            }
        }, value);
        
        std::cout << std::endl;
    }

    // Task 49: Dynamic Type Blueprint Registration
    void visitStructDeclStmt(const StructDecl* stmt) override {
        // Extract the raw identifier name dynamically from the AST node
        std::string typeName = stmt->name.lexeme;
        
        // Wrap it in a blueprint container
        auto blueprint = std::make_shared<StructBlueprint>(typeName);
        
        // Save it to the environment using its exact identifier token
        environment->define(stmt->name.lexeme, blueprint);
    }

    // Task 43: Wire up variable declarations
    void visitVarStmt(const VarStmt* stmt) override {
        Object value = std::monostate{}; // Default initialization value is null

        // If the variable was declared with an initial value (e.g., let a = 5;)
        if (stmt->initializer != nullptr) {
            value = evaluate(stmt->initializer.get());
        }

        // Store the variable name string and evaluated value into the map
        environment->define(stmt->name.lexeme, value);
    }


    // Task 47: Evaluate if and if-else branching logic
    void visitIfStmt(const IfStmt* stmt) override {
        // 1. Check the conditional expression's runtime state
        if (isTruthy(evaluate(stmt->condition.get()))) {
            execute(stmt->thenBranch.get());
        } 
        // 2. If it is false, branch into the else sequence if it exists
        else if (stmt->elseBranch != nullptr) {
            execute(stmt->elseBranch.get());
        }
    }

    // --- Expression Visitor Implementations ---
    // Task 41: Evaluate Binary Arithmetic & Comparisons
    Object visitBinaryExpr(const Binary* expr) override {
        Object left = evaluate(expr->left.get());
        Object right = evaluate(expr->right.get());

        switch (expr->op.type) {
            // --- Arithmetic Operators ---
            case TokenType::MINUS:
                checkNumberOperands(expr->op, left, right);
                return std::get<double>(left) - std::get<double>(right);
                
            case TokenType::SLASH:
                checkNumberOperands(expr->op, left, right);
                if (std::get<double>(right) == 0.0) {
                    throw std::runtime_error("Runtime Error: Division by zero.");
                }
                return std::get<double>(left) / std::get<double>(right);
                
            case TokenType::STAR:
                checkNumberOperands(expr->op, left, right);
                return std::get<double>(left) * std::get<double>(right);
                
            case TokenType::PLUS:
                // Handle number addition
                if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right)) {
                    return std::get<double>(left) + std::get<double>(right);
                }
                // Handle string concatenation
                if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
                    return std::get<std::string>(left) + std::get<std::string>(right);
                }
                throw std::runtime_error("Runtime Error: Operands must be two numbers or two strings for '+'.");

            // --- Comparison Operators ---
            case TokenType::GREATER:
                checkNumberOperands(expr->op, left, right);
                return std::get<double>(left) > std::get<double>(right);
                
            case TokenType::GREATER_EQUAL:
                checkNumberOperands(expr->op, left, right);
                return std::get<double>(left) >= std::get<double>(right);
                
            case TokenType::LESS:
                checkNumberOperands(expr->op, left, right);
                return std::get<double>(left) < std::get<double>(right);
                
            case TokenType::LESS_EQUAL:
                checkNumberOperands(expr->op, left, right);
                return std::get<double>(left) <= std::get<double>(right);

            // --- Task 36 & 41: Logical Operators ---
            case TokenType::AMPERSAND: {
                // Short-circuit: If left is false, the whole thing is false
                if (!isTruthy(left)) return false;
                return isTruthy(right);
            }
                
            case TokenType::PIPE: {
                // Short-circuit: If left is true, the whole thing is true
                if (isTruthy(left)) return true;
                return isTruthy(right);
            }

            // --- Equality Operators ---
            case TokenType::BANG_EQUAL: 
                return left != right; // std::variant handles direct comparisons natively!
            case TokenType::EQUAL_EQUAL: 
                return left == right;

            default:
                break;
        }

        return std::monostate{};
    }
    // Task 40: Evaluate Unary Expressions (- and !)
    Object visitUnaryExpr(const Unary* expr) override {
        // Evaluate the right-hand operand first
        Object right = evaluate(expr->right.get());

        switch (expr->op.type) {
            case TokenType::MINUS:
                // Check if the operand is a number before negating
                if (std::holds_alternative<double>(right)) {
                    return -std::get<double>(right);
                }
                throw std::runtime_error("Operand must be a number for unary minus.");
                
            case TokenType::BANG:
                // Flip the truthiness value of the operand
                return !isTruthy(right);
                
            default:
                break;
        }

        return std::monostate{};
    }

    // Task 49 / Task 52: Dynamic Instantiation Engine
    Object visitCallExpr(const Call* expr) override {
        Object callee = evaluate(expr->callee.get());

        // 1. Struct Instantiation (Task 49)
        if (std::holds_alternative<std::shared_ptr<StructBlueprint>>(callee)) {
            auto blueprint = std::get<std::shared_ptr<StructBlueprint>>(callee);
            return std::make_shared<StructInstance>(blueprint);
        }
        // 2. Function Execution (Task 52)
        else if (std::holds_alternative<std::shared_ptr<RuntimeFunction>>(callee)) {
            auto function = std::get<std::shared_ptr<RuntimeFunction>>(callee);
            
            // Check Arity (Argument count)
            if (expr->arguments.size() != function->declaration->params.size()) {
                throw std::runtime_error("Expected " + std::to_string(function->declaration->params.size()) + 
                                         " arguments but got " + std::to_string(expr->arguments.size()) + ".");
            }

            // Evaluate all arguments
            std::vector<Object> args;
            for (const auto& argExpr : expr->arguments) {
                args.push_back(evaluate(argExpr.get()));
            }

            // Create a local isolated environment for the function parameters
            auto callEnv = std::make_shared<Environment>(function->closure);
            for (size_t i = 0; i < function->declaration->params.size(); ++i) {
                callEnv->define(function->declaration->params[i].lexeme, args[i]);
            }

            // Execute the function body
            std::shared_ptr<Environment> previous = this->environment;
            this->environment = callEnv;
            
            try {
                execute(function->declaration->body.get());
            } catch (ReturnException& returnValue) {
                // We caught a return jump! Restore environment and return the value.
                this->environment = previous; 
                return returnValue.value;
            } catch (...) {
                this->environment = previous;
                throw;
            }
            
            this->environment = previous;
            return std::monostate{}; // Default to null if no return statement is hit
        }// 3. Native C++ Function Execution (Task 53)
        else if (std::holds_alternative<std::shared_ptr<NativeFunction>>(callee)) {
            auto nativeFn = std::get<std::shared_ptr<NativeFunction>>(callee);
            
            if (expr->arguments.size() != nativeFn->arity) {
                throw std::runtime_error("Line " + std::to_string(expr->paren.line) + 
                                         ": Expected " + std::to_string(nativeFn->arity) + 
                                         " arguments but got " + std::to_string(expr->arguments.size()) + ".");
            }

            // Evaluate all arguments
            std::vector<Object> args;
            for (const auto& argExpr : expr->arguments) {
                args.push_back(evaluate(argExpr.get()));
            }

            // Call the underlying C++ logic!
            return nativeFn->callable(args);
        }

        throw std::runtime_error("Can only call functions and structs.");
    }

    Object visitLiteralExpr(const Literal* expr) override {
        return expr->value; 
    }
    Object visitGroupingExpr(const Grouping* expr) override {
        return evaluate(expr->expression.get());
    }
    // Task 50: Dynamic Struct Field Access via the Arrow Operator (->)
    Object visitStructAccessExpr(const StructAccess* expr) override {
        // 1. Fully evaluate the base object expression (e.g., cell, vehicle_ptr, etc.)
        Object object = evaluate(expr->object.get());

        // 2. Dynamically check if the runtime variant holds a heap-allocated StructInstance
        if (std::holds_alternative<std::shared_ptr<StructInstance>>(object)) {
            auto instance = std::get<std::shared_ptr<StructInstance>>(object);
            
            // Critical runtime safeguard against null pointer dereference errors
            if (!instance) {
                throw std::runtime_error("Line " + std::to_string(expr->property.line) + 
                                         ": Runtime Error: Attempted to read field '" + 
                                         expr->property.lexeme + "' from a null instance pointer.");
            }

            // 3. Delegate the lookup purely to the specific instance instance's lookup dictionary
            return instance->get(expr->property);
        }

        // Catch improper type usages (e.g., trying to do 5->speed or "hello"->property)
        throw std::runtime_error("Line " + std::to_string(expr->property.line) + 
                                 ": Runtime Error: Only struct instances can contain properties. Invalid arrow access.");
    }

    // Task 51: Evaluate Struct Field Assignment (writing new values using ->)
    Object visitStructSetExpr(const StructSet* expr) override {
        // 1. Evaluate the base object expression
        Object object = evaluate(expr->object.get());

        // 2. Ensure it actually contains a heap-allocated StructInstance
        if (std::holds_alternative<std::shared_ptr<StructInstance>>(object)) {
            auto instance = std::get<std::shared_ptr<StructInstance>>(object);
            if (!instance) {
                throw std::runtime_error("Line " + std::to_string(expr->property.line) + 
                                         ": Cannot assign property '" + expr->property.lexeme + 
                                         "' to a null object reference.");
            }

            // 3. Evaluate the new right-hand value
            Object value = evaluate(expr->value.get());

            // 4. Save it into the instance's isolated field map
            instance->set(expr->property, value);
            
            // 5. In C++, assignments evaluate to the assigned value (e.g., a = b = c)
            return value;
        }

        throw std::runtime_error("Line " + std::to_string(expr->property.line) + 
                                 ": Only struct instances have assignable fields.");
    }

    // Task 44 & 49: Variable Lookups and Instantiations
    Object visitVariableExpr(const Variable* expr) override {
        return environment->get(expr->name); // Look up variables cleanly!
    }

    // Task 45: Implement assignment to modify existing Environment variables
    Object visitAssignExpr(const Assign* expr) override {
        // 1. Evaluate the right-hand side expression value
        Object value = evaluate(expr->value.get());

        // 2. Update the variable inside our active environment chain
        environment->assign(expr->name, value);

        // 3. Return the value (allows cascading assignments like: a = b = 10)
        return value;
    }

    // Helper to determine truthiness
    bool isTruthy(const Object& object) {
        if (std::holds_alternative<std::monostate>(object)) return false;
        if (std::holds_alternative<bool>(object)) return std::get<bool>(object);
        return true; // Numbers and strings are implicitly truthy
    }

    // Helper to validate that both operands are doubles
    void checkNumberOperands(const Token& op, const Object& left, const Object& right) {
        if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right)) return;
        throw std::runtime_error("Line " + std::to_string(op.line) + ": Operands must be numbers.");
    }

    // Task 46: Execute a block of statements within a bounded lexical environment scope
    void executeBlock(const std::vector<std::unique_ptr<Stmt>>& statements, 
                      std::shared_ptr<Environment> nextEnvironment) {
        // 1. Cache the current active environment scope
        std::shared_ptr<Environment> previous = this->environment;
        
        try {
            // 2. Pivot the interpreter to use the new nested environment
            this->environment = nextEnvironment;

            // 3. Process the sequential statements inside the local block scope
            for (const auto& statement : statements) {
                execute(statement.get());
            }
        } catch (...) {
            // 4. CRITICAL: Always restore the outer environment scope even if an error occurs
            this->environment = previous;
            throw; 
        }

        // 5. Restore the original outer environment scope upon safe completion
        this->environment = previous;
    }

    // Task 46 Visitor: Handle Block Statements [ ]
    void visitBlockStmt(const Block* stmt) override {
        // Spawn a brand new local scope linked back to the enclosing outer environment
        auto localEnv = std::make_shared<Environment>(this->environment);
        executeBlock(stmt->statements, localEnv);
    }

    // Task 48 Visitor: Evaluate While Loops (Drives desugared for-loops!)
    void visitWhileStmt(const WhileStmt* stmt) override {
        // Keep looping as long as the condition evaluates to a truthy variant state
        while (isTruthy(evaluate(stmt->condition.get()))) {
            execute(stmt->body.get());
        }
    }

    // Task 52 & 54: Register the function and CAPTURE the closure
    void visitFunctionStmt(const FunctionStmt* stmt) override {
        // Pass 'this->environment' to freeze a snapshot of the current scope
        auto function = std::make_shared<RuntimeFunction>(stmt, this->environment);
        environment->define(stmt->name.lexeme, function);
    }

    // Evaluates the return value and THROWS it up the C++ call stack to escape the block
    void visitReturnStmt(const ReturnStmt* stmt) override {
        Object value = std::monostate{};
        if (stmt->value != nullptr) {
            value = evaluate(stmt->value.get());
        }
        throw ReturnException(value); // Unwind!
    }
};