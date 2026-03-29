// File.h - 文件处理类声明
#ifndef FILE_H
#define FILE_H

#include <fstream>
#include <string>
#include <vector>
#include "./Token.h"
#include "./AST/ASTnode.h"
#include "./AST/AST.h"
#include "./AST/ASTPrinter.h"
#include "./IR/IR.h"
#include "./IR/IRprinter.h"
#include "./IR/IRToAssembly.h"

class File {
    std::fstream codeFile;
    std::fstream tempFile;
    std::vector<Token> TokenList;
    ASTBaseNode* ASTroot;
    std::vector<IRNode> IRnodes;
    
    void format();
    void getAllToken();
    void compileAST();
    void compileIR();
    
public:
    File();
    File(const std::string& fileName);
    
    void load(const std::string& fileName);
    ~File();
    
    void compile();
    std::string compileToAssembly(bool useIntelSyntax = true, bool withComments = true);
    bool compileToBinary(const std::string& outputFile, bool useIntelSyntax = true);
    
#ifdef _DEBUG
    void printAST();
    void printIR();
    std::vector<Token> returnAllToken();
#endif
    
    std::vector<IRNode>& ir();
};

#endif // FILE_H
