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
        
        // --- PASS 1: RECORD LABELS ---
        uint16_t currentAddress = 0; 
        
        for (const std::string& line : sourceCode) {
            if (line.empty()) continue;

            if (line.back() == ':') {
                std::string labelName = line.substr(0, line.length() - 1);
                symbolTable[labelName] = currentAddress; 
            } else {
                std::vector<std::string> tokens = tokenize(line);
                // If it's a LOAD_ADDR, it takes 4 bytes in RAM. Everything else takes 2 bytes.
                if (!tokens.empty() && tokens[0] == "LOAD_ADDR") {
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

        if (mnemonic == "LOAD_ADDR") {
            uint16_t opcode = 0x16; 
            uint16_t dest = parseRegister(tokens[1]); 
            uint16_t addr = std::stoi(tokens[2]); // Pass 2 already swapped labels for numbers!
            
            // Word 1: Opcode and Destination Register
            uint16_t word1 = (opcode << 11) | (dest << 8);
            // Word 2: The raw 16-bit address
            uint16_t word2 = addr;
            
            return {word1, word2};
        }
        else if (mnemonic == "MOV_IMM") {
            uint16_t opcode = 0x00; 
            uint16_t dest = parseRegister(tokens[1]); 
            uint16_t imm = std::stoi(tokens[2]); 
            // Return as a single-element vector
            return { (uint16_t)((opcode << 11) | (dest << 8) | (imm & 0xFF)) };
        }
        // ... (Wrap your other opcode returns in {} to match the vector return type)
        // Example: return { (uint16_t)((opcode << 11) | (dest << 8) | (src1 << 5) | (src2 << 2)) };
};