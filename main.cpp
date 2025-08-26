#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h" //to use: clang::tooling::FixedCompilationDatabase
#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/PPCallbacks.h" //used by include collector
#include "clang/Lex/Preprocessor.h" //used by include collector

// #include "llvm/Support/raw_ostream.h"


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

// IncludeCollector : A preprocessor callback that finds and stores all #include directives in its member: RequiredHeaders
// when the .c file will compile, the types will be correct, because infos about types are in the includes
class IncludeCollector : public PPCallbacks {
private:
    std::set<std::string> &RequiredHeaders;

public:
    //ctor
    explicit IncludeCollector(std::set<std::string> &headers) : RequiredHeaders(headers) {}

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
        // Collect the header name in the correct format (<...> or "...").
        std::string headerStr = IsAngled ? "<" + FileName.str() + ">" : "\"" + FileName.str() + "\"";
        RequiredHeaders.insert(headerStr);
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
// members: Context (from Tool), FunctionDeclarations, MainFilePath (from Tool), RequiredHeaders
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

// HeaderGeneratorConsumer : an AST consumer that takes an object from class: FunctionDeclCollector and uses
// its member FunctionDeclarations
//      - create the .h file, write in it: the #includes + the functions declarations.
//      - print a msg : "Generated XX.h successfully" when end of HandleTranslationUnit() is reached.
// the processing is done via the method: HandleTranslationUnit that calls Visitor.TraverseDecl() [that collects
// the FunctionDeclarations]
// members: Visitor, OutputFilePath (Visitor is a FunctionDeclCollector)
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
        CI.getPreprocessor().addPPCallbacks(std::make_unique<IncludeCollector>(RequiredHeaders));
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

    // 1. create an object from class: HeaderGeneratorFrontendActionFactory from HFileName and return it via
    // the method get() ; Now the created factory object has the member: OutputFilePath = HFileName.
    // 2. the returned object (has class: HeaderGeneratorFrontendActionFactory) is passed as an argument to
    // Tool.run()
    // 3. Tool.run() does the following:
    //      3.1. executes the method create() of the returned object:
    //      - the method create() creates an object from class: HeaderGeneratorFrontendAction fom the value
    //      of factory object member: OutputFilePath (value = HFileName) ; Now the created action object
    //      has the member: OutputFilePath = HFileName.
    //      3.2. executes the method CreateASTConsumer() on the created action object with parameters from
    //      the clang tool object infos (CI + InFile) ; InFile = the .c file, and CI containing the AST context
    //      of the .c file.
    //      - this method CreateASTConsumer(): creates an object from class: HeaderGeneratorConsumer
    //      Now: the created consumer object has has members : Visitor(Context of .c file, MainFile = .c file) and
    //      OutputFilePath(OutputFile = HFileName); 
    //      Visitor is from class: FunctionDeclCollector.
    //      Visitor is a collector.
    //      3.3. calls the method HandleTranslationUnit() of the created consumer object ;
    //      this method does the following:
    //          - calls Visitor.TraverseDecl():
    //                  - collector.TraverseDecl() travereses the AST and calls a cb each time it encounters
    //                  a function declaration; the cb is: VisitFunctionDecl(): it populates the collector 
    //                  member: FunctionDeclarations
    //                  Now we have the functions declarations stored in a string set (FunctionDeclarations)
    //          - creates the .h file in the path in: OutputFilePath = HFileName
    //          - writes inside the .h file:
    //                  -- the HeaderGuard (#ifndef __My_FILE_H ...)
    //                  -- the functions declarations (from FunctionDeclarations) 
    //          - prints a msg (.h created successfully) when end of Visitor.TraverseDecl() is reached
    return Tool.run(std::make_unique<HeaderGeneratorFrontendActionFactory>(HFileName).get());
}