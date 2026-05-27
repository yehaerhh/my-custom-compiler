#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <stdexcept>
#include "Token.h"
#include "Expr.h" // Pulls in our runtime Object variant definition

class Environment : public std::enable_shared_from_this<Environment> {
private:
    std::unordered_map<std::string, Object> values;
    std::shared_ptr<Environment> enclosing; // Pointer to the outer scope

public:
    // Global scope constructor (no outer environment)
    Environment() : enclosing(nullptr) {}

    // Local scope constructor (nested inside an outer environment)
    explicit Environment(std::shared_ptr<Environment> enclosing) 
        : enclosing(std::move(enclosing)) {}

    // 1. Define a brand new variable (e.g., let a = 5;)
    void define(const std::string& name, const Object& value) {
        values[name] = value;
    }

    // 2. Lookup a variable's value (e.g., print a;)
    Object get(const Token& name) {
        // If it exists in the current local scope, return it
        if (values.find(name.lexeme) != values.end()) {
            return values[name.lexeme];
        }

        // If it's not here, search up the chain into the enclosing outer scopes
        if (enclosing != nullptr) {
            return enclosing->get(name);
        }

        // If we hit the top-level global scope and still haven't found it, panic!
        throw std::runtime_error("Line " + std::to_string(name.line) + 
                                 ": Undefined variable '" + name.lexeme + "'.");
    }

    // 3. Assign/Modify an existing variable (e.g., a = 6;)
    void assign(const Token& name, const Object& value) {
        if (values.find(name.lexeme) != values.end()) {
            values[name.lexeme] = value;
            return;
        }

        // If it's not in our current block, try mutating it in the outer scope layers
        if (enclosing != nullptr) {
            enclosing->assign(name, value);
            return;
        }

        throw std::runtime_error("Line " + std::to_string(name.line) + 
                                 ": Undefined variable assignment target '" + name.lexeme + "'.");
    }
};