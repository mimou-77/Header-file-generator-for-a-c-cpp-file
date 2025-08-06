cmake_minimum_required(VERSION 3.13)
project(HeaderGenerator CXX)





#this tool uses CLANG and LLVM
#Clang's primary output is LLVM IR.
#Clang parses c/c++ file, translates the AST into LLVM IR.
#LLVM IR is then passed to the LLVM back end for optimization and code generation


# find the package config file for LLVM (.cmake file) ;
# if not found: stop the project config process
# load the config ; i.e: set CMake variables + define targets for LLVM components
# variables like: LLVM_INCLUDE_DIRS, LLVM_DEFINITIONS, LLVM_VERSION_MAJOR
# targets like: LLVM::Core, LLVM::Support, LLVM::IRReader 
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Include LLVM's CMake macros
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Add Clang's CMake macros
include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${CLANG_DEFINITIONS})


# Ligne de débogage pour vérifier le chemin
message(STATUS "Clang include directories: ${CLANG_INCLUDE_DIRS}")

# Add your source file
add_executable(generate_header_tool main.cpp)

# Link with necessary Clang/LLVM libraries
# Minimal set required for this simple tool
target_link_libraries(generate_header_tool
    PRIVATE
    clangTooling
    clangFrontend
    clangAST
    clangLex
    clangBasic
    LLVMSupport
)

