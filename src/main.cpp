#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "Scanner.h"
#include "Parser.h"
#include "AstPrinter.h"
#include "Intrepreter.h"

Interpreter interpreter;
// This function will eventually trigger your Lexer
// This function triggers the Lexer and prints the raw Tokens
void run(const std::string& source) {
    // 1. Lexical Analysis
    Scanner scanner(source);
    std::vector<Token> tokens = scanner.scanTokens();
    if (scanner.hasError) {
        std::cerr << "\nCompilation halted due to lexical errors." << std::endl;
        exit(65);
    }

    // 2. Parsing (Builds a sequence of statements)
    Parser parser(tokens);
    std::vector<std::unique_ptr<Stmt>> statements = parser.parse();

    if (statements.empty()) return;

    // 3. Execution Engine (Task 39)
    interpreter.interpret(statements);
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

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cout << "Usage: compiler [script_path]" << std::endl;
        return 64; 
    } else if (argc == 2) {
        // If they passed a file, run Task 10
        runFile(argv[1]);
    } else {
        // If they just ran ./compiler, start the REPL (Task 9)
        runPrompt();
    }
    return 0;
}