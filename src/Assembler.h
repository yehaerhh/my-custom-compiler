#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdint>
#include <unordered_map>

class Assembler {
public:
    // Our Symbol Table: Maps "loop_start" -> Memory Address 0x0004
    std::unordered_map<std::string, uint16_t> symbolTable;

    // The Master Assembler Function
    std::vector<uint16_t> assembleProgram(const std::vector<std::string>& sourceCode) {
        std::vector<uint16_t> binary;
        
        uint16_t currentAddress = 0; 
        
        for (const std::string& line : sourceCode) {
            if (line.empty()) continue;

            if (line.back() == ':') {
                std::string labelName = line.substr(0, line.length() - 1);
                symbolTable[labelName] = currentAddress; 
            } else {
                std::vector<std::string> tokens = tokenize(line);
                
                // FIX: If the line was just spaces or comments, skip it without advancing RAM address!
                if (tokens.empty()) continue; 

                // If it's a LOAD_ADDR, it takes 4 bytes in RAM. Everything else takes 2 bytes.
                if (tokens[0] == "LOAD_ADDR") {
                    currentAddress += 4; 
                } else {
                    currentAddress += 2; 
                }
            }
        }

        // --- PASS 2: GENERATE BINARY ---
        for (const std::string& line : sourceCode) {
            // Skip empty lines and labels
            if (line.empty() || line.back() == ':') continue;

            std::vector<std::string> tokens = tokenize(line);

            // Replace text labels (like loop_start) with raw numeric strings
            for (size_t i = 1; i < tokens.size(); i++) {
                if (symbolTable.find(tokens[i]) != symbolTable.end()) {
                    tokens[i] = std::to_string(symbolTable[tokens[i]]);
                }
            }

            // --- THE FIXED REFACTOR LOCATION ---
            // assembleLine now returns a vector of words. We fetch that vector,
            // iterate through it, and push each word into our final machine code.
            std::vector<uint16_t> instructionWords = assembleLine(tokens);
            for (uint16_t word : instructionWords) {
                binary.push_back(word);
            }
        }

        return binary;
    }

    // --- YOUR EXISTING HELPER METHODS ---
    std::vector<std::string> tokenize(const std::string& line) {
        std::vector<std::string> tokens;
        std::stringstream ss(line);
        std::string token;
        while (ss >> token) tokens.push_back(token);
        return tokens;
    }

    uint16_t parseRegister(const std::string& reg) {
        if (reg.length() == 2 && reg[0] == 'R') return reg[1] - '0';
        return 0; 
    }

