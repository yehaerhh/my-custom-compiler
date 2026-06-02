#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include "Chunk.h"

class Debugger {
public:
    static void disassembleChunk(Chunk* chunk, const std::string& name) {
        std::cout << "== " << name << " ==\n";
        
        for (int offset = 0; offset < chunk->code.size();) {
            // disassembleInstruction returns the next offset to process!
            offset = disassembleInstruction(chunk, offset);
        }
    }

    static int disassembleInstruction(Chunk* chunk, int offset) {
        // Print the byte offset index (e.g., 0000, 0001)
        std::cout << std::setfill('0') << std::setw(4) << offset << " ";

        // Print the source code line number (or a | if it's the same line as the previous instruction)
        if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
            std::cout << "   | ";
        } else {
            std::cout << std::setfill(' ') << std::setw(4) << chunk->lines[offset] << " ";
        }

        uint8_t instruction = chunk->code[offset];
        switch (instruction) {
            case OP_CONSTANT:
                return constantInstruction("OP_CONSTANT", chunk, offset);
            case OP_ADD:
                return simpleInstruction("OP_ADD", offset);
            case OP_SUBTRACT:
                return simpleInstruction("OP_SUBTRACT", offset);
            case OP_MULTIPLY:
                return simpleInstruction("OP_MULTIPLY", offset);
            case OP_DIVIDE:
                return simpleInstruction("OP_DIVIDE", offset);
            case OP_NEGATE:
                return simpleInstruction("OP_NEGATE", offset);
            case OP_POP:  // <--- ADD THIS
                return simpleInstruction("OP_POP", offset);
            case OP_RETURN:
                return simpleInstruction("OP_RETURN", offset);
            case OP_PRINT:
                return simpleInstruction("OP_PRINT", offset);
            case OP_GET_LOCAL:
                std::cout << std::left << std::setw(16) << "OP_GET_LOCAL" 
                          << " slot " << (int)chunk->code[offset + 1] << "\n";
                return offset + 2;
            case OP_SET_LOCAL:
                std::cout << std::left << std::setw(16) << "OP_SET_LOCAL" 
                          << " slot " << (int)chunk->code[offset + 1] << "\n";
                return offset + 2;
            case OP_DEFINE_GLOBAL:
                return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
            case OP_GET_GLOBAL:
                return constantInstruction("OP_GET_GLOBAL", chunk, offset);
            case OP_SET_GLOBAL:
                return constantInstruction("OP_SET_GLOBAL", chunk, offset);
            case OP_JUMP:
                return jumpInstruction("OP_JUMP", 1, chunk, offset);
            case OP_JUMP_IF_FALSE:
                return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
            case OP_LOOP: // <--- ADD THIS
                return jumpInstruction("OP_LOOP", -1, chunk, offset);
            case OP_LESS:    return simpleInstruction("OP_LESS", offset);
            case OP_GREATER: return simpleInstruction("OP_GREATER", offset);
            case OP_EQUAL:   return simpleInstruction("OP_EQUAL", offset);
            case OP_ALLOC_STRUCT:
                return constantInstruction("OP_ALLOC_STRUCT", chunk, offset);
            case OP_GET_FIELD:
                return constantInstruction("OP_GET_FIELD", chunk, offset);
            case OP_SET_FIELD:
                return constantInstruction("OP_SET_FIELD", chunk, offset);
            default:
                std::cout << "Unknown opcode " << (int)instruction << "\n";
                return offset + 1;
        }
    }

private:
    // For instructions that are just a single byte long (like OP_RETURN)
    static int simpleInstruction(const std::string& name, int offset) {
        std::cout << name << "\n";
        return offset + 1;
    }

    static int jumpInstruction(const std::string& name, int sign, Chunk* chunk, int offset) {
        // Reassemble the 16-bit number from the two bytes!
        uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
        jump |= chunk->code[offset + 2];
        
        // Print where the IP is jumping to
        std::cout << std::left << std::setw(16) << name << " " << offset 
                  << " -> " << offset + 3 + sign * jump << "\n";
        
        return offset + 3; // Jumps take up 3 bytes: Opcode + Byte1 + Byte2
    }

    // For instructions that have an operand/argument (like OP_CONSTANT <index>)
    static int constantInstruction(const std::string& name, Chunk* chunk, int offset) {
        // The index of the constant is stored in the byte immediately following the opcode
        uint8_t constant_index = chunk->code[offset + 1];
        
        std::cout << std::left << std::setw(16) << name << " " 
                  << std::right << std::setw(4) << (int)constant_index << " '";
        
        // Print the actual value stored in the constants pool safely
        std::visit([](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                std::cout << "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                std::cout << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, std::shared_ptr<StructBlueprint>> || 
                                 std::is_same_v<T, std::shared_ptr<StructInstance>> ||
                                 std::is_same_v<T, std::shared_ptr<RuntimeFunction>> ||
                                 std::is_same_v<T, std::shared_ptr<NativeFunction>>) {
                if (arg) std::cout << arg->toString();
                else std::cout << "null";
            } else {
                std::cout << arg; // Natively handles double and std::string
            }
        }, chunk->constants[constant_index]);
        
        std::cout << "'\n";
        
        // Return offset + 2 because this instruction takes up TWO bytes (opcode + index)
        return offset + 2; 
    }
};