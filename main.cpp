
/**
 * @note this tool pre-processes the file and generates AST, THEN does its work
 */

// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻ How this tool works ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
/*

1. in main():

    - HFileName : from argv (the arg following '-o')
    - InputFile : argv[1]

    - ClangTool Tool : from fixed compilation options and InputFile

    Tool.run(std::make_unique<HeaderGeneratorFrontendActionFactory>(HFileName).get());
    => 1. "get()" : creates a HeaderGeneratorFrontendActionFactory object from HFileName
       2. Tool : contains src_file name and output_file name
       3. Tool.run :
            - executes the method "create()" of the class HeaderGeneratorFrontendActionFactory that creates a HeaderGeneratorFrontendAction obj from 
            output_file
            - executes the method "CreateASTConsumer()" on the created HeaderGeneratorFrontendAction obj :
            this method creates a HeaderGeneratorConsumer obj from CompilerInstance and InFile ; 
            this created object has a member : Visitor (its class is FunctionDeclCollector) that is created with the constructor from InFile
            - executes the method HandleTranslationUnit() on the created HeaderGeneratorConsumer obj :
            this method HandleTranslationUnit() :
            /////////////////////////////////////
                ++ executes : Visitor.TraverseDecl() : populates the member Visitor.FunctionDeclarations by calling FunctionDeclCollector::VisitFunctionDecl
                that collects functions declarations
                ++ calls Visitor.getDeclarations() to get the functions declarations from the member Visitor.FunctionDeclarations
                now we have the functions declarations
                ++ creates the output file and writes the functions declarations in it
            /////////////////////////////////////
*/
// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻






#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/Support/CommandLine.h" // For cl::extrahelp

#include "clang/Frontend/CompilerInstance.h" //for clang::CompilerInstance


#include <set>
#include <string>
#include <fstream>
#include <algorithm> // For std::sort

using namespace clang;
using namespace clang::tooling;
using namespace llvm;





/*-----------------------------------------------------------------------------------------------*/
/* classes                                                                             */
/*-----------------------------------------------------------------------------------------------*/

// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
// 1. a RecursiveASTVisitor that parses the AST generated from the InputFile and collects functions declarations from the AST and formats them
// into a string (string = declarations separated by ;\n)   
// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
/** 
 * @class FunctionDeclCollector : a special AST visitor to collect functions declarations
 * 
 * @member: Context : generated from MainFilePath
 * @member: FunctionDeclarations : a set of strings, each one is a function declaration inside the .c file
 * @member: MainFilePath : path of the .c file 
 * 
 * @method: FunctionDeclCollector : constructor === creates a FunctionDeclCollector object from Context and MainFilePath
 * @method: VisitFunctionDecl(FunctionDecl *F) : cb that executes each time the declcollector finds a function declaration:
 *          inserts it inside the member: FunctionDeclarations (string inside set_od_strings)
 */
class FunctionDeclCollector : public RecursiveASTVisitor<FunctionDeclCollector>
{

    //attributes
    private:
        ASTContext &Context; // ASTContext = AST + other things ; AST= Abstract Syntax Tree
        std::set<std::string> FunctionDeclarations; //a set of strings 
        std::string MainFilePath;
    

    //methods    
    public: //explicit = object from this class can be created only via constructor + obj must be created
            // before using it

        //constructor for the class "FunctionDeclCollector"
        explicit FunctionDeclCollector(ASTContext &Context, StringRef MainFile) 
        : Context(Context), MainFilePath(MainFile.str()) {} //this is a member initialization list
        

        /**
         * @brief a cb function, executed each time the visitor finds a fucntion decl F :
         *          → what it does: inserts the function declaration string inside the member: FunctionDeclarations
         *          
         *          FunctionDeclarations: contains all fns declarations in the .c file (taken from AST)
         *          
         *          - function declaration string has this format:
         *          "return_type function_name(p1_type p1_name, p2_type p2_name);"
         *          or , for a variadic function:
         *          "return_type function_name(p1_type p1_name, p2_type p2_name, ...);"
         *          or if all params can vary in nbr: "return_type function_name(...);"
         *          
         * @param F 
         * @return true : to tell the calling function to keep going to get the next fucntion declaration
         */
        bool VisitFunctionDecl(FunctionDecl *F)
        {
            //SourceManager is part of ASTContext; SourceManager processes src files
            //F->getLocation() returns position in file where declaration F is found
            //isInMainFile() tells if the location is in the file we are processing
            if(!Context.getSourceManager().isInMainFile(F->getLocation()))
            {
                return true;
            }

            //get return type of the function declaration F
            std::string ReturnType = F->getReturnType().getAsString();

            //get function name of the function declaration F
            std::string FunctionName = F->getNameAsString();

            //get fucntion parameters list
            std::string ParamsStr; //format of ParamsStr = "p1_type p1_name, p2_type p2_name, p3_type p3_name ..."
            
            for (unsigned i = 0; i < F->getNumParams(); ++i)
            {
                //get the parameter i (type+name) from the function declaration F
                ParmVarDecl *Param = F->getParamDecl(i);
                //get the type of parameter Parm and append it in string ParamsStr
                ParamsStr += Param->getType().getAsString();
                //if the parameter has a name
                if (!Param->getNameAsString().empty())
                {
                    ParamsStr += " "; //add a space to ParamsStr
                    ParamsStr += Param->getNameAsString(); //add the parameter name to ParamsStr
                }
                //if we didn't reach the last parameter (ie: i < index(last_param) <=> i < (nbr_param -1) )
                if (i < F->getNumParams()-1)
                {
                    ParamsStr += ", "; //separator
                } 
            } //end for

            // Handle variadic functions (example f can do: f(p1, p2) or f(p1))
            if (!ParamsStr.empty()) //if F has at least one parameter
            {
                ParamsStr += "..."; //add this at end of ParamsStr to indication F is variadic
            }

            // Form the declaration string : 
            //      FORMAT =    return_type function_name(p1_type p1_name, p2_type p2_name);
            std::string declaration = ReturnType + " " + FunctionName + "(" + ParamsStr + ");";

            //put declaration inside the member FunctionDeclarations
            FunctionDeclarations.insert(declaration);
            
            //operation done, indicate to calling function to keep going
            return true; 
        }

