#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// --- THE NEW HARDWARE TOOLCHAIN ---
#include "Scanner.h"
#include "Parser.h"
#include "Compiler.h"
#include "Assembler.h"
#include "CPU.h"

// --- TASK 90: THE LINKER HELPERS ---
void saveBinaryExecutable(const std::vector<uint16_t>& machineCode, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Linker Error: Could not create binary file.\n";
        return;
    }
    for (uint16_t instruction : machineCode) {
        uint8_t highByte = (instruction >> 8) & 0xFF;
        uint8_t lowByte = instruction & 0xFF;
        file.write(reinterpret_cast<char*>(&highByte), 1);
        file.write(reinterpret_cast<char*>(&lowByte), 1);
    }
    file.close();
}

void flashROM(CPU& cpu, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return;
    int address = 0;
    char byte;
    while (file.read(&byte, 1) && address < 32768) { 
        cpu.memory[address] = static_cast<uint8_t>(byte);
        address++;
    }
    file.close();
}

// --- UPDATED CORE RUNNER ---
void run(const std::string& source) {
    // 1. Frontend: Scanner -> Tokens -> Parser -> AST
    Scanner scanner(source);
    std::vector<Token> tokens = scanner.scanTokens();
    
    Parser parser(tokens);
    std::vector<std::unique_ptr<Stmt>> statements = parser.parse();
    if (statements.empty()) return;

    // 2. Middle-end: AST -> Register-Based Assembly
    Compiler compiler;
    compiler.compile(statements); 

    std::cout << "\n=== GENERATED ASSEMBLY OUT ===\n";
    for (const auto& line : compiler.assemblyOutput) {
        std::cout << line << "\n";
    }
    std::cout << "==============================\n";

    
    // 3. Backend: Assembly -> 16-bit Machine Code
    Assembler assembler;
    std::vector<uint16_t> machineCode = assembler.assembleProgram(compiler.assemblyOutput);
    
    // 4. Linker: Save to Disk
    saveBinaryExecutable(machineCode, "out.bin");

    // 5. Hardware: Load & Run
    CPU myComputer;
    flashROM(myComputer, "out.bin");
    
    std::cout << "--- HARDWARE EXECUTION ---" << std::endl;
    myComputer.run();
    std::cout << "--------------------------" << std::endl;
}

// Task 10: File Loader (Preserved)
void runFile(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << std::endl;
        exit(74);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    run(buffer.str());
}

// Task 9: REPL (Preserved)
void runPrompt() {
    std::string line;
    std::cout << "Hardware Compiler REPL v2.0 (Register Machine)\n";
    for (;;) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
        run(line);
    }
}

int main(int argc, char* argv[]) {
    // Now you can run: ./compiler script.txt 
    // OR just: ./compiler (for the REPL)
    if (argc > 1) {
        runFile(argv[1]);
    } else {
        runPrompt();
    }
    
    return 0;
}