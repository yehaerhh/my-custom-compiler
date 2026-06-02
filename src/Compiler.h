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
    // Maps a Struct Name -> (Property Name -> Memory Offset in bytes)
std::unordered_map<std::string, std::unordered_map<std::string, uint16_t>> structBlueprints;
// Add to the top of Compiler.h
std::unordered_map<std::string, uint16_t> globalPropertyOffsets;
class Compiler : public ExprVisitor, public StmtVisitor {
public:
    // --- TASK 88: THE ASSEMBLY OUTPUT ---
    std::vector<std::string> assemblyOutput;
    int currentLine = 0;
    int labelCounter = 0; // Used to generate unique labels like "end_if_1"

    // --- TASK 89: NAIVE REGISTER ALLOCATOR ---
    int nextReg = 0;
    std::string lastResultReg = ""; // Tracks which register holds the latest calculation

    std::string allocateReg() {
        if (nextReg > 7) { 
            std::cerr << "Compile Error: Out of registers!\n"; 
            return "R7"; 
        }
        return "R" + std::to_string(nextReg++);
    }

    void freeReg() {
        if (nextReg > 0) nextReg--;
    }

    // --- NAIVE MEMORY MAPPER ---
    uint16_t nextRamAddress = 0x8000; // General RAM starts here!
    std::unordered_map<std::string, uint16_t> variables;

    // The new emit helper!
    void emitLine(const std::string& line) {
        assemblyOutput.push_back(line);
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

        std::string elseLabel = "else_" + std::to_string(labelCounter);
        std::string endLabel = "end_if_" + std::to_string(labelCounter++);

        // Our condition (e.g. x == 5) sets the CPU flags.
        // If it was false (Zero flag = 0), we jump to the else block
        emitLine("    JNE " + elseLabel); 
        
        freeReg(); // Free the condition result

        stmt->thenBranch->accept(this);
        emitLine("    JMP " + endLabel);

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
        // Look up the physical RAM address of the variable
        uint16_t addr = variables[expr->name.lexeme];
        
        std::string addrReg = allocateReg();
        std::string destReg = allocateReg();

        // Load the address, then load the value from RAM into our destination register
        emitLine("    LOAD_ADDR " + addrReg + " " + std::to_string(addr));
        emitLine("    LOAD " + destReg + " " + addrReg); // LOAD dest, [addr]

        // Free the temporary address register, keep the destination register active
        freeReg(); 
        lastResultReg = destReg;
        
        return std::monostate{};
    }

    Object visitBinaryExpr(const Binary* expr) override {
        // 1. Calculate the Left side
        expr->left->accept(this);
        std::string leftReg = lastResultReg;

        // 2. Calculate the Right side
        expr->right->accept(this);
        std::string rightReg = lastResultReg;

        // 3. Free the two source registers so we can reuse them!
        freeReg(); 
        freeReg(); 

        // 4. Allocate a new register for the final result
        std::string destReg = allocateReg();

        switch (expr->op.type) {
            case TokenType::PLUS:  
                emitLine("    ADD " + destReg + " " + leftReg + " " + rightReg); 
                break;
            case TokenType::MINUS: 
                emitLine("    SUB " + destReg + " " + leftReg + " " + rightReg); 
                break;
            case TokenType::STAR:  
                emitLine("    MUL " + destReg + " " + leftReg + " " + rightReg); 
                break;
            case TokenType::SLASH: 
                emitLine("    DIV " + destReg + " " + leftReg + " " + rightReg); 
                break;
            case TokenType::EQUAL_EQUAL:   
            case TokenType::LESS:
            case TokenType::GREATER:
                // Compare the two registers (Updates the CPU FLAGS!)
                emitLine("    CMP " + leftReg + " " + rightReg); 
                break;
            default: break;
        }

        lastResultReg = destReg;
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
        std::string loopEnd = "loop_end_" + std::to_string(labelCounter++);

        emitLine(loopStart + ":");
        stmt->condition->accept(this); // Evaluate condition

        emitLine("    JNE " + loopEnd); // Jump if false
        freeReg(); 

        stmt->body->accept(this); // Run the loop body
        emitLine("    JMP " + loopStart); // Jump back to top

        emitLine(loopEnd + ":");
    }
    
