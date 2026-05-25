#pragma once
#include "Expr.h"
#include <string>
#include <sstream>

class AstPrinter {
public:
    // This is the main function we will call from main.cpp
    std::string print(const Expr* expr) {
        if (!expr) return "NULL";

        // Is it a Binary node? (e.g., a + b)
        if (const Binary* b = dynamic_cast<const Binary*>(expr)) {
            return parenthesize(b->op.lexeme, b->left.get(), b->right.get());
        }
        
        // Is it a Unary node? (e.g., !a or -a)
        if (const Unary* u = dynamic_cast<const Unary*>(expr)) {
            return parenthesize(u->op.lexeme, u->right.get());
        }
        
        // Is it a Grouping node? (e.g., (a + b))
        if (const Grouping* g = dynamic_cast<const Grouping*>(expr)) {
            return parenthesize("group", g->expression.get());
        }

        // Is it a Struct Access node? (e.g., my_car->speed)
        if (const StructAccess* s = dynamic_cast<const StructAccess*>(expr)) {
            // We print it as: (access speed my_car)
            return parenthesize("access " + s->property.lexeme, s->object.get());
        }

        // Is it a Variable node? (e.g., my_car)
        if (const Variable* v = dynamic_cast<const Variable*>(expr)) {
            return v->name.lexeme; // Just print the variable's name
        }

        // Is it a Literal value? (e.g., 100, "hello", true)
        if (const Literal* l = dynamic_cast<const Literal*>(expr)) {
            // We have to crack open the std::variant to print the right type
            if (std::holds_alternative<double>(l->value)) {
                return std::to_string(std::get<double>(l->value));
            }
            if (std::holds_alternative<std::string>(l->value)) {
                return std::get<std::string>(l->value);
            }
            if (std::holds_alternative<bool>(l->value)) {
                return std::get<bool>(l->value) ? "true" : "false";
            }
            return "null";
        }

        return "UNKNOWN_EXPR";
    }

private:
    // Helper to wrap Binary nodes in parentheses: (operator left right)
    std::string parenthesize(const std::string& name, const Expr* left, const Expr* right) {
        return "(" + name + " " + print(left) + " " + print(right) + ")";
    }

    // Helper to wrap Unary/Grouping nodes in parentheses: (operator right)
    std::string parenthesize(const std::string& name, const Expr* expr) {
        return "(" + name + " " + print(expr) + ")";
    }
};