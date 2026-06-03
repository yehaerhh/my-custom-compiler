cmake_minimum_required(VERSION 3.10)

# Project Name
project(MyCompiler CXX)

# Force modern C++ standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- 1. PRODUCTION OPTIMIZATION & WARNINGS ---
# -O3: Maximum optimization (makes your compiler & VM run lightning fast)
# -Wall -Wextra: Shows all potential code quality issues
# -march=native: Optimizes the code specifically for YOUR actual CPU hardware
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic)
else()
    # Debug mode keeps symbols (-g) and turns off optimizations for easier stepping
    add_compile_options(-g -Wall -Wextra -Wpedantic)
endif()

# --- 2. SOURCE MANAGEMENT ---
include_directories(src)

# Professional tip: Explicitly listing files is safer than GLOB, 
# but for a project this size, GLOB is perfectly fine.
file(GLOB SOURCES "src/*.cpp")

# Build the main compiler executable
add_executable(compiler ${SOURCES})

# --- 3. TESTING SUITE ---
enable_testing()
find_package(GTest REQUIRED)

# Test executable
add_executable(compiler_tests tests/test_main.cpp)

# Link GoogleTest and threading libraries
target_link_libraries(compiler_tests GTest::GTest GTest::Main pthread)

# Register tests
add_test(NAME SanityCheck COMMAND compiler_tests)