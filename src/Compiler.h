#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <variant>
#include "Expr.h"
#include "Stmt.h"

// Add this to the top of Compiler.h!

class Compiler : public ExprVisitor, public StmtVisitor {
public:
    // --- ADD THESE MAPS HERE ---
    // 1. Heavy Associative Maps & Vectors First
    std::unordered_map<std::string, uint16_t> structSizes;
    std::unordered_map<std::string, uint16_t> globalPropertyOffsets;
    std::unordered_map<std::string, uint16_t> variables;
    std::vector<std::string> assemblyOutput;
    std::vector<Token> currentFunctionParams;
    std::string lastResultReg = "";

    // 2. Medium Enums
    TokenType lastCompareOp = TokenType::EQUAL_EQUAL;

    // 3. 32-bit Integers
    int currentLine = 0;
    int labelCounter = 0;
    int nextReg = 0;

    // 4. Smallest Primitives packed at the very bottom
    uint16_t nextRamAddress = 0x8000;
    // FIX: Instant zero-allocation register lookup
    std::string allocateReg() {
        if (nextReg > 7) { 
            std::cerr << "Compile Error: Out of registers!\n"; 
            return "R7"; 
        }
        static const std::string regNames[] = {"R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7"};
        return regNames[nextReg++];
    }

    void freeReg() {
        if (nextReg > 0) nextReg--;
    }

    // The new emit helper!
    void emitLine(const std::string& line) {
        assemblyOutput.push_back(std::move(line));
    }

    // The main entry point for the Compiler
    void compile(const std::vector<std::unique_ptr<Stmt>>& statements) {
        for (const auto& stmt : statements) {
            stmt->accept(this);
        }
        // Tell the hardware CPU to stop!
        emitLine("    HALT");
    }

    // --- STATEMENT COMPILATION ---

    void visitVarStmt(const VarStmt* stmt) override {
        currentLine = stmt->name.line;
        
        // 1. Assign this variable a permanent physical address in RAM
        variables[stmt->name.lexeme] = nextRamAddress;
        nextRamAddress += 2; // Move forward 2 bytes for the next variable

        // 2. Compile the right side of the equals sign
        if (stmt->initializer != nullptr) {
            stmt->initializer->accept(this); 
        } else {
            // Default to 0 if no initializer
            std::string zeroReg = allocateReg();
            emitLine("    LOAD_ADDR " + zeroReg + " 0");
            lastResultReg = zeroReg;
        }

        // 3. Store the calculation result into the RAM address!
        std::string addrReg = allocateReg();
        emitLine("    LOAD_ADDR " + addrReg + " " + std::to_string(variables[stmt->name.lexeme]));
        emitLine("    STORE " + addrReg + " " + lastResultReg); // STORE [addr], result
        
        freeReg(); // Free the address register
        freeReg(); // Free the result register
    }

    void visitPrintStmt(const PrintStmt* stmt) override {
        // Compile the expression
        stmt->expression->accept(this);
        
        // MMIO MAGIC! Load the terminal screen address (0xF000 = 61440)
        std::string addrReg = allocateReg();
        emitLine("    LOAD_ADDR " + addrReg + " 61440");
        
        // Store the calculation result into the screen address to print it!
        emitLine("    STORE " + addrReg + " " + lastResultReg);
        
        freeReg();
        freeReg();
    }

    void visitExpressionStmt(const ExpressionStmt* stmt) override {
        stmt->expression->accept(this);
        freeReg(); // We don't need the result, so just free the register!
    }

