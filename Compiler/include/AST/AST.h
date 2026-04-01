// AST.h - 语法分析器声明
#ifndef AST_H
#define AST_H

#include "../Token.h"
#include "./ASTnode.h"
#include <vector>
#include <string>

class AST {
private:
    std::vector<Token> tokens;
    size_t currentPos;
    
    // 辅助工具函数：检查当前 Token 是否匹配目标字符串
    bool match(const std::string& target);
    
    // 辅助工具函数：消费当前 Token 并返回
    Token consume();
    
    // 辅助工具函数：预览当前 Token
    Token peek();
    
    // 辅助工具函数：判断是否到达 Token 末尾
    bool isAtEnd();
    
    // 辅助工具函数：期望特定 Token，不匹配则抛出异常
    Token expect(const std::string& target);
    
    // 解析参数列表
    std::vector<std::pair<std::string, std::string>> parseParameters();
    
    // 解析语句块
    ASTBaseNode* parseStatementBlock();
    
    // 解析逻辑或（||）
    ASTBaseNode* parseLogicalOr();
    
    // 解析逻辑与（&&）
    ASTBaseNode* parseLogicalAnd();
    
    // 解析比较运算符
    ASTBaseNode* parseComparison();
    
    // 解析表达式
    ASTBaseNode* parseExpression();
    
    // 解析项
    ASTBaseNode* parseTerm();
    
    // 解析因子
    ASTBaseNode* parseFactor();
    
    // 解析变量声明
    ASTBaseNode* parseVariableDeclaration();
    
    // 解析 if 语句
    ASTBaseNode* parseIfStatement();
    
    // 解析 for 语句
    ASTBaseNode* parseForStatement();
    
    // 解析 syscall 语句（新增）
    ASTBaseNode* parseSyscallStatement();
    
    // 解析函数声明
    ASTBaseNode* parseFunctionDeclaration();
    
    // 解析函数调用
    ASTBaseNode* parseFunctionCall(const std::string& funcName);
    
    // 解析语句
    ASTBaseNode* parseStatement();
    
    // 解析表达式语句（不消耗分号，用于 for 循环的 update 部分）
    ASTBaseNode* parseExpressionStatement();
    
    // 辅助工具：预览下一个 Token
    Token peek(int offset);
    
public:
    AST(const std::vector<Token>& tokenList);
    
    // 构建 AST 根节点
    ASTBaseNode* buildAST();
    
    ~AST() = default;
};

#endif // AST_H
