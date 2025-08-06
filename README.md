# Generate_header_tool

> [!IMPORTANT]
> Linux Debian

This tool generates a .h file from a .c file using CLANG and LLVM.

---

`#### CLANG:`

Is the frontend, used to parse src code and convert it to LLVM formats.

`#### LLVM:`

Is the backend, used to optimize and generate code.

---

### → How it works:

- Here: we use CLANG to parse the .c file and generate its AST.

- RecursiveASTVisitor traverses the nodes of the AST, when it encounters a function declaration, it extracts its infos(return type, name, parameter, ...).

- We use the extracted infos to create a string forming the function header to be included in the .h file. Format of the string like: int my_fn(int a, int b);

- We use LLVM libraries to create the .h file and write the declarations and other things into it.

---

## Build the haeder_generator_tool from src : 

### 1. To include the .h files at the beginning of the main.cpp : we need to install clang/llvm dev packages:

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

> ./generate_header_tool ../f1.c -o f1.h 

> [!note]
> A modification in the main.cpp file can be done to add reading compilation commands from the command line. (check line 399 of main.cpp)
>
>> Example :
>> 
>> ./generate_header_tool ../f1.c -o f1.h -- -std=c17 
 
### 4. Create a symbolic link to be able to execute the command "generate_header_tool" from any place

> sudo ln -s /path/to/your/tool/generate_header_tool /usr/local/bin/generate_header_tool

Now you can create another .c file (example: ~/my_file.c) and do: 

> generate_header_tool my_file.c -o my_file.h