    // Notice we changed this to accept the already-tokenized array!
    // Change the return type from uint16_t to std::vector<uint16_t>
    std::vector<uint16_t> assembleLine(const std::vector<std::string>& tokens) {
        if (tokens.empty()) return {};
        std::string mnemonic = tokens[0];

        // --- FORMAT B: IMMEDIATE LOADS (Opcodes take an 8-bit immediate) ---
        if (mnemonic == "MOV_IMM") {
            uint16_t opcode = 0x00;
            uint16_t dest = parseRegister(tokens[1]);
            uint16_t imm = std::stoi(tokens[2]);
            return { (uint16_t)((opcode << 11) | (dest << 8) | (imm & 0xFF)) };
        }
        else if (mnemonic == "LOAD_ADDR") {
            uint16_t opcode = 0x16; 
            uint16_t dest = parseRegister(tokens[1]); 
            uint16_t addr = std::stoi(tokens[2]); 
            return { (uint16_t)((opcode << 11) | (dest << 8)), addr }; // Returns 2 words!
        }

        // --- FORMAT A: REGISTER-TO-REGISTER (Math & Memory Access) ---
        else if (mnemonic == "LOAD") {
            uint16_t opcode = 0x02;
            uint16_t dest = parseRegister(tokens[1]);
            uint16_t src1 = parseRegister(tokens[2]);
            return { (uint16_t)((opcode << 11) | (dest << 8) | (src1 << 5)) };
        }
        else if (mnemonic == "STORE") {
            uint16_t opcode = 0x03;
            uint16_t dest = parseRegister(tokens[1]); // Address register
            uint16_t src1 = parseRegister(tokens[2]); // Value register
            return { (uint16_t)((opcode << 11) | (dest << 8) | (src1 << 5)) };
        }
        else if (mnemonic == "ADD") {
            uint16_t opcode = 0x04;
            uint16_t dest = parseRegister(tokens[1]);
            uint16_t src1 = parseRegister(tokens[2]);
            uint16_t src2 = parseRegister(tokens[3]);
            return { (uint16_t)((opcode << 11) | (dest << 8) | (src1 << 5) | (src2 << 2)) };
        }
        else if (mnemonic == "SUB") {
            uint16_t opcode = 0x05;
            uint16_t dest = parseRegister(tokens[1]);
            uint16_t src1 = parseRegister(tokens[2]);
            uint16_t src2 = parseRegister(tokens[3]);
            return { (uint16_t)((opcode << 11) | (dest << 8) | (src1 << 5) | (src2 << 2)) };
        }
        else if (mnemonic == "MUL") {
            uint16_t opcode = 0x06;
            uint16_t dest = parseRegister(tokens[1]);
            uint16_t src1 = parseRegister(tokens[2]);
            uint16_t src2 = parseRegister(tokens[3]);
            return { (uint16_t)((opcode << 11) | (dest << 8) | (src1 << 5) | (src2 << 2)) };
        }
        else if (mnemonic == "DIV") {
            uint16_t opcode = 0x07;
            uint16_t dest = parseRegister(tokens[1]);
            uint16_t src1 = parseRegister(tokens[2]);
            uint16_t src2 = parseRegister(tokens[3]);
            return { (uint16_t)((opcode << 11) | (dest << 8) | (src1 << 5) | (src2 << 2)) };
        }
        else if (mnemonic == "CMP") {
            uint16_t opcode = 0x0C;
            uint16_t src1 = parseRegister(tokens[1]);
            uint16_t src2 = parseRegister(tokens[2]);
            return { (uint16_t)((opcode << 11) | (src1 << 8) | (src2 << 5)) };
        }
        else if (mnemonic == "PUSH") {
            uint16_t opcode = 0x12; // Matches your Opcode16::PUSH
            uint16_t dest = parseRegister(tokens[1]);
            return { (uint16_t)((opcode << 11) | (dest << 8)) };
        }
        else if (mnemonic == "POP") {
            uint16_t opcode = 0x13; // Matches your Opcode16::POP
            uint16_t dest = parseRegister(tokens[1]);
            return { (uint16_t)((opcode << 11) | (dest << 8)) };
        }
        // --- FORMAT C: JUMPS & BRANCHING (Opcodes take an 11-bit address destination) ---
        else if (mnemonic == "JMP") {
            uint16_t opcode = 0x0D;
            uint16_t addr = std::stoi(tokens[1]);
            return { (uint16_t)((opcode << 11) | (addr & 0x7FF)) };
        }
        else if (mnemonic == "JEQ") {
            uint16_t opcode = 0x0E;
            uint16_t addr = std::stoi(tokens[1]);
            return { (uint16_t)((opcode << 11) | (addr & 0x7FF)) };
        }
        else if (mnemonic == "JNE") {
            uint16_t opcode = 0x0F;
            uint16_t addr = std::stoi(tokens[1]);
            return { (uint16_t)((opcode << 11) | (addr & 0x7FF)) };
        }
        else if (mnemonic == "HALT") {
            return { (uint16_t)(0x1F << 11) };
        }
        // --- ADD THESE INSIDE YOUR assembleLine IF/ELSE CHAIN ---
        else if (mnemonic == "MOV_REG") {
            uint16_t opcode = 0x01;
            uint16_t dest = parseRegister(tokens[1]);
            uint16_t src1 = parseRegister(tokens[2]);
            return { (uint16_t)((opcode << 11) | (dest << 8) | (src1 << 5)) };
        }
        else if (mnemonic == "CALL") {
            uint16_t opcode = 0x14;
            uint16_t reg = parseRegister(tokens[1]); // The register holding the target address
            return { (uint16_t)((opcode << 11) | (reg << 8)) };
        }
        else if (mnemonic == "RET") {
            uint16_t opcode = 0x15;
            return { (uint16_t)(opcode << 11) };
        }

        std::cerr << "Assembler Error: Unknown instruction " << mnemonic << "\n";
        return {};
    }
};