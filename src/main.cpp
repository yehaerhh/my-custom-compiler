#include <iostream>

int main(int argc, char* argv[]) {
    // Eventually, we will pass a source code file here: ./compiler script.ext
    if (argc == 1) {
        std::cout << "Usage: compiler [path to script]" << std::endl;
        return 64; // Standard exit code for incorrect usage
    }

    std::cout << "Compiler initialized. Ready to read: " << argv[1] << std::endl;
    return 0;
}