        //get the string in the member: FunctionDeclarations : AKA: the function declaration of F
        const std::set<std::string>& getDeclarations() const
        {
            return FunctionDeclarations;
        }    
}; //end of class



// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
// 2. a ASTConsumer that processes the parsed AST generated from the InputFile :
// creates the output_file + writes the collected declarations in it ; declarations collected with the FunctionDeclCollector
// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
//AST consumer : is a class that allows you to interact with an AST file
// class HeaderGeneratorConsumer has 2 members: visitor + outputfile string
// visitor is an object class that allows you to traverse with an object structure (here: an AST)
// here the visitor is meant to traverse the ASTContext of MainFile
/**
 * @class HeaderGeneratorConsumer : 
 * 
 * @member: Visitor : a FunctionDeclCollector ↑ {contains among its members: FunctionDeclarations}
 * @member: OutputFilePath : the .h file path
 * 
 * @method: ..constructor
 * @method: HandleTranslationUnit: generates the .h file from the member: visitor {visitor contains among its members: FunctionDeclarations}
 */
class HeaderGeneratorConsumer : public ASTConsumer
{
    //members
    private:
        FunctionDeclCollector Visitor; //object that can parse an AST and return a function declaration string
        std::string OutputFilePath;



    //methods
    public:
        //constructor
        explicit HeaderGeneratorConsumer(ASTContext &Context, StringRef MainFile, StringRef OutputFile)
            : Visitor(Context, MainFile), OutputFilePath(OutputFile.str()) {}

        
        // override: public methods of class child replace the public methods of class parent with the same name
        void HandleTranslationUnit(ASTContext &Context) override
        {
            Visitor.TraverseDecl(Context.getTranslationUnitDecl()); // tells visitor that the start of traversal of AST context must be from  
                                                                    // TranslationUnitDecl = a node of the AST containing all declarations (typedef, 
                                                                    // global vars, functions declarations, classes, namespaces,..)
            
            
            // store in this vector of strings: store the string returned by getDeclarations() :
            // FORMAT =
            // "return_type_f1 function_name_f1(p1_type p1_name, p2_type p2_name);return_type_f2 function_name_f2(p1_type p1_name, p2_type p2_name);"
            std::vector<std::string> declarations(Visitor.getDeclarations().begin(), Visitor.getDeclarations().end());

            //sort the fns declarations string alphabetically // ??? i'm not convinced 
            std::sort(declarations.begin(), declarations.end());

            // create the .h file in the path OutputFilePath
            std::ofstream HeaderFile(OutputFilePath);
            if (!HeaderFile.is_open())
            {
                errs() << "Error: Could not open output file " << OutputFilePath << "\n";
                return;
            }


            //append this to the .h file:
            // #ifndef MY_F1_H_
            // #define MY_F1_H_
            // #include <lib_1.h>
            // #include <lib_2.h>
            // decl_1
            // decl_2
            // ...
            // decl_n
            // #endif // MY_F1_H_

            // ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ 
            //
            //header guard : = the string MY_F1_H_ if your typed ./generate_header_tool ../f1.c -o f1.h -- -std=c17
            //               = the string _/MY_F1_H_ if you typed ./generate_header_tool ../f1.c -o ./f1.h -- -std=c17
            //               = the string __/MY_F1_H_ if you typed ./generate_header_tool ../f1.c -o ../f1.h -- -std=c17
            std::string HeaderGuard = OutputFilePath; // ./f1.h
            std::transform(HeaderGuard.begin(), HeaderGuard.end(), HeaderGuard.begin(), ::toupper); //uppercase the file path => ./F1.H
            std::replace(HeaderGuard.begin(), HeaderGuard.end(), '.', '_'); // _/F1_H
            std::replace(HeaderGuard.begin(), HeaderGuard.end(), '-', '_');
            HeaderGuard += "_"; // F1_H_ 
            HeaderFile << "#ifndef " << HeaderGuard << "\n"; // << : append #ifndef MY_F1_H_ \n  #define MY_F1_H_ \n\n at the end of the file
            HeaderFile << "#define " << HeaderGuard << "\n\n";
            // Minimal standard includes
            HeaderFile << "// Standard includes that might be needed by the declarations\n";
            HeaderFile << "#include <stddef.h> // For size_t, etc.\n";
            HeaderFile << "#include <stdbool.h> // For bool\n";
            HeaderFile << "#include <stdint.h> // For fixed-width integers\n";
            HeaderFile << "\n";

            //append the functions declarations to the .h file
            // for decl in declarations
            for (const std::string& decl : declarations)
            {
                HeaderFile << decl << "\n";
            }

            // append at end of file:
            // #endif // _MY_F1_H_
            HeaderFile << "\n#endif // " << HeaderGuard << "\n"; 

            //close the file inode
            HeaderFile.close();

            //log
            outs() << "Generated " << OutputFilePath << " successfully.\n";
        }
    
}; //end class HeaderGeneratorConsumer


// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
// an ASTFrontendAction that creates the ASTConsumer (2.)
// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
/**
 * @class HeaderGeneratorFrontendAction
 * 
 * @attribute: OutputFilePath
 * @method: CreateASTConsumer : creates an ASTConsumer for a file according to compiler
 *      @param CompilerInstance
 *      @param StringRef InFile
 * 
 */
class HeaderGeneratorFrontendAction : public ASTFrontendAction 
{
    //attributes
    private:
        std::string OutputFilePath;


    //methods
    public:
        //constructor : makes an object from an output_file name
        HeaderGeneratorFrontendAction(StringRef OutputFile) : OutputFilePath(OutputFile.str()) {}

        //CreateASTConsumer: returns an ASTConsumer from file name and according to compiler CI
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override
        {
            // compiler language options is C17 ; C++ is treated as C
            CI.getLangOpts().C17 = true;
            CI.getLangOpts().CPlusPlus = false;

            return std::make_unique<HeaderGeneratorConsumer>(CI.getASTContext(), InFile, OutputFilePath);
        };

}; //end class HeaderGeneratorFrontendAction

// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
// a FrontendActionFactory that creates the ASTFrontendAction ↑
// ☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻☻
//FrontendAction: mainly creates an ASTConsumer: that processes an AST to do things on it like in our case generate a header file
//FrontendActionFactory: a class that creates multiple instances of a frontendaction and for (if you want) multiple src files
// COMPILER FRONTEND: deals with src language (c, cpp...) 
// COMPILER BACKEND: deals with target machine architecture (x86, ARM, MIPS...)
// code > COMPILER FRONTEND > IR > COMPILER BACKEND > Assembly code for a specific processor architecture
class HeaderGeneratorFrontendActionFactory : public FrontendActionFactory
{
    //attributes
    private:
        std::string OutputFilePath;


    //methods
    public:
        //constructor: creates actionfactory from outputfile
        HeaderGeneratorFrontendActionFactory(StringRef OutputFile) : OutputFilePath(OutputFile.str()) {}

        //create the frontend action for the output file path
        std::unique_ptr<FrontendAction> create() override
        {
            return std::make_unique<HeaderGeneratorFrontendAction>(OutputFilePath);
        }
    
};






/*-----------------------------------------------------------------------------------------------*/
/* main function                                                                             */
/*-----------------------------------------------------------------------------------------------*/
int main(int argc, const char **argv)
{
    
    // This solution assumes the tool will be called with a single source file
    // and an optional output file. We will manually parse the command line.
    
    // Check if the source file argument is provided.
    if (argc < 2) {
        llvm::errs() << "Error: No source file specified.\n";
        return 1;
    }

    std::string InputFile = argv[1];
    std::string HFileName;

    //if there is the "-o xxx" in argv 
    // A simple manual loop to check for the '-o' option and recuperate HFileName (the arg that comes right after '-o')
    // This is less robust than llvm::cl::ParseCommandLineOptions but
    // avoids all the setup complexity.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-o" && i + 1 < argc) {
            HFileName = argv[i+1];
            break;
        }
    }

    //if there isn't a -o xxx : generate HFileName from InputFile name
    if (HFileName.empty())
    {
        size_t dot_pos = InputFile.rfind('.');
        if (dot_pos != std::string::npos)
        {   
            HFileName = InputFile.substr(0, dot_pos) + ".h";
        }
        else
        {
            HFileName = InputFile + ".h";
        }
    }

    // Explicitly define the source files and compiler flags.
    std::vector<std::string> SourceFiles = { InputFile };
    std::string CWD = ".";
    std::vector<std::string> Flags = {"-std=c17", "-x", "c"};

    // Create the ClangTool with a FixedCompilationDatabase.
    // This correctly matches one of the available constructors.
    clang::tooling::FixedCompilationDatabase Compilations(CWD, Flags);
    clang::tooling::ClangTool Tool(Compilations, SourceFiles);

    // Run the tool with your custom frontend action factory.
    return Tool.run(std::make_unique<HeaderGeneratorFrontendActionFactory>(HFileName).get());



} //end main
 