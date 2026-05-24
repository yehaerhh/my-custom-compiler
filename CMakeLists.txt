cmake_minimum_required(VERSION 3.10)

# Name of your project
project(MyCompiler)

# Force modern C++ standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Tell it to build an executable named 'compiler' using main.cpp
add_executable(compiler src/main.cpp)