    void visitStructDeclStmt(const StructDecl* stmt) override {
        // We don't generate any CPU code here! We just create a blueprint.
        std::unordered_map<std::string, uint16_t> offsets;
        uint16_t currentOffset = 0;

        // Assuming your StructDecl has a list of properties/fields (adjust names as per your AST)
        /* for (const auto& method : stmt->methods) { // Or stmt->fields
            offsets[method->name.lexeme] = currentOffset;
            currentOffset += 2; // Each 16-bit property takes 2 bytes
        }
        */
        
        // Save the blueprint in the compiler's memory
        // structBlueprints[stmt->name.lexeme] = offsets;
        
        // Add a comment to the assembly output for debugging
        emitLine("    ; Struct Blueprint compiled: " + stmt->name.lexeme);
    }
    void visitFunctionStmt(const FunctionStmt* stmt) override {
        std::string funcLabel = stmt->name.lexeme;
        std::string endLabel = "end_func_" + stmt->name.lexeme;

        // 1. Jump OVER the function code so it doesn't execute linearly
        emitLine("    JMP " + endLabel);

        // 2. Mark the entry point for the CALL instruction to land on
        emitLine(funcLabel + ":");

        // 3. Compile the inside of the function
        stmt->body->accept(this);

        // 4. Hardware return (Pops the PC off the stack)
        emitLine("    RET");

        // 5. Mark the end of the jump
        emitLine(endLabel + ":");
    }
    void visitReturnStmt(const ReturnStmt* stmt) override {
        if (stmt->value != nullptr) {
            // Calculate the return value
            stmt->value->accept(this);
            
            // Move the result into R0 (The official return register)
            emitLine("    MOV_REG R0 " + lastResultReg);
            
            // Free the temporary register used for the calculation
            freeReg(); 
        }
        
        // Pop the stack and go back to the caller
        emitLine("    RET");
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
        // Find out the name of the function being called
        std::string funcName = "";
        if (auto var = dynamic_cast<Variable*>(expr->callee.get())) {
            funcName = var->name.lexeme;
        } else {
            std::cerr << "Compile Error: Can only call named functions directly.\n";
            return std::monostate{};
        }

        // 1. Allocate a temporary register to hold the function's memory address
        std::string addrReg = allocateReg();
        
        // 2. Load the label (Assembler Pass 2 will replace this with a number)
        emitLine("    LOAD_ADDR " + addrReg + " " + funcName);
        
        // 3. Fire the hardware CALL opcode (Pushes current PC to stack, jumps to addrReg)
        emitLine("    CALL " + addrReg);
        
        // 4. The function has returned! Grab the answer from R0.
        std::string resultReg = allocateReg();
        emitLine("    MOV_REG " + resultReg + " R0");

        // Clean up the address register, but keep the result register active!
        freeReg(); 
        lastResultReg = resultReg;

        return std::monostate{};
    }
    Object visitStructAccessExpr(const StructAccess* expr) override {
        // 1. Compile the object (e.g., "player"). 
        // NOTE: Your visitVariableExpr needs to be slightly tweaked to return 
        // the BASE ADDRESS of the struct, not the value, if it's an object!
        expr->object->accept(this);
        std::string baseAddrReg = lastResultReg;

        // 2. Look up the byte offset for this property (e.g., "y" = 2)
        uint16_t offset = globalPropertyOffsets[expr->property.lexeme];

        // 3. Load the offset into a register
        std::string offsetReg = allocateReg();
        emitLine("    MOV_IMM " + offsetReg + " " + std::to_string(offset));

        // 4. Do the Pointer Math: Base Address + Offset
        std::string finalAddrReg = allocateReg();
        emitLine("    ADD " + finalAddrReg + " " + baseAddrReg + " " + offsetReg);

        // 5. Load the actual value from that calculated RAM address!
        std::string destReg = allocateReg();
        emitLine("    LOAD " + destReg + " " + finalAddrReg);

        // Clean up all the math registers!
        freeReg(); // Free finalAddrReg
        freeReg(); // Free offsetReg
        freeReg(); // Free baseAddrReg

        lastResultReg = destReg;
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