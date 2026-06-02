#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "Scanner.h"
#include "Parser.h"
#include "Chunk.h"
#include "Debug.h"
#include "VM.h"
#include "Compiler.h"


void run(const std::string& source) {
    Scanner scanner(source);
    std::vector<Token> tokens = scanner.scanTokens();
    
    Parser parser(tokens);
    std::vector<std::unique_ptr<Stmt>> statements = parser.parse();

    Chunk chunk;
    Compiler compiler; // Call the default constructor (which is fine!)
    
    // Use your specific compile method:
    compiler.compile(statements, &chunk);

    // Run the VM
    VM vm;
    vm.interpret(&chunk);
}

// Task 10: Read an entire file into memory
void runFile(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << std::endl;
        exit(74); // Standard exit code for IO Error
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf(); // Dump the whole file into the buffer
    run(buffer.str());
}

// Task 9: The Interactive REPL
void runPrompt() {
    std::string line;
    std::cout << "Custom Compiler REPL v1.0. Type 'exit' to quit.\n";
    
    for (;;) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break; // Catch Ctrl+D
        if (line.rfind("exit", 0) == 0) break;
        
        run(line);
    }
}



int main() {
    // 1. The Source Code
    // The first line calculates 99 but should immediately pop it.
    // The second line calculates 5 and prints it.
    std::string source = 
        "let myCar = Vehicle(); "
        "myCar->speed = 120; "
        "print myCar->speed;";
    
    // 2. Scan to Tokens
    Scanner scanner(source);
    std::vector<Token> tokens = scanner.scanTokens();
    
    // 3. Parse to AST
    Parser parser(tokens);
    std::vector<std::unique_ptr<Stmt>> statements = parser.parse();
    
    // 4. Compile AST to Bytecode
    Chunk chunk;
    Compiler compiler;
    compiler.compile(statements, &chunk);
    
    // 5. Verify the emitted bytecode
    Debugger::disassembleChunk(&chunk, "Compiled AST");
    
    // 6. Execute in the Virtual Machine!
    VM vm;
    std::cout << "\n--- Starting VM ---\n";
    vm.interpret(&chunk);
    
    return 0;
}