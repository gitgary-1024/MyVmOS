// ASTnode.cpp - AST 节点实现
#include "ASTnode.h"

// ASTBaseNode 实现
ASTBaseNode::ASTBaseNode() : nodeType(BASE) {}

ASTBaseNode::~ASTBaseNode() {
    for (auto child : children) {
        delete child;
    }
}

void ASTBaseNode::addChild(ASTBaseNode* child) {
    children.push_back(child);
}

void ASTBaseNode::addAttribute(std::string& attributeName, std::string& attributeNum) {
    attribute[attributeName] = attributeNum;
}

std::string ASTBaseNode::getAttribute(std::string& attributeName) {
    if (attribute.find(attributeName) != attribute.end()) {
        return attribute[attributeName];
    }
    
    return NULL_ATTRIBUTE_VALUE;
}

std::vector<ASTBaseNode*> ASTBaseNode::getAllChildren() {
    return children;
}

ASTBaseNode::NodeType ASTBaseNode::getNodeType() {
    return nodeType;
}

bool ASTBaseNode::isLeaf() {
    return children.size() == 0;
}

// Statement 实现
Statement::Statement(StmtType type) : stmtType(type) {
    nodeType = STATEMENT;
}

Statement::~Statement() {}

Statement::StmtType Statement::getStmtType() {
    return stmtType;
}

// StatementBlock 实现
StatementBlock::StatementBlock() {
    nodeType = STMT_BLOCK;
}

StatementBlock::~StatementBlock() {}

// Expression 实现
Expression::Expression(ExprType type, const std::string& val)
    : exprType(type), value(val), left(nullptr), right(nullptr), operand(nullptr) {
    nodeType = EXPRESSION;
}

Expression::Expression(ExprType type, const std::string& op, Expression* l, Expression* r)
    : exprType(type), value(op), left(l), right(r), operand(nullptr) {
    nodeType = EXPRESSION;
}

Expression::Expression(ExprType type, const std::string& op, Expression* opnd)
    : exprType(type), value(op), operand(opnd), left(nullptr), right(nullptr) {
    nodeType = EXPRESSION;
}

Expression::~Expression() {
    delete left;
    delete right;
    delete operand; // 释放单目操作数
}

// VariableDeclaration 实现
VariableDeclaration::VariableDeclaration(const std::string& type, const std::string& name, Expression* init)
    : varType(type), varName(name), initExpr(init) {
    nodeType = VAR_DECL;
}

VariableDeclaration::~VariableDeclaration() {
    delete initExpr; // 释放初始化表达式
}

// FunctionDeclaration 实现
FunctionDeclaration::FunctionDeclaration(const std::string& retType, const std::string& name,
                                        const std::vector<std::pair<std::string, std::string>>& params)
    : returnType(retType), funcName(name), parameters(params) {
    nodeType = FUNC_DECL;
}

FunctionDeclaration::~FunctionDeclaration() {}

// FunctionCall 实现
FunctionCall::FunctionCall(const std::string& name)
    : Expression(Expression::FUNC_CALL, name), funcName(name) {
}

void FunctionCall::addParameter(Expression* param) {
    parameters.push_back(param);
}

FunctionCall::~FunctionCall() {
    // 释放参数表达式
    for (auto param : parameters) {
        delete param;
    }
}

// IfStatement 实现
IfStatement::IfStatement(Expression* cond, ASTBaseNode* thenStmt, ASTBaseNode* elseStmt)
    : condition(cond), thenBlock(thenStmt), elseBlock(elseStmt) {
    nodeType = IF_STATEMENT;
}

IfStatement::~IfStatement() {
    delete condition;
    delete thenBlock;
    delete elseBlock;
}

// ForStatement 实现
ForStatement::ForStatement(ASTBaseNode* init, Expression* cond, ASTBaseNode* update, ASTBaseNode* b)
    : initStmt(init), condition(cond), updateStmt(update), body(b) {
    nodeType = FOR_STATEMENT;
}

ForStatement::~ForStatement() {
    delete initStmt;
    delete condition;
    delete updateStmt;
    delete body;
}
