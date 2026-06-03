#pragma once
#include <cstdint>
#include <iostream>
#include <vector>
#include <cstring>

// 1. The Language of the Hardware
enum class Opcode16 : uint8_t {
    MOV_IMM = 0x00, MOV_REG = 0x01, LOAD = 0x02, STORE = 0x03,
    ADD = 0x04, SUB = 0x05, MUL = 0x06, DIV = 0x07,
    AND = 0x08, OR = 0x09, XOR = 0x0A, NOT = 0x0B, CMP = 0x0C,
    JMP = 0x0D, JEQ = 0x0E, JNE = 0x0F, JLT = 0x10, JGT = 0x11,
    PUSH = 0x12, POP = 0x13, CALL = 0x14, RET = 0x15, LOAD_ADDR = 0x16,
    HALT = 0x1F
};

class CPU {
public:
    // --- THE REGISTER FILE ---
    uint16_t r[8];       // General Purpose Registers (R0 - R7)
    uint16_t pc;         // Program Counter
    uint16_t sp;         // Stack Pointer
    uint16_t flags;      // Status Flags

    // --- THE PHYSICAL RAM ---
    // 65536 bytes = Exactly 64 Kilobytes
    uint8_t memory[65536];

    // Constructor: Power on the machine
    CPU() {
        reset();
    }

    // --- STACK HELPERS ---
    
    void push16(uint16_t value) {
        sp -= 2; // Move the stack pointer down by 2 bytes
        
        // Save the 16-bit value in Big-Endian format (High byte first)
        memory[sp]     = (value >> 8) & 0xFF; 
        memory[sp + 1] = value & 0xFF;        
    }

    uint16_t pop16() {
        // Read the 16-bit value
        uint16_t value = (memory[sp] << 8) | memory[sp + 1];
        
        sp += 2; // Move the stack pointer back up
        return value;
    }

    // Add this inside the CPU class, right below your reset() method!
    