    void visitIfStmt(const IfStmt* stmt) override {
        stmt->condition->accept(this);

        std::string trueLabel = "if_true_" + std::to_string(labelCounter);
        std::string elseLabel = "else_" + std::to_string(labelCounter);
        std::string endLabel = "end_if_" + std::to_string(labelCounter++);

        // --- NEW: CONDITION CODE ROUTING (The Trampoline) ---
        if (lastCompareOp == TokenType::EQUAL_EQUAL) {
            emitLine("    JEQ " + trueLabel);
        } else if (lastCompareOp == TokenType::LESS) {
            emitLine("    JLT " + trueLabel);
        } else if (lastCompareOp == TokenType::GREATER) {
            emitLine("    JGT " + trueLabel);
        }
        
        // If the condition failed, fall through and jump to the else block!
        emitLine("    JMP " + elseLabel);
        // ----------------------------------------------------

        freeReg(); // Free the condition result

        // The TRUE block
        emitLine(trueLabel + ":");
        stmt->thenBranch->accept(this);
        emitLine("    JMP " + endLabel);

        // The FALSE / ELSE block
        emitLine(elseLabel + ":");
        if (stmt->elseBranch != nullptr) {
            stmt->elseBranch->accept(this);
        }

        emitLine(endLabel + ":");
    }

    // --- EXPRESSION COMPILATION (The Math Engine) ---

    Object visitLiteralExpr(const Literal* expr) override {
        // Grab a fresh register, load the number into it, and tell the rest of the compiler where it is
        std::string destReg = allocateReg();
        
        // For simplicity, assuming the literal is a double that we convert to int
        int value = 0;
        if (std::holds_alternative<double>(expr->value)) {
            value = (int)std::get<double>(expr->value);
        }

        emitLine("    LOAD_ADDR " + destReg + " " + std::to_string(value));
        lastResultReg = destReg;
        return std::monostate{};
    }

    Object visitVariableExpr(const Variable* expr) override {
        // 1. Look up the physical RAM address of the variable
        uint16_t addr = variables[expr->name.lexeme];
        
        // 2. Allocate just ONE register to do all the work
        std::string reg = allocateReg();

        // 3. Load the 16-bit address into our register
        emitLine("    LOAD_ADDR " + reg + " " + std::to_string(addr));
        
        // 4. Overwrite the register with the actual data value stored at that address
        emitLine("    LOAD " + reg + " " + reg); // LOAD reg, [reg]

        // 5. Tell the compiler where the result is. Do NOT call freeReg() here!
        lastResultReg = reg;
        
        return std::monostate{};
    }

    Object visitBinaryExpr(const Binary* expr) override {
        // 1. Calculate the Left side
        expr->left->accept(this);
        
        // --- NEW: PROTECT THE LEFT SIDE! ---
        // Push it to the hardware stack so the right side cannot destroy it!
        emitLine("    PUSH " + lastResultReg); 
        freeReg(); // Free the register so the right side has all CPU registers available
        // -----------------------------------

        // 2. Calculate the Right side
        expr->right->accept(this);
        std::string rightReg = lastResultReg;

        // --- NEW: RESTORE THE LEFT SIDE! ---
        // Allocate a new register and Pop the left side back into it
        std::string leftReg = allocateReg();
        emitLine("    POP " + leftReg);
        // -----------------------------------

        // 3. Do the actual math (Saving the result directly into leftReg)
        switch (expr->op.type) {
            case TokenType::PLUS:  
                emitLine("    ADD " + leftReg + " " + leftReg + " " + rightReg); 
                break;
            case TokenType::MINUS: 
                emitLine("    SUB " + leftReg + " " + leftReg + " " + rightReg); 
                break;
            case TokenType::STAR:  
                emitLine("    MUL " + leftReg + " " + leftReg + " " + rightReg); 
                break;
            case TokenType::SLASH: 
                emitLine("    DIV " + leftReg + " " + leftReg + " " + rightReg); 
                break;
            case TokenType::EQUAL_EQUAL:   
            case TokenType::LESS:
            case TokenType::GREATER:
                // Compare the two registers (Updates the CPU FLAGS!)
                emitLine("    CMP " + leftReg + " " + rightReg); 
                lastCompareOp = expr->op.type;
                break;
            default: break;
        }

        // 4. LIFO CLEANUP!
        // Right now, leftReg is on TOP of rightReg in the allocator stack.
        // We move our final answer down into rightReg, and free leftReg!
        if (expr->op.type != TokenType::EQUAL_EQUAL && 
            expr->op.type != TokenType::LESS && 
            expr->op.type != TokenType::GREATER) {
            emitLine("    MOV_REG " + rightReg + " " + leftReg);
        }
        
        freeReg(); // Frees leftReg

        // Keep the final answer locked in rightReg for the parent statement!
        lastResultReg = rightReg; 
        return std::monostate{};
    }

