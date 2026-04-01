// File.cpp - 文件处理类实现
#include "File.h"
#include <iostream>
#include <sstream>
#include <fstream>

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

std::string File::compileToAssembly(bool useIntelSyntax, bool withComments) {
    // 创建寄存器映射（32 位 -> 64 位）
    std::unordered_map<std::string, std::string> regMap;
    regMap["eax"] = "rax";
    regMap["ebx"] = "rbx";
    regMap["ecx"] = "rcx";
    regMap["edx"] = "rdx";
    regMap["esi"] = "rsi";
    regMap["edi"] = "rdi";
    regMap["ebp"] = "rbp";
    regMap["esp"] = "rsp";
    
    // 使用 IRToAssembly 生成汇编代码
    IRToAssembly assembler(IRnodes, regMap, useIntelSyntax, withComments);
    return assembler.compile();
}

bool File::compileToBinary(const std::string& outputFile, bool useIntelSyntax) {
    // 1. 生成汇编代码
    std::string assembly = compileToAssembly(useIntelSyntax, false);
    
    // 2. 添加段声明和数据定义
    std::stringstream fullAsm;
    if (useIntelSyntax) {
        fullAsm << "section .data\n";
        fullAsm << "    x: dq 0\n";
        fullAsm << "    y: dq 0\n";
        fullAsm << "    result: dq 0\n";
        fullAsm << "\n";
    }
    fullAsm << assembly;
    
    // 3. 写入临时汇编文件
    std::string tempAsmFile = "temp_output.asm";
    std::ofstream asmOut(tempAsmFile);
    if (!asmOut.is_open()) {
        std::cerr << "Error: Cannot create temporary assembly file" << std::endl;
        return false;
    }
    asmOut << fullAsm.str();
    asmOut.close();
    
    // 4. 调用 NASM 汇编器生成目标文件
    std::string tempObjFile = "temp_output.o";
    std::stringstream nasmCmd;
    nasmCmd << "nasm -f win64 " << tempAsmFile << " -o " << tempObjFile;
    int nasmResult = system(nasmCmd.str().c_str());
    
    if (nasmResult != 0) {
        std::cerr << "Error: NASM assembly failed" << std::endl;
        return false;
    }
    
    // 5. 调用链接器生成可执行文件
    std::stringstream ldCmd;
    ldCmd << "g++ -o " << outputFile << " " << tempObjFile;
    int ldResult = system(ldCmd.str().c_str());
    
    if (ldResult != 0) {
        std::cerr << "Error: Linking failed" << std::endl;
        return false;
    }
    
    // 6. 清理临时文件
    std::remove(tempAsmFile.c_str());
    std::remove(tempObjFile.c_str());
    
    return true;
}

bool File::compileToObjectFile(const std::string& outputFile) {
    // 使用 Assembler 直接生成机器码
    Assembler assembler(IRnodes);
    std::vector<uint8_t> machineCode = assembler.compile();
    
    // 打印机器码（调试用）
    std::cout << "\n=== 生成的机器码 ===" << std::endl;
    assembler.printMachineCode();
    std::cout << std::endl;
    
    // 保存为二进制文件
    if (assembler.saveToFile(outputFile)) {
        std::cout << "✓ 对象文件已生成：" << outputFile << std::endl;
        std::cout << "  大小：" << machineCode.size() << " 字节" << std::endl;
        return true;
    } else {
        std::cerr << "✗ 无法保存对象文件" << std::endl;
        return false;
    }
}
