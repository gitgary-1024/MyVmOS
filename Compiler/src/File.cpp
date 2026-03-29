// File.cpp - 文件处理类实现
#include "File.h"
#include <iostream>

const std::string symbolList = "+-*/=(){}[];,&|!><";

bool issymbol(char c) {
    return symbolList.find(c) != symbolList.npos;
}

bool islitter(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

void File::format() {
    std::string line = "";
    
    while (getline(codeFile, line)) {
        for (size_t i = 0; i < line.size(); i++) {
            tempFile << line[i];
            bool isMultiCharOp = false;
            
            // 检查是否为多字符逻辑运算符（如&&、||）
            if (i + 1 < line.size()) {
                std::string twoChars = std::string(1, line[i]) + std::string(1, line[i + 1]);
                
                // 包含逻辑运算符和比较运算符
                if (twoChars == "&&" || twoChars == "||" ||
                    twoChars == "==" || twoChars == "!=" ||
                    twoChars == ">=" || twoChars == "<=" ||
                    twoChars == "++") {
                    isMultiCharOp = true;
                }
            }
            
            // 仅在不是多字符运算符的情况下，才添加空格分隔
            if (issymbol(line[i])) {
                if ((i + 1 < line.size()) && isdigit(line[i + 1]) && !isMultiCharOp) {
                    tempFile << " ";
                }
                
                if ((i + 1 < line.size()) && islitter(line[i + 1]) && !isMultiCharOp) {
                    tempFile << " ";
                }
                
                if ((i + 1 < line.size()) && issymbol(line[i + 1]) && !isMultiCharOp) {
                    tempFile << " ";
                }
            }
            
            if (isdigit(line[i])) { // 数字
                if ((i + 1 < line.size()) && issymbol(line[i + 1])) { // 符号
                    tempFile << " ";
                }
            }
            
            if (islitter(line[i])) { // 字母
                if ((i + 1 < line.size()) && issymbol(line[i + 1])) { // 符号
                    tempFile << " ";
                }
            }
        }
        
        tempFile << "\n";
    }
}

void File::getAllToken() {
    tempFile.seekg(0, std::ios::beg);
    std::string content = "";
    int c = 0;
    
    while (tempFile >> content) {
        TokenList.push_back({content, c});
        c += content.size() + 1;
    }
}

void File::compileAST() {
    AST ast(TokenList);
    ASTroot = ast.buildAST();
}

void File::compileIR() {
    IR ir(ASTroot);
    IRnodes = ir.compileToIR();
}

File::File() {}

File::File(const std::string& fileName) {
    codeFile.open(fileName);
    tempFile.open("temp.txt", std::ios::out | std::ios::in | std::ios::trunc);
    compile();
}

void File::load(const std::string& fileName) {
    codeFile.open(fileName);
    tempFile.open("temp.txt", std::ios::out | std::ios::in | std::ios::trunc);
}

File::~File() {
    codeFile.close();
    tempFile.close();
}

void File::compile() {
    format();                 // 格式化
    getAllToken();           // 转为 token 形式
    compileAST();            // 转为 AST
#ifdef _DEBUG
    printAST();
#endif
    compileIR();
#ifdef _DEBUG
    printIR();
#endif
}

#ifdef _DEBUG
void File::printAST() {
    if (ASTroot) {
        ASTPrinter printer;
        printer.printAST(ASTroot); // 调用 ASTPrinter 打印 AST 结构
    }
    else {
        std::cout << "No AST generated." << std::endl;
    }
}

void File::printIR() {
    if (IRnodes.size() != 0) {
        IRprinter printIR(IRnodes);
        printIR.print();
    }
    else {
        std::cout << "No IR generated." << std::endl;
    }
}

std::vector<Token> File::returnAllToken() {
    return TokenList;
}
#endif

std::vector<IRNode>& File::ir() {
    return IRnodes;
}
