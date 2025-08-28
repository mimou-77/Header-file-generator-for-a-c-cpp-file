#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h" //to use: clang::tooling::FixedCompilationDatabase
#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/PPCallbacks.h" //used by include collector
#include "clang/Lex/Preprocessor.h" //used by include collector


#include <set>
#include <string>
#include <fstream>
#include <vector>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

/*-----------------------------------------------------------------------------------------------*/
/* classes                                                                             */
/*-----------------------------------------------------------------------------------------------*/

// IncludeCollector : A preprocessor callbacks set that finds and stores all #include directives in its member: RequiredHeaders
class IncludeCollector : public PPCallbacks {
private:
    std::set<std::string> &RequiredHeaders;
    const SourceManager &SM;

public:
    //ctor
    explicit IncludeCollector(std::set<std::string> &headers, const SourceManager &SM) : RequiredHeaders(headers), SM(SM) {}

    // This callback is triggered for every #include directive.
    // It will be called by the Preprocessor when it encounters an #include.
    void InclusionDirective(SourceLocation HashLoc,
                            const Token &IncludeTok,
                            StringRef FileName,
                            bool IsAngled,
                            CharSourceRange FilenameRange,
                            OptionalFileEntryRef File,
                            StringRef SearchPath,
                            StringRef RelativePath,
                            const clang::Module *SuggestedModule,
                            bool ModuleImported,
                            SrcMgr::CharacteristicKind FileType) override {

        if (SM.isInMainFile(HashLoc)) //collect only the includes which # is in the .c file
        {
        // Collect the header name in the correct format (<...> or "...").
        std::string headerStr = IsAngled ? "<" + FileName.str() + ">" : "\"" + FileName.str() + "\"";
        RequiredHeaders.insert(headerStr);
        }
    }

    const std::set<std::string>& getHeaders() const {
        return RequiredHeaders;
    }
};




// FunctionDeclCollector : an AST visitor that parses the AST of the .c file and:
//      - collects functions declarations and stores them as a string set in the member: FunctionDeclarations
//
// when the (inherited) method: TraverseDecl() is called, it parses the AST and executes 
// the cb: VisitFunctionDecl() each time it encounters a function declaration. This cb does the processing (aka:
// populating FunctionDeclarations and RequiredHeaders)
// members: Context (from Tool), FunctionDeclarations, MainFilePath (from Tool)
class FunctionDeclCollector : public RecursiveASTVisitor<FunctionDeclCollector> {
private:
    ASTContext &Context; //AST + other things
    std::set<std::string> FunctionDeclarations;
    std::string MainFilePath;

public:
    //constructor
    explicit FunctionDeclCollector(ASTContext &Context, StringRef MainFile)
        : Context(Context), MainFilePath(MainFile.str()) {}

    // cb; 
    // there is an inherited method: Visitor.TraverseDecl(); 
    // when it's called as Visitor.TraverseDecl(Context.getTranslationUnitDecl()): this cb is executed each time
    // the visitor (this object) encounters a function declaration in the AST
    bool VisitFunctionDecl(FunctionDecl *F) {
        
        // leave the cb if the declaration isnt in MainFile (could be included from another file 
        // after the preprocessing)
        if (!Context.getSourceManager().isInMainFile(F->getLocation())) {
            return true;
        }

        // Get the PrintingPolicy from the AST context to get source-level type names
        const PrintingPolicy &PP = Context.getPrintingPolicy();

        //get function name and return type and storage class
        std::string ReturnType = F->getReturnType().getAsString(PP);
        std::string FunctionName = F->getNameAsString();
        std::string ParamsStr;
        std::string StorageClass = ""; //storage class can be: static or extern
        
        if (F->getStorageClass() == SC_Static) {
            StorageClass = "static ";
        }
        else 
        if (F->getStorageClass() == SC_Extern)
        {
            StorageClass = "extern ";
        }
        
        //fill the ParamsStr
        for (unsigned i = 0; i < F->getNumParams(); ++i) 
        {
            ParmVarDecl *Param = F->getParamDecl(i);
            
            ParamsStr += Param->getType().getAsString(PP);
            if (!Param->getNameAsString().empty()) {
                ParamsStr += " ";
                ParamsStr += Param->getNameAsString();
            }
            if (i < F->getNumParams() - 1) {
                ParamsStr += ", ";
            }
        }

        // A function is variadic only if it explicitly has '...' in its declaration.
        if (F->isVariadic()) {
             if (!ParamsStr.empty()) {
                ParamsStr += ", ";
            }
            ParamsStr += "...";
        }


        std::string declaration = StorageClass + ReturnType + " " + FunctionName + "(" + ParamsStr + ");";
        FunctionDeclarations.insert(declaration);

        return true;
    }

