#pragma once
#include <vector>
#include <iostream>
#include <unordered_map>
#include <string>
#include "Chunk.h"
#include "Expr.h"

enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

struct CallFrame {
    // The function object we are executing
    std::shared_ptr<RuntimeFunction> function; 
    
    // The instruction pointer (the 'bookmark' for this function)
    uint8_t* ip; 
    
    // Pointer to the first local variable slot for this function
    Object* slots; 
};

const int STACK_MAX = 256; 

class VM {
public:
    Chunk* chunk;
    CallFrame frames[64]; // Support up to 64 nested function calls
    int frameCount = 0;   // How deep we are in the call stack

    Object stack[STACK_MAX];
    Object* stackTop;
    std::unordered_map<std::string, Object> globals; // <--- ADD THIS
    std::vector<Object> heap;
    
    bool isFalsey(Object value) {
        if (std::holds_alternative<std::monostate>(value)) return true; // Null is false
        if (std::holds_alternative<bool>(value)) return !std::get<bool>(value);
        return false; // Everything else (numbers, strings, structs) is true
    }

    VM() {
        resetStack();
    }

    void resetStack() {
        stackTop = stack;
    }

    void push(Object value) {
        *stackTop = value;
        stackTop++;
    }

    Object pop() {
        stackTop--;
        return *stackTop;
    }

    Object& peek(int distance) {
        return stackTop[-1 - distance];
    }

