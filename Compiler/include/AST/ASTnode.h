// ASTnode.h - 抽象语法树节点定义
#ifndef ASTNODE_H
#define ASTNODE_H

#include <vector>
#include <string>
#include <unordered_map>
#include "../Token.h"

#ifndef NULL_ATTRIBUTE_VALUE
    #define NULL_ATTRIBUTE_VALUE "NULLATTRIBUTEVALUE"
#endif

class ASTBaseNode {
public:
    enum NodeType {
        BASE,
        STATEMENT,
        STMT_BLOCK,
        EXPRESSION,
        VAR_DECL,
        FUNC_DECL,
        FUNC_CALL,
        IF_STATEMENT,
        FOR_STATEMENT
    };
    
protected:
    std::vector<ASTBaseNode*> children;
    std::unordered_map<std::string, std::string> attribute;
    NodeType nodeType;
    
public:
    ASTBaseNode();
    virtual ~ASTBaseNode();
    
    void addChild(ASTBaseNode* child);
    void addAttribute(std::string& attributeName, std::string& attributeNum);
    std::string getAttribute(std::string& attributeName);
    std::vector<ASTBaseNode*> getAllChildren();
    NodeType getNodeType();
    bool isLeaf();
};

class Statement : public ASTBaseNode {
public:
    enum StmtType { RETURN, EMPTY };
    StmtType stmtType;
    
    Statement(StmtType type);
    ~Statement();
    
    StmtType getStmtType();
};

class StatementBlock : public ASTBaseNode {
public:
    StatementBlock();
    ~StatementBlock();
};

// 在 ASTnode.h 的 Expression 类中添加
class Expression : public ASTBaseNode {
public:
    enum ExprType { 
        LITERAL,
        IDENTIFIER,
        BINARY_OPERATOR, // 支持算术运算符、逻辑运算符（+、-、*、/、&&、||等）
        FUNC_CALL,
        UNARY_OPERATOR,
        ASSIGN_OPERATOR // 新增：赋值运算符
    };
    
    ExprType exprType;
    std::string value; // 用于字面量、标识符或运算符
    Expression* operand; // 新增：单目运算符的操作数（如自增的变量）
    Expression* left; // 左操作数（二元运算时）
    Expression* right; // 右操作数（二元运算时）
    
    // 构造函数：字面量/标识符
    Expression(ExprType type, const std::string& val);
    
    // 构造函数：二元运算符
    Expression(ExprType type, const std::string& op, Expression* l, Expression* r);
    
    // 新增：单目运算符构造函数（自增等）
    Expression(ExprType type, const std::string& op, Expression* opnd);
    
    ~Expression();
};

class VariableDeclaration : public ASTBaseNode {
public:
    std::string varType;
    std::string varName;
    Expression* initExpr; // 新增：存储初始化表达式
    
    VariableDeclaration(const std::string& type, const std::string& name, Expression* init = nullptr);
    ~VariableDeclaration();
};

// 在 FunctionDeclaration 类中添加参数存储
class FunctionDeclaration : public ASTBaseNode {
public:
    std::string returnType;
    std::string funcName;
    std::vector<std::pair<std::string, std::string>> parameters; // 新增：存储参数类型和名称
    
    FunctionDeclaration(const std::string& retType, const std::string& name,
                       const std::vector<std::pair<std::string, std::string>>& params);
    ~FunctionDeclaration();
};

class FunctionCall : public Expression {
public:
    std::string funcName;
    std::vector<Expression*> parameters; // 新增：存储函数调用参数
    
    FunctionCall(const std::string& name);
    
    // 新增：添加参数方法
    void addParameter(Expression* param);
    
    ~FunctionCall();
};

class IfStatement : public ASTBaseNode {
public:
    Expression* condition; // if 条件表达式
    ASTBaseNode* thenBlock; // 条件为真时的语句块
    ASTBaseNode* elseBlock; // 可选的 else 语句块（可为 nullptr）
    
    IfStatement(Expression* cond, ASTBaseNode* thenStmt, ASTBaseNode* elseStmt = nullptr);
    ~IfStatement();
};

// 添加 ForStatement 类
class ForStatement : public ASTBaseNode {
public:
    ASTBaseNode* initStmt; // 初始化语句
    Expression* condition; // 条件表达式
    ASTBaseNode* updateStmt; // 更新语句
    ASTBaseNode* body; // 循环体
    
    ForStatement(ASTBaseNode* init, Expression* cond, ASTBaseNode* update, ASTBaseNode* b);
    ~ForStatement();
};

#endif /* ASTNODE_H */