    const std::set<std::string>& getDeclarations() const {
        return FunctionDeclarations;
    }

};

// HeaderGeneratorConsumer : an AST consumer that :
//      - 
// members: Visitor, OutputFilePath (Visitor is a FunctionDeclCollector), CI, RequiredHeaders
class HeaderGeneratorConsumer : public ASTConsumer {
private:
    FunctionDeclCollector Visitor;
    std::string OutputFilePath;
    CompilerInstance &CI;
    std::set<std::string> RequiredHeaders;

public:
    //ctor
    explicit HeaderGeneratorConsumer(CompilerInstance &CI, StringRef MainFile, StringRef OutputFile)
        : Visitor(CI.getASTContext(), MainFile), OutputFilePath(OutputFile.str()), CI(CI)
    {
        // Register the IncludeCollector as a preprocessor callback
        // This is the correct way to pass ownership of the unique_ptr
        CI.getPreprocessor().addPPCallbacks(std::make_unique<IncludeCollector>(RequiredHeaders, CI.getSourceManager()));
    }

    //this method uses the visitor to collect declarations and store them in the member: FunctionDeclarations
    //then writes them into a .h file and adds the headerguard
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());

        std::vector<std::string> declarations(Visitor.getDeclarations().begin(), Visitor.getDeclarations().end());
        std::sort(declarations.begin(), declarations.end());

        std::ofstream HeaderFile(OutputFilePath);
        if (!HeaderFile.is_open()) {
            errs() << "Error: Could not open output file " << OutputFilePath << "\n";
            return;
        }

        std::string HeaderGuard = OutputFilePath;
        std::transform(HeaderGuard.begin(), HeaderGuard.end(), HeaderGuard.begin(), ::toupper);
        std::replace(HeaderGuard.begin(), HeaderGuard.end(), '.', '_');
        std::replace(HeaderGuard.begin(), HeaderGuard.end(), '-', '_');
        std::replace(HeaderGuard.begin(), HeaderGuard.end(), '/', '_');
        std::replace(HeaderGuard.begin(), HeaderGuard.end(), '\\', '_');
        HeaderGuard += "_";

        HeaderFile << "#ifndef " << HeaderGuard << "\n";
        HeaderFile << "#define " << HeaderGuard << "\n\n";

    
        //write all the collected headers from the .c in the .h
        for (const auto &header : RequiredHeaders)
        {
            HeaderFile << "#include " << header << "\n";
        }
        
        HeaderFile << "\n";
        
        for (const std::string& decl : declarations) {
            HeaderFile << decl << "\n";
        }

        HeaderFile << "\n#endif // " << HeaderGuard << "\n";

        HeaderFile.close();

        outs() << "Generated " << OutputFilePath << " successfully.\n";
    }
};


// HeaderGeneratorFrontendAction : an AST Frontend Action that can:
//      - create an object from class: HeaderGeneratorConsumer (the one that creates the .h file)
//      - set the compilator to use C17
// members: OutputFilePath
class HeaderGeneratorFrontendAction : public ASTFrontendAction {
private:
    std::string OutputFilePath;

public:
    HeaderGeneratorFrontendAction(StringRef OutputFile) : OutputFilePath(OutputFile.str()) {}

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override {
        CI.getLangOpts().C17 = true;
        CI.getLangOpts().CPlusPlus = false;
        return std::make_unique<HeaderGeneratorConsumer>(CI, InFile, OutputFilePath);
    }
};


// HeaderGeneratorFrontendActionFactory : can create a HeaderGeneratorFrontendAction
// members: OutputFilePath
class HeaderGeneratorFrontendActionFactory : public FrontendActionFactory {
private:
    std::string OutputFilePath;

public:
    HeaderGeneratorFrontendActionFactory(StringRef OutputFile) : OutputFilePath(OutputFile.str()) {}

    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<HeaderGeneratorFrontendAction>(OutputFilePath);
    }
};




/*-----------------------------------------------------------------------------------------------*/
/* main function                                                                             */
/*-----------------------------------------------------------------------------------------------*/