    // Helper to safely print ANY of our custom Object types
    void printObject(Object value) {
        std::visit([](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                std::cout << "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                std::cout << (arg ? "true" : "false");
            } 
        // Use 'if constexpr' to handle only the types actually present in your variant
            else if constexpr (std::is_same_v<T, std::shared_ptr<RuntimeFunction>> ||
                           std::is_same_v<T, std::shared_ptr<NativeFunction>>) {
            std::cout << arg->toString();
        } else {
            std::cout << arg;
        }
        }, value);
    }

    void sweep() {
        // Iterate backwards or use an iterator so we can safely delete items
        for (auto it = heap.begin(); it != heap.end(); ) {
            bool marked = false;
            
            // We have to use std::visit to look inside the variant and check the flag
            std::visit([&marked](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                // Only check types that actually have the 'isMarked' flag
                if constexpr (std::is_same_v<T, std::shared_ptr<StructInstance>> || 
                              std::is_same_v<T, std::shared_ptr<RuntimeFunction>>) {
                    if (arg) marked = arg->isMarked;
                }
            }, *it);

            if (!marked) {
                // THE MAGIC: Erasing it from this vector drops the shared_ptr count.
                // If it's not on the stack anymore, C++ instantly deletes it from RAM!
                it = heap.erase(it); 
            } else {
                // It IS marked (in use). Unmark it so it can be collected NEXT time if it becomes unused.
                std::visit([](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::shared_ptr<StructInstance>> || 
                                  std::is_same_v<T, std::shared_ptr<RuntimeFunction>>) {
                        if (arg) arg->isMarked = false;
                    }
                }, *it);
                
                ++it; // Move to the next object
            }
        }
    }

    InterpretResult run(CallFrame* frame) {
        // Change this:
        #define READ_BYTE() (*frame->ip++)
        #define READ_CONSTANT() (chunk->constants[READ_BYTE()])
        #define READ_STRING() (std::get<std::string>(READ_CONSTANT())) // <--- ADD THIS
        #define READ_SHORT() \
            (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
        // Task 59: A macro to cleanly execute math operations
        #define BINARY_OP(op) \
            do { \
                if (!std::holds_alternative<double>(*(stackTop - 1)) || \
                    !std::holds_alternative<double>(*(stackTop - 2))) { \
                    std::cerr << "Runtime Error: Operands must be numbers.\n"; \
                    return INTERPRET_RUNTIME_ERROR; \
                } \
                double b = std::get<double>(pop()); \
                double a = std::get<double>(pop()); \
                push(a op b); \
            } while (false)

        for (;;) {
            // -- DEBUG TRACE: Print the stack cleanly --
            std::cout << "          ";
            for (Object* slot = stack; slot < stackTop; slot++) {
                std::cout << "[ ";
                printObject(*slot); // Use our safe printer!
                std::cout << " ]";
            }
            std::cout << "\n";
            // ------------------------------------------

            uint8_t instruction;
            switch (instruction = READ_BYTE()) {
                
                case OP_CONSTANT: {
                    Object constant = READ_CONSTANT();
                    push(constant);
                    break;
                }
                
                case OP_RETURN: {
                    Object result = pop(); // Grab the value the function is returning
                    frameCount--;          // Drop the current call frame

                    if (frameCount == 0) {
                        pop(); // Pop the main dummy function
                        return INTERPRET_OK; // Program finished completely!
                    }

                    // Discard all the function's local variables from the stack
                    stackTop = frame->slots; 
                
                    // Push the result back onto the stack so the caller can use it
                    push(result);            

                    // Restore the VM to the previous frame
                    frame = &frames[frameCount - 1];
                    break;
                }
                
                // Task 59: Execute the math!
                case OP_ADD:      BINARY_OP(+); break;
                case OP_SUBTRACT: BINARY_OP(-); break;
                case OP_MULTIPLY: BINARY_OP(*); break;
                case OP_DIVIDE:   BINARY_OP(/); break;
                // Logic
                case OP_LESS:     BINARY_OP(<); break;
                case OP_GREATER:  BINARY_OP(>); break;
                case OP_EQUAL:    BINARY_OP(==); break;

                case OP_ALLOC_STRUCT: {
                    // For now, we just push a completely empty struct instance to the stack!
                    // (We will wire up the blueprint name later in the compiler)
                    auto instance = std::make_shared<StructInstance>(nullptr);
                    Object obj = instance;

                    heap.push_back(obj);
                    push(obj);
                    break;
                }

                case OP_GET_FIELD: {
                    std::string fieldName = READ_STRING();
                    Object target = pop();
                    
                    if (!std::holds_alternative<std::shared_ptr<StructInstance>>(target)) {
                        std::cerr << "Runtime Error: Only instances have properties.\n";
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    auto instance = std::get<std::shared_ptr<StructInstance>>(target);
                    if (instance->fields.find(fieldName) == instance->fields.end()) {
                        std::cerr << "Runtime Error: Undefined property '" << fieldName << "'.\n";
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    push(instance->fields[fieldName]);
                    break;
                }

                case OP_SET_FIELD: {
                    std::string fieldName = READ_STRING();
                    Object value = pop();  // The value we want to save
                    Object target = pop(); // The struct instance we are saving it to
                    
                    if (!std::holds_alternative<std::shared_ptr<StructInstance>>(target)) {
                        std::cerr << "Runtime Error: Only instances have fields.\n";
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    auto instance = std::get<std::shared_ptr<StructInstance>>(target);
                    instance->fields[fieldName] = value;
                    
                    // Assignment is an expression! We leave the evaluated value sitting on the stack.
                    push(value); 
                    break;
                }
                
                case OP_NEGATE: {
                    if (!std::holds_alternative<double>(*(stackTop - 1))) {
                        std::cerr << "Runtime Error: Operand must be a number.\n";
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    // Pop it, flip the sign, and push it back!
                    double value = std::get<double>(pop());
                    push(-value);
                    break;
                }

                case OP_POP: { // <--- ADD THIS
                    pop(); 
                    break;
                }

                case OP_PRINT: {
                    printObject(pop());
                    std::cout << "\n";
                    break;
                }

                case OP_GET_LOCAL: {
                    uint8_t slot = READ_BYTE();
                    push(stack[slot]); // Copy the value from the local slot to the top of the stack
                    break;
                }

                case OP_SET_LOCAL: {
                    uint8_t slot = READ_BYTE();
                    stack[slot] = *(stackTop - 1); // Overwrite the local slot with the top of the stack
                    break;
                }

                case OP_DEFINE_GLOBAL: {
                    std::string name = READ_STRING(); // Get the variable name from the constant pool
                    globals[name] = pop();            // Take the value off the stack and store it!
                    break;
                }
                
                case OP_GET_GLOBAL: {
                    std::string name = READ_STRING();
                    if (globals.find(name) == globals.end()) {
                        std::cerr << "Runtime Error: Undefined variable '" << name << "'.\n";
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(globals[name]); // Push the stored value onto the stack
                    break;
                }

                case OP_SET_GLOBAL: {
                    std::string name = READ_STRING();
                    if (globals.find(name) == globals.end()) {
                        std::cerr << "Runtime Error: Undefined variable '" << name << "'.\n";
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    // Assignment is an expression! We update the map, but leave the value ON the stack 
                    // so it can be used by other things (e.g. print a = 5;)
                    globals[name] = *(stackTop - 1); 
                    break;
                }

                case OP_JUMP_IF_FALSE: {
                    uint16_t offset = READ_SHORT();
                    if (isFalsey(peek(0))) frame->ip += offset; // Use frame->ip
                    break;
                }

                case OP_JUMP: {
                    uint16_t offset = READ_SHORT();
                    frame->ip += offset; // Use frame->ip
                    break;
                }

                case OP_LOOP: {
                    uint16_t offset = READ_SHORT();
                    frame->ip -= offset; // Use frame->ip
                    break;
                }
                case OP_CALL: {
                    int argCount = READ_BYTE(); // How many arguments we passed to the function
                    Object callee = peek(argCount); // The function sits on the stack just below the arguments

                    if (std::holds_alternative<std::shared_ptr<RuntimeFunction>>(callee)) {
                        auto function = std::get<std::shared_ptr<RuntimeFunction>>(callee);
                    
                        if (argCount != function->arity) {
                            std::cerr << "Runtime Error: Expected " << function->arity << " arguments but got " << argCount << ".\n";
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        if (frameCount == 64) {
                            std::cerr << "Runtime Error: Stack overflow.\n";
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        // 1. Create a new CallFrame on the top of the stack
                        CallFrame* newFrame = &frames[frameCount++];
                        newFrame->function = function;
                        newFrame->ip = function->chunk.code.data();
                    
                        // 2. Give the function its own local memory space
                        newFrame->slots = stackTop - argCount - 1; 
                    
                        // 3. Switch the VM's active frame to this new one!
                        frame = newFrame;
                    } else {
                        std::cerr << "Runtime Error: Can only call functions.\n";
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
            }
        }

        #undef READ_BYTE
        #undef READ_CONSTANT
        #undef READ_STRING
        #undef READ_SHORT
    }

    InterpretResult interpret(Chunk* chunk) {
    // Initialize the first frame
    frameCount = 0;
    CallFrame* frame = &frames[frameCount++];
    frame->function = nullptr; // Or a dummy main function
    frame->ip = chunk->code.data();
    frame->slots = stack;      // Assuming you have a stack array

    // Now, pass 'frame' to your run() loop!
    return run(frame); 
    }
};