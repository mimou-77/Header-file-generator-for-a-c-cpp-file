# Generate_header_tool

> [!IMPORTANT]
> Linux Debian

This tool generates a .h file from a .c file or a .cpp file, using CLANG.

The generated .h file contains :

- the functions declarations of the functions defined in the .c file
- the #includes in the .c file
  
---

### → How it works:

- `(Tool.run())` : starts by executing the CLANG preprocessor on the .c file. Via a custom __preprocessor callback__ we created, It collects the includes and stores them as a set of strings.

- Then CLANG parses the .c file and generate its AST.

- A __RecursiveASTVisitor__ traverses the nodes of the AST, when it encounters a function declaration, it extracts its infos(return type, name, parameter, ...) and stores them as a string in a set of strings.

- An __ASTConsumer__ uses the infos collected by the __preprocessor callback__ and the __RecursiveASTVisitor__ to generate the .h file ; this .h file contains the includes copied from the .c file and the functions declarations as well as a header guard.

---

## Build the haeder_generator_tool from src : 

### 1. PREREQUISITES: To include the .h files at the beginning of the main.cpp : we need to install clang/llvm dev packages:

> wget https://apt.llvm.org/llvm.sh
 
> chmod +x llvm.sh

> sudo ./llvm.sh 19  # Or 'sudo ./llvm.sh all' for the very latest version

> sudo apt update

> sudo apt install clang-19 clang-tools-19 libclang-19-dev libllvm-19-dev lldb-19 lld-19 libc++-19-dev libc++abi-19-dev


### 2. To build the executable "generate_header_tool"

> mkdir build

> cd build

> cmake -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -DClang_DIR=/usr/lib/llvm-19/lib/cmake/clang ..

> make

### 3. Test on the f1.c file

(you are in: build/)

> ./generate_header_tool ../f1.c

 
### 4. Create a symbolic link to be able to execute the command "generate_header_tool" from any place

> sudo ln -s /path/to/your/tool/generate_header_tool /usr/local/bin/generate_header_tool

Now you can create another .c file (example: ~/my_file.c) and do: 

(example: you are in : ~/)

> generate_header_tool my_file.c