// typed command must have the format:
// generate_header_tool my_file.c
// OR
// generate_header_tool my_file.c -o my_wow_file.h
int main(int argc, const char **argv) {
    
    // checks if the .c file is mentionned in the typed command
    if (argc < 2) {
        llvm::errs() << "Error: No source file specified.\n";
        return 1;
    }

    //1st arg is the InputFile
    std::string InputFile = argv[1];
    std::string HFileName;

    //if there is "-o" in the argv : take the following argv parameter as the HFileName
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-o" && i + 1 < argc) {
            HFileName = argv[i + 1];
            break;
        }
    }

    //if the .h file name isnt mentioned in the typed comman, derive it from the .c file name.
    if (HFileName.empty()) {
        size_t dot_pos = InputFile.rfind('.');
        if (dot_pos != std::string::npos) {
            HFileName = InputFile.substr(0, dot_pos) + ".h";
        } else {
            HFileName = InputFile + ".h";
        }
    }


    std::vector<std::string> SourceFiles = {InputFile};
    //CWD: current working directory : where to find the src files
    std::string CWD = ".";
    //fixed compilation flags: "-x", "c" : treat code as C code
    //-I: added include paths for standard C++ headers
    std::vector<std::string> Flags = {"-std=c17", "-x", "c", "-I/usr/include/c++/17", "-I/usr/include/x86_64-linux-gnu/c++/17", "-I/usr/include"};
    Flags.push_back("-I.");

    //create a clang tool from .c file and FixedCompilationDatabase ;
    // the created clang tool has the following infos: CompilerInstance + InFile  
    clang::tooling::FixedCompilationDatabase Compilations(CWD, Flags);
    clang::tooling::ClangTool Tool(Compilations, SourceFiles);

    
    // This line of code initiates the entire tool execution process.
// It can be broken down into the following steps:
//
// 1. `std::make_unique<HeaderGeneratorFrontendActionFactory>(HFileName)`:
//    - This creates a smart pointer (`std::unique_ptr`) to a new instance of
//      `HeaderGeneratorFrontendActionFactory`, passing the desired output
//      header filename (`HFileName`) to its constructor.
//
// 2. `.get()`:
//    - This retrieves the raw pointer from the smart pointer. `Tool.run()`
//      expects a raw pointer to a `FrontendActionFactory`.
//
// 3. `Tool.run(...)`:
//    - The `ClangTool` object's `run()` method takes the factory and begins
//      the compilation process. It handles the low-level details of setting up
//      the compiler and parsing the source file (`InputFile`).
//
//    - During the run, the following callbacks and actions occur in order:
//      a. **Factory creates Action**: `Tool.run()` calls the factory's `create()`
//         method, which returns a `std::unique_ptr<HeaderGeneratorFrontendAction>`.
//         This action object is responsible for the overall task.
//
//      b. **Action creates Consumer**: The `Tool.run()` method then calls the
//         action's `CreateASTConsumer()` method. It provides a `CompilerInstance`
//         object (`CI`), which contains the state of the compiler, including
//         the preprocessor and the `ASTContext`.
//
//      c. **Preprocessor runs and collects includes**: Within the `HeaderGeneratorConsumer`'s
//         constructor, an `IncludeCollector` is added to the preprocessor. CLANG
//         runs the preprocessor, and the `IncludeCollector`'s `InclusionDirective`
//         callback is executed for every `#include` directive, populating the
//         `RequiredHeaders` set.
//
//      d. **AST is built**: After preprocessing, CLANG parses the code and builds
//         the Abstract Syntax Tree (AST).
//
//      e. **Consumer processes AST**: `Tool.run()` calls the consumer's
//         `HandleTranslationUnit()` method, passing the newly created `ASTContext`.
//         This is where your custom logic takes over.
//
//         - `Visitor.TraverseDecl()`: The `HandleTranslationUnit` method
//           initiates a traversal of the AST using the `FunctionDeclCollector`
//           visitor.
//
//         - `VisitFunctionDecl()`: As the visitor encounters each function
//           declaration in the AST, its `VisitFunctionDecl()` callback is
//           executed. This callback extracts the function's details and adds
//           its declaration string to the `FunctionDeclarations` set.
//
//      f. **Header is written**: After the traversal is complete, the
//         `HandleTranslationUnit` method takes the collected `RequiredHeaders`
//         and `FunctionDeclarations` and writes them, along with the header
//         guard, to the specified output file (`HFileName`).
    return Tool.run(std::make_unique<HeaderGeneratorFrontendActionFactory>(HFileName).get());
}