    void run() {
        bool isRunning = true;

        while (isRunning) {
            // --- 1. FETCH ---
            // Our memory array stores 8-bit bytes, but our instructions are 16-bit words.
            // We read two bytes and combine them. (Using Big-Endian: High byte first)
            uint16_t instruction = (memory[pc] << 8) | memory[pc + 1];
            
            // Advance the Program Counter by 2 bytes to point to the next instruction
            pc += 2; 

            // --- 2. DECODE ---
            // We use bitwise shifting (>>) and masking (&) to slice up the 16 bits.
            
            // Opcode is the top 5 bits (Bits 11 to 15)
            uint8_t opcode = (instruction >> 11) & 0x1F;
            
            // Dest is the next 3 bits (Bits 8 to 10)
            uint8_t dest = (instruction >> 8) & 0x07;

            // For Format A (Register Math): Extract Src1 and Src2
            uint8_t src1 = (instruction >> 5) & 0x07;
            uint8_t src2 = (instruction >> 2) & 0x07;

            // For Format B (Immediate Load): Extract the bottom 8 bits
            uint8_t imm = instruction & 0xFF;


            // --- 3. EXECUTE ---
            switch (static_cast<Opcode16>(opcode)) {
                
                case Opcode16::HALT:
                    std::cout << "CPU Halted.\n";
                    isRunning = false;
                    break;
                
                case Opcode16::MOV_IMM:
                    // Task 82/83: Move the 8-bit immediate value into the destination register
                    r[dest] = imm;
                    break;

                case Opcode16::MOV_REG:
                    // Copy value from Src1 register to Dest register
                    r[dest] = r[src1];
                    break;

                // We will fill out the ALU and Memory operations in Tasks 82 & 83!
                // --- ARITHMETIC ---
                case Opcode16::ADD:
                    r[dest] = r[src1] + r[src2];
                    break;
                    
                case Opcode16::SUB:
                    r[dest] = r[src1] - r[src2];
                    break;
                    
                case Opcode16::MUL:
                    r[dest] = r[src1] * r[src2];
                    break;
                    
                case Opcode16::DIV:
                    if (r[src2] == 0) {
                        std::cerr << "Hardware Exception: Division by Zero at PC 0x" << std::hex << (pc - 2) << "\n";
                        isRunning = false; // Halt the CPU on a hardware fault
                    } else {
                        r[dest] = r[src1] / r[src2];
                    }
                    break;

                // --- BITWISE LOGIC ---
                case Opcode16::AND:
                    r[dest] = r[src1] & r[src2];
                    break;
                    
                case Opcode16::OR:
                    r[dest] = r[src1] | r[src2];
                    break;
                    
                case Opcode16::XOR:
                    r[dest] = r[src1] ^ r[src2];
                    break;
                    
                case Opcode16::NOT:
                    r[dest] = ~r[src1]; // Bitwise invert the source register
                    break;

                // --- THE COMPARATOR (FLAGS) ---
                case Opcode16::CMP: {
                    // Compare mathematically subtracts src2 from src1, 
                    // but it DOES NOT save the result. It ONLY updates the FLAGS register.
                    
                    // We cast to 32-bit signed integers so we can accurately check for negative numbers
                    int32_t result = (int32_t)r[dest] - (int32_t)r[src1];
                    
                    flags = 0; // Clear the flags register first

                    // Bit 0: Zero Flag (Z)
                    if (result == 0) {
                        flags |= (1 << 0); // Set bit 0 to 1
                    }
                    
                    // Bit 1: Negative Flag (N)
                    if (result < 0) {
                        flags |= (1 << 1); // Set bit 1 to 1
                    }
                    break;
                }
                // --- MEMORY OPERATIONS ---
                case Opcode16::LOAD: {
                    // LOAD Dest, [Src1]
                    // Reads a 16-bit value from the memory address held in Src1, saves to Dest
                    uint16_t addr = r[src1];
                    
                    // Glue the two 8-bit bytes back into a 16-bit register
                    r[dest] = (memory[addr] << 8) | memory[addr + 1];
                    break;
                }

                case Opcode16::STORE: {
                    // STORE [Dest], Src1
                    // Writes the 16-bit value from Src1 into the memory address held in Dest
                    uint16_t addr = r[dest];
                    uint16_t val = r[src1];

                    // --- THE MAGIC OF MMIO ---
                    // If the CPU tries to write to the designated MMIO address (0xF000),
                    // we intercept it and print it to the terminal instead of saving to RAM!
                    if (addr == 0xF000) {
                        std::cout << val <<std::endl; 
                    } 
                    else {
                        // Otherwise, split the 16-bit value and save it to normal RAM
                        memory[addr]     = (val >> 8) & 0xFF; // High byte
                        memory[addr + 1] = val & 0xFF;        // Low byte
                    }
                    break;
                }
                // --- CONTROL FLOW & BRANCHING ---

                case Opcode16::JMP:
                    // FIX: Unconditional Jump to the embedded 11-bit literal address
                    pc = instruction & 0x7FF;
                    break;

                case Opcode16::JEQ:
                    // FIX: Jump if Equal to the embedded 11-bit literal address
                    if (flags & (1 << 0)) {
                        pc = instruction & 0x7FF;
                    }
                    break;

                case Opcode16::JNE:
                    // FIX: Jump if Not Equal to the embedded 11-bit literal address
                    if (!(flags & (1 << 0))) {
                        pc = instruction & 0x7FF;
                    }
                    break;

                case Opcode16::JLT:
                    // Jump if Less Than: Check if Bit 1 (Negative Flag) is 1
                    if (flags & (1 << 1)) {
                        pc = instruction & 0x7FF; // <-- CHANGED from r[dest]
                    }
                    break;

                case Opcode16::JGT:
                    // Jump if Greater Than: Must NOT be Zero, and MUST NOT be Negative
                    if (!(flags & (1 << 0)) && !(flags & (1 << 1))) {
                        pc = instruction & 0x7FF; // <-- CHANGED from r[dest]
                    }
                    break;
                // --- THE HARDWARE STACK ---               
                case Opcode16::PUSH:
                    // Push the value from the Dest register onto the stack
                    push16(r[dest]);
                    break;

                case Opcode16::POP:
                    // Pop the top of the stack into the Dest register
                    r[dest] = pop16();
                    break;

                case Opcode16::CALL:
                    // 1. Save our current Program Counter (where we return to!)
                    push16(pc);
                    // 2. Jump to the address stored in the Dest register
                    pc = r[dest];
                    break;

                case Opcode16::RET:
                    // Pop the return address off the stack and jump back to it
                    pc = pop16();
                    break;
                // --- NEW: 16-BIT ADDRESS LOADER ---
                case Opcode16::LOAD_ADDR: {
                    // 1. Fetch the NEXT two bytes from RAM as the raw 16-bit address
                    uint16_t fullAddress = (memory[pc] << 8) | memory[pc + 1];
                    
                    // 2. Advance the Program Counter again so it doesn't try to execute the address!
                    pc += 2; 
                    
                    // 3. Save it to the destination register
                    r[dest] = fullAddress;
                    break;
                }
                default:
                    std::cerr << "FATAL: Unknown Hardware Opcode: 0x" << std::hex << (int)opcode << " at PC: 0x" << (pc - 2) << "\n";
                    isRunning = false;
                    break;
            }
        }
    }

    // Hardware Reset Button
    // Hardware Reset Button
    void reset() {
        // Instant hardware-level zeroing of the registers
        std::memset(r, 0, sizeof(r));
        
        // Boot at the very beginning of the ROM
        pc = 0x0000; 
        
        // The stack starts at the TOP of the stack memory map and grows downwards
        sp = 0xF000; 
        
        flags = 0;

        // Instant hardware-level zeroing of the 64KB RAM
        std::memset(memory, 0, sizeof(memory));
    }
};