    // Stubs to keep the class compiling
    void visitBlockStmt(const Block* stmt) override {
        // Just loop through all statements inside the { } and compile them!
        for (const auto& statement : stmt->statements) {
            statement->accept(this);
        }
    }
    void visitWhileStmt(const WhileStmt* stmt) override {
        std::string loopStart = "loop_start_" + std::to_string(labelCounter);
        std::string loopBody = "loop_body_" + std::to_string(labelCounter);
        std::string loopEnd = "loop_end_" + std::to_string(labelCounter++);

        // 1. Drop the loop start anchor label
        emitLine(loopStart + ":");
        
        // 2. Evaluate condition (Sets CPU flags and updates lastCompareOp)
        stmt->condition->accept(this); 

        // --- NEW: CONDITION CODE ROUTING (The Trampoline) ---
        if (lastCompareOp == TokenType::EQUAL_EQUAL) {
            emitLine("    JEQ " + loopBody);
        } else if (lastCompareOp == TokenType::LESS) {
            emitLine("    JLT " + loopBody);
        } else if (lastCompareOp == TokenType::GREATER) {
            emitLine("    JGT " + loopBody);
        }
        
        // If the condition fails, jump completely out of the loop
        emitLine("    JMP " + loopEnd);
        // ----------------------------------------------------
        
        freeReg(); 

        // 3. The LOOP block
        emitLine(loopBody + ":");
        stmt->body->accept(this); 
        
        // 4. Jump back to evaluate the condition again
        emitLine("    JMP " + loopStart); 

        // 5. Drop the loop exit anchor label
        emitLine(loopEnd + ":");
    }
    void visitFunctionStmt(const FunctionStmt* stmt) override {
        std::string funcLabel = stmt->name.lexeme;
        std::string endLabel = "end_func_" + stmt->name.lexeme;

        emitLine("    JMP " + endLabel);
        emitLine(funcLabel + ":");

        currentFunctionParams = stmt->params;
        
        if (!stmt->params.empty()) {
            // 1. Move the Return Address out of the way!
            std::string retAddr = allocateReg();
            emitLine("    POP " + retAddr);

            // 2. Pop arguments from stack into temporary registers
            std::vector<std::string> argRegs;
            for (int i = stmt->params.size() - 1; i >= 0; i--) {
                std::string r = allocateReg();
                emitLine("    POP " + r);
                argRegs.push_back(r);
            }

            // 3. Back up the Caller's global variables to the stack
            for (const auto& param : stmt->params) {
                if (variables.find(param.lexeme) == variables.end()) {
                    variables[param.lexeme] = nextRamAddress;
                    nextRamAddress += 2;
                }
                std::string addrReg = allocateReg();
                emitLine("    LOAD_ADDR " + addrReg + " " + std::to_string(variables[param.lexeme]));
                emitLine("    LOAD " + addrReg + " " + addrReg);
                emitLine("    PUSH " + addrReg);
                freeReg();
            }

            // 4. Put the Return Address back on top so RET doesn't break!
            emitLine("    PUSH " + retAddr);

            // 5. Store the passed arguments into the local variables
            for (size_t i = 0; i < stmt->params.size(); i++) {
                std::string valReg = argRegs[stmt->params.size() - 1 - i];
                std::string addrReg = allocateReg();
                emitLine("    LOAD_ADDR " + addrReg + " " + std::to_string(variables[stmt->params[i].lexeme]));
                emitLine("    STORE " + addrReg + " " + valReg);
                freeReg();
            }

            // Clean up temporaries
            for (size_t i = 0; i < argRegs.size(); i++) freeReg();
            freeReg(); // Free retAddr
        }

        stmt->body->accept(this);

        currentFunctionParams.clear();
        emitLine("    RET");
        emitLine(endLabel + ":");
    }
    void visitReturnStmt(const ReturnStmt* stmt) override {
        if (stmt->value != nullptr) {
            stmt->value->accept(this);
            emitLine("    MOV_REG R0 " + lastResultReg);
            freeReg();
        }
        
        if (!currentFunctionParams.empty()) {
            std::string protectR0 = allocateReg(); // Lock R0!
            
            // 1. Move Return Address out of the way
            std::string retAddr = allocateReg();
            emitLine("    POP " + retAddr);

            // 2. Restore global variables from the stack
            for (int i = currentFunctionParams.size() - 1; i >= 0; i--) {
                std::string valReg = allocateReg();
                emitLine("    POP " + valReg);
                
                std::string addrReg = allocateReg();
                emitLine("    LOAD_ADDR " + addrReg + " " + std::to_string(variables[currentFunctionParams[i].lexeme]));
                emitLine("    STORE " + addrReg + " " + valReg);
                
                freeReg(); 
                freeReg(); 
            }

            // 3. Put Return Address back
            emitLine("    PUSH " + retAddr);
            
            freeReg(); // Free retAddr
            freeReg(); // Free protectR0
        }

        emitLine("    RET");
    }
    void visitStructDeclStmt(const StructDecl* stmt) override {
        uint16_t currentOffset = 0;
        
        // FIX: Loop through your 'properties' vector!
        for (const auto& prop : stmt->properties) { 
            // Map each property token's lexeme string to its memory byte offset (0, 2, 4, etc.)
            globalPropertyOffsets[prop.lexeme] = currentOffset;
            currentOffset += 2; 
        }
        
        // Save the total size of the struct so we know how much RAM to allocate later
        structSizes[stmt->name.lexeme] = currentOffset; 
    }
    Object visitGroupingExpr(const Grouping* expr) override {
        // Just visit the expression inside the parentheses!
        // The result will naturally be left in lastResultReg.
        expr->expression->accept(this);
        return std::monostate{};
    }
    Object visitUnaryExpr(const Unary* expr) override {
        // Calculate the value on the right side of the operator
        expr->right->accept(this); 
        std::string rightReg = lastResultReg;

        if (expr->op.type == TokenType::MINUS) {
            // Subtract the number from 0 to make it negative!
            std::string zeroReg = allocateReg();
            emitLine("    MOV_IMM " + zeroReg + " 0");
            
            std::string destReg = allocateReg();
            emitLine("    SUB " + destReg + " " + zeroReg + " " + rightReg);
            
            freeReg(); // Free the zero register
            freeReg(); // Free the source register
            lastResultReg = destReg;
        } 
        else if (expr->op.type == TokenType::BANG) { // Logical NOT (!)
            std::string destReg = allocateReg();
            // Using the bitwise NOT from your Task 79 ISA
            emitLine("    NOT " + destReg + " " + rightReg + " " + rightReg);
            
            freeReg(); 
            lastResultReg = destReg;
        }
        
        return std::monostate{};
    }
    Object visitAssignExpr(const Assign* expr) override {
        // 1. Calculate the new value (e.g., the "5" in x = 5)
        expr->value->accept(this); 
        std::string valReg = lastResultReg; 

        // 2. Look up where 'x' lives in physical RAM
        uint16_t addr = variables[expr->name.lexeme];
        
        // 3. Load that RAM address using our new 32-bit instruction!
        std::string addrReg = allocateReg();
        emitLine("    LOAD_ADDR " + addrReg + " " + std::to_string(addr)); 

        // 4. Overwrite the old RAM value with the new value
        emitLine("    STORE " + addrReg + " " + valReg);

        // Clean up
        freeReg(); 
        freeReg(); 
        
        return std::monostate{};
    }
    Object visitCallExpr(const Call* expr) override {
        // Find out the name of the function/struct being called
        std::string funcName = "";
        if (auto var = dynamic_cast<Variable*>(expr->callee.get())) {
            funcName = var->name.lexeme;
        } else {
            std::cerr << "Compile Error: Can only call named functions directly.\n";
            return std::monostate{};
        }

        // --- NEW: IS THIS A STRUCT INSTANTIATION? ---
        if (structSizes.find(funcName) != structSizes.end()) {
            // It's a struct! Grab its base address and reserve a chunk of RAM
            uint16_t baseAddr = nextRamAddress;
            nextRamAddress += structSizes[funcName]; 
            
            // Return the base pointer address in a safe register
            std::string resultReg = allocateReg();
            emitLine("    LOAD_ADDR " + resultReg + " " + std::to_string(baseAddr));
            lastResultReg = resultReg;
            
            return std::monostate{}; // Exit early, do NOT emit a CALL instruction!
        }
        // --------------------------------------------

        // --- NEW: PUSH ARGUMENTS TO THE HARDWARE STACK ---
        for (const auto& arg : expr->arguments) {
            arg->accept(this);
            emitLine("    PUSH " + lastResultReg); // Save argument to stack
            freeReg(); // Free the math register
        }
        // -------------------------------------------------

        // 1. Allocate a temporary register to hold the function's memory address
        std::string addrReg = allocateReg(); // Allocates R0
        
        // 2. Load the label (Assembler Pass 2 will replace this with a number)
        emitLine("    LOAD_ADDR " + addrReg + " " + funcName);
        
        // 3. Fire the hardware CALL opcode
        emitLine("    CALL " + addrReg);
        
        // FIX: Free the address register IMMEDIATELY here while it is at the top of the stack!
        freeReg(); // Frees R0. Allocator stack is clean and empty again.
        
        // 4. The function has returned! Grab the answer from hardware register R0.
        std::string resultReg = allocateReg(); // Allocates R0 safely!
        emitLine("    MOV_REG " + resultReg + " R0"); // MOV_REG R0 R0 (Safe operation/no-op)

        // Keep the result register active for the parent statement
        lastResultReg = resultReg; // Points to R0, which is safely locked.

        return std::monostate{};
    }
    Object visitStructAccessExpr(const StructAccess* expr) override {
        // 1. Compile the object to get the Base Address
        expr->object->accept(this);
        std::string baseAddrReg = lastResultReg; // e.g., Allocates R0

        // 2. Look up the byte offset
        uint16_t offset = globalPropertyOffsets[expr->property.lexeme];

        // 3. Allocate ONE temporary register for the offset
        std::string offsetReg = allocateReg(); // e.g., Allocates R1
        emitLine("    MOV_IMM " + offsetReg + " " + std::to_string(offset));

        // 4. Do the math and put the result back into baseAddrReg!
        emitLine("    ADD " + baseAddrReg + " " + baseAddrReg + " " + offsetReg);

        // 5. Load the actual value directly into baseAddrReg!
        emitLine("    LOAD " + baseAddrReg + " " + baseAddrReg);

        // 6. Free the offset register. 
        freeReg(); // Frees R1. R0 (baseAddrReg) remains locked safely!

        lastResultReg = baseAddrReg; 
        return std::monostate{};
    }
    Object visitStructSetExpr(const StructSet* expr) override {
        // 1. Compile the value we want to save (e.g., the "10")
        expr->value->accept(this);
        std::string valReg = lastResultReg;

        // 2. Compile the object to get its Base RAM Address pointer
        expr->object->accept(this);
        std::string baseAddrReg = lastResultReg;

        // 3. Get the byte offset for the property
        uint16_t offset = globalPropertyOffsets[expr->property.lexeme];

        // 4. Load the offset and do the Pointer Math
        std::string offsetReg = allocateReg();
        emitLine("    MOV_IMM " + offsetReg + " " + std::to_string(offset));

        std::string finalAddrReg = allocateReg();
        emitLine("    ADD " + finalAddrReg + " " + baseAddrReg + " " + offsetReg);

        // 5. Store the value into the calculated address!
        emitLine("    STORE " + finalAddrReg + " " + valReg);

        // Clean up EVERYTHING
        freeReg(); // Free finalAddrReg
        freeReg(); // Free offsetReg
        freeReg(); // Free baseAddrReg
        freeReg(); // Free valReg

        return std::monostate{};
    }
};