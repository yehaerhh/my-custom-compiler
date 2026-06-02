cmake_minimum_required(VERSION 3.10)

# Name of your project
project(MyCompiler CXX)

# Force modern C++ standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- 1. NEW: Strict Warnings & Debugging ---
add_compile_options(-Wall -Wextra -Wpedantic -g)

# --- 2. NEW: Global Include Directory ---
include_directories(src)

# --- 3. CHANGED: Auto-detect all source files ---
file(GLOB SOURCES "src/*.cpp")

# Tell it to build the main executable using the auto-detected sources
add_executable(compiler ${SOURCES})

# --- YOUR GOOGLETEST SETUP (PRESERVED) ---
enable_testing()

# Find the GoogleTest library on your Arch system
find_package(GTest REQUIRED)

# Create a separate executable just for running tests
add_executable(compiler_tests tests/test_main.cpp)

# Link GoogleTest to your test executable
target_link_libraries(compiler_tests GTest::GTest GTest::Main pthread)

# Register the test with CTest
add_test(NAME SanityCheck COMMAND compiler_tests)