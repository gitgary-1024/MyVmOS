// ASTPrinter.h - AST 打印器声明
#ifndef ASTPRINTER_H
#define ASTPRINTER_H

#include "../AST/ASTnode.h"

class ASTPrinter {
private:
    // 打印缩进
    void printIndent(int depth);
    
    // 递归打印节点
    void printNode(ASTBaseNode* node, int depth);
    
public:
    void printAST(ASTBaseNode* root);
};

#endif // ASTPRINTER_H
