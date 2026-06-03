#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// --- THE HARDWARE TOOLCHAIN ---
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
    
    // We must write byte-by-byte here to force Big-Endian format, 
    // ensuring it works even if the host computer (x86) is Little-Endian.
    for (uint16_t instruction : machineCode) {
        uint8_t highByte = (instruction >> 8) & 0xFF;
        uint8_t lowByte = instruction & 0xFF;
        file.write(reinterpret_cast<char*>(&highByte), 1);
        file.write(reinterpret_cast<char*>(&lowByte), 1);
    }
    file.close();
}

void flashROM(CPU& cpu, const std::string& filename) {
    // Open the file and immediately seek to the end (ate = at end) to get its size
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return;

    std::streamsize size = file.tellg(); // Get total file size
    file.seekg(0, std::ios::beg);        // Rewind back to the beginning

    // Prevent buffer overflow if the binary is somehow larger than the 32KB ROM space
    if (size > 32768) size = 32768;

    // --- MASSIVE OPTIMIZATION ---
    // Instead of looping byte-by-byte, we load the entire file into the CPU's memory 
    // array using a single operating system call.
    file.read(reinterpret_cast<char*>(cpu.memory), size);
    
    file.close();
}

// --- UPDATED CORE RUNNER ---
// Added the printAsm boolean flag!
void run(const std::string& source, bool printAsm) {
    // 1. Frontend: Scanner -> Tokens -> Parser -> AST
    Scanner scanner(source);
    std::vector<Token> tokens = scanner.scanTokens();
    
    Parser parser(tokens);
    std::vector<std::unique_ptr<Stmt>> statements = parser.parse();
    if (statements.empty()) return;

    // 2. Middle-end: AST -> Register-Based Assembly
    Compiler compiler;
    compiler.compile(statements); 

    // ONLY print the assembly if the user passed the flag!
    if (printAsm) {
        std::cout << "\n=== GENERATED ASSEMBLY OUT ===\n";
        for (const auto& line : compiler.assemblyOutput) {
            std::cout << line << "\n";
        }
        std::cout << "==============================\n";
    }
    
    // 3. Backend: Assembly -> 16-bit Machine Code
    Assembler assembler;
    std::vector<uint16_t> machineCode = assembler.assembleProgram(compiler.assemblyOutput);
    
    // 4. Linker: Save to Disk
    saveBinaryExecutable(machineCode, "out.bin");

    // 5. Hardware: Load & Run
    CPU myComputer;
    flashROM(myComputer, "out.bin");
    
    // std::cout << "--- HARDWARE EXECUTION ---\n";
    myComputer.run();
    // std::cout << "--------------------------\n";
}

// Task 10: File Loader 
void runFile(const char* path, bool printAsm) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << std::endl;
        exit(74);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    run(buffer.str(), printAsm);
}

// Task 9: REPL 
// Upgraded Interactive Prompt (REPL)
void runPrompt(bool printAsm) {
    std::string line;
    std::string sessionCode = ""; // This holds everything you type!
    
    std::cout << "Hardware Compiler REPL v1.0.0\n";
    std::cout << "Type your code live. Type 'run' to execute it on the CPU.\n";
    std::cout << "Type 'clear' to wipe the RAM, or 'exit' to quit.\n";
    
    for (;;) {
        std::cout << ">>> ";
        if (!std::getline(std::cin, line)) break;
        
        if (line == "exit") {
            break;
        }
        else if (line == "clear") {
            sessionCode = ""; // Wipes the slate clean
            std::cout << "[Memory Cleared]\n";
        }
        else if (line == "run") {
            // Compile and run everything typed so far!
            run(sessionCode, printAsm);
        }
        else {
            // Add the line to our live session code
            sessionCode += line + "\n";
        }
    }
}

// --- NEW COMMAND LINE ARGUMENT PARSER ---
int main(int argc, char* argv[]) {
    bool printAssembly = false;
    std::string filename = "";

    // Loop through all arguments provided to the executable
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // If we see our custom flags, set the boolean to true
        if (arg == "--asm" || arg == "-a") {
            printAssembly = true;
        } 
        // If it's not a flag, and we haven't found a filename yet, it must be the script!
        else if (filename.empty()) {
            filename = arg;
        } 
        // If we already have a filename, they typed too many things.
        else {
            std::cerr << "Usage: ./compiler [--asm] [script.txt]\n";
            return 1;
        }
    }

    // Launch the correct mode
    if (!filename.empty()) {
        runFile(filename.c_str(), printAssembly);
    } else {
        runPrompt(printAssembly);
    }
    
    return 0;
}