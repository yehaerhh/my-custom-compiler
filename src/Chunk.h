#pragma once
#include <vector>
#include <cstdint>
#include "Expr.h" // We reuse your Object variant for our constant pool!

// We will expand this in Task 56, but we need at least one opcode to start
// Task 56: The Instruction Set Architecture (ISA) for our VM
enum OpCode {
    OP_CONSTANT,  // Load a constant value (like 1.2 or "hello") from the pool
    OP_ADD,       // a + b
    OP_SUBTRACT,  // a - b
    OP_MULTIPLY,  // a * b
    OP_DIVIDE,    // a / b
    OP_NEGATE,    // -
    OP_POP,
    OP_PRINT,   // <--- ADD THIS
    OP_CALL,
    OP_RETURN,     // Exit current execution
    // Task 62: Global Variables
    OP_DEFINE_GLOBAL, // Creates a new variable
    OP_GET_GLOBAL,    // Reads a variable and pushes it to the stack
    OP_SET_GLOBAL,     // Updates an existing variable
    // Task 63: Local Variables
    OP_GET_LOCAL,     // Loads a value from a specific stack index
    OP_SET_LOCAL,      // Overwrites a value at a specific stack index    
    // Task 64: Control Flow
    OP_JUMP_IF_FALSE, // Jump over the 'then' block if the condition is false
    OP_JUMP,           // Unconditional jump (used to jump over the 'else' block) 
    OP_LOOP,
    // Task 66b: Comparisons
    OP_LESS,
    OP_GREATER,
    OP_EQUAL,
    // Task 67: Structs
    OP_ALLOC_STRUCT, // Creates a blank struct instance in memory
    OP_GET_FIELD,    // Reads a property
    OP_SET_FIELD     // Writes a property
};

class Chunk {
public:
    std::vector<uint8_t> code;     // The actual bytecode instructions
    std::vector<Object> constants; // The pool of static values (numbers, strings)
    std::vector<int> lines;        // Line numbers corresponding to each byte

    // Write a new byte (instruction or operand) to the chunk
    void writeChunk(uint8_t byte, int line) {
        code.push_back(byte);
        lines.push_back(line);
    }

    // Add a constant value to the pool and return its index so the bytecode can find it
    int addConstant(Object value) {
        constants.push_back(value);
        return constants.size() - 1; // Return the index of the newly added constant
    }
};