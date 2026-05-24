cmake_minimum_required(VERSION 3.10)

# Name of your project
project(MyCompiler)

# Force modern C++ standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Tell it to build an executable named 'compiler' using main.cpp
add_executable(compiler src/main.cpp)


# Enable testing
enable_testing()

# Find the GoogleTest library on your Arch system
find_package(GTest REQUIRED)

# Create a separate executable just for running tests
add_executable(compiler_tests tests/test_main.cpp)

# Link GoogleTest to your test executable
target_link_libraries(compiler_tests GTest::GTest GTest::Main pthread)

# Register the test with CTest
add_test(NAME SanityCheck COMMAND compiler_tests)