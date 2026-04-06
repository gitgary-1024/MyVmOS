// AST.cpp - 语法分析器实现
#include "AST.h"
#include <stdexcept>

bool AST::match(const std::string& target) {
    if (isAtEnd())
        return false;
        
    auto [type, content] = peek().getToken();
    return content == target;
}

Token AST::consume() {
    if (!isAtEnd())
        return tokens[currentPos++];
        
    throw std::runtime_error("Unexpected end of tokens");
}

Token AST::peek() {
    return tokens[currentPos];
}

bool AST::isAtEnd() {
    return currentPos >= tokens.size();
}

Token AST::expect(const std::string& target) {
    if (match(target))
        return consume();
        
    auto [type, content] = peek().getToken();
    throw std::runtime_error("Unexpected token: " + content + ", expected: " + target);
}

std::vector<std::pair<std::string, std::string>> AST::parseParameters() {
    std::vector<std::pair<std::string, std::string>> params;
    
    // 解析第一个参数
    if (peek().getToken().second == "int") {
        std::string type = consume().getToken().second;
        std::string name = consume().getToken().second;
        params.emplace_back(type, name);
        
        // 解析后续参数（逗号分隔）
        while (match(",")) {
            consume(); // 消耗逗号
            
            if (peek().getToken().second != "int") {
                throw std::runtime_error("Expected int type for parameter");
            }
            
            type = consume().getToken().second;
            name = consume().getToken().second;
            params.emplace_back(type, name);
        }
    }
    return params;
}

ASTBaseNode* AST::parseStatementBlock() {
    StatementBlock* block = new StatementBlock();
    expect("{"); // 消耗左花括号
    
    // 解析块内所有语句直到右花括号
    while (!match("}") && !isAtEnd()) {
        ASTBaseNode* stmt = parseStatement();
        
        if (stmt)
            block->addChild(stmt);
    }
    
    expect("}"); // 消耗右花括号
    return block;
}

ASTBaseNode* AST::parseLogicalOr() {
    ASTBaseNode* node = parseLogicalAnd(); // 先解析逻辑与
    
    while (match("||")) {
        std::string op = consume().getToken().second;
        Expression* right = dynamic_cast<Expression*>(parseLogicalAnd());
        node = new Expression(Expression::BINARY_OPERATOR, op,
                             dynamic_cast<Expression*>(node), right);
    }
    
    return node;
}

ASTBaseNode* AST::parseLogicalAnd() {
    ASTBaseNode* node = parseComparison(); // 改为先解析比较表达式
    
    while (match("&&")) {
        std::string op = consume().getToken().second;
        Expression* right = dynamic_cast<Expression*>(parseComparison()); // 右操作数也是比较表达式
        node = new Expression(Expression::BINARY_OPERATOR, op,
                             dynamic_cast<Expression*>(node), right);
    }
    
    return node;
}

ASTBaseNode* AST::parseComparison() {
    ASTBaseNode* node = parseExpression(); // 先解析算术表达式（+ -）
    
    while (match("==") || match("!=") ||
           match(">") || match("<") ||
           match(">=") || match("<=")) {
        std::string op = consume().getToken().second;
        Expression* right = dynamic_cast<Expression*>(parseExpression()); // 右操作数也是算术表达式
        node = new Expression(Expression::BINARY_OPERATOR, op,
                             dynamic_cast<Expression*>(node), right);
    }
    
    return node;
}

ASTBaseNode* AST::parseExpression() {
    ASTBaseNode* node = parseTerm(); // 先解析高优先级的项
    
    // 循环处理连续的 + 或 -
    while (match("+") || match("-")) {
        std::string op = consume().getToken().second;
        Expression* right = dynamic_cast<Expression*>(parseTerm());
        node = new Expression(Expression::BINARY_OPERATOR, op,
                             dynamic_cast<Expression*>(node), right);
    }
    
    return node;
}

ASTBaseNode* AST::parseTerm() {
    ASTBaseNode* node = parseFactor(); // 先解析因子
    
    // 循环处理连续的 * 或 /
    while (match("*") || match("/")) {
        std::string op = consume().getToken().second;
        Expression* right = dynamic_cast<Expression*>(parseFactor());
        node = new Expression(Expression::BINARY_OPERATOR, op,
                             dynamic_cast<Expression*>(node), right);
    }
    
    return node;
}

ASTBaseNode* AST::parseFactor() {
    Token token = peek();
    auto [type, content] = token.getToken();
    
    // 处理单目逻辑非！，等级最高
    if (content == "!") {
        consume(); // 消耗!
        Expression* operand = dynamic_cast<Expression*>(parseFactor()); // 解析操作数
        return new Expression(Expression::UNARY_OPERATOR, "!", operand);
    }
    
    // 处理括号表达式
    if (content == "(") {
        consume(); // 消耗 '('
        ASTBaseNode* expr = parseLogicalOr(); // 递归解析括号内的表达式（改为最后处理||）
        expect(")"); // 消耗 ')'
        return expr;
    }
    
    // 处理字面量
    if (type == Literals) {
        consume();
        return new Expression(Expression::LITERAL, content);
    }
    
    // 处理标识符或函数调用
    if (type == Identifiers) {
        consume();
        
        if (match("(")) {
            return parseFunctionCall(content); // 函数调用
        }
        
        return new Expression(Expression::IDENTIFIER, content); // 普通标识符
    }
    
    throw std::runtime_error("Unexpected token in factor: " + content);
}

ASTBaseNode* AST::parseVariableDeclaration() {
    Token typeToken = expect("int");
    std::string varType = typeToken.getToken().second;
    Token nameToken = peek();
    auto [nameType, varName] = nameToken.getToken();
    
    if (nameType != Identifiers) {
        throw std::runtime_error("Expected identifier for variable name");
    }
    
    consume(); // 消耗变量名
    Expression* initExpr = nullptr; // 初始化表达式指针
    
    // 处理初始化（允许函数调用作为初始化表达式）
    if (match("=")) {
        consume(); // 消耗 '='
        ASTBaseNode* exprNode = parseLogicalOr(); // 最后处理||
        initExpr = dynamic_cast<Expression*>(exprNode);
        
        // 放宽检查条件，允许任何 Expression 类型（包括函数调用）
        if (!initExpr) {
            throw std::runtime_error("Invalid initializer expression in variable declaration");
        }
    }
    
    expect(";"); // 消耗分号
    // 将 initExpr 传入构造函数
    return new VariableDeclaration(varType, varName, initExpr);
}

// 解析 if 语句：if (condition) thenBlock [else elseBlock]
ASTBaseNode* AST::parseIfStatement() {
    consume(); // 消耗"if"关键词
    expect("("); // 解析左括号
    // 解析条件表达式（支持逻辑运算）
    ASTBaseNode* condNode = parseLogicalOr();
    Expression* condition = dynamic_cast<Expression*>(condNode);
    
    if (!condition) {
        throw std::runtime_error("Invalid condition in if statement");
    }
    
    expect(")"); // 解析右括号
    // 解析 then 分支（单语句或语句块）
    ASTBaseNode* thenBlock = parseStatement();
    
    if (!thenBlock) {
        throw std::runtime_error("Missing statement after if condition");
    }
    
    // 解析可选的 else 分支
    ASTBaseNode* elseBlock = nullptr;
    
    if (match("else")) {
        consume(); // 消耗"else"关键词
        elseBlock = parseStatement(); // else 后的语句或语句块
    }
    
    return new IfStatement(condition, thenBlock, elseBlock);
}

// 在 AST 类中添加 for 循环解析函数
ASTBaseNode* AST::parseForStatement() {
    consume(); // 消耗"for"关键字
    expect("("); // 解析左括号
    // 解析初始化语句
    ASTBaseNode* initStmt = parseStatement();
    
    if (!initStmt) {
        throw std::runtime_error("Missing initialization statement in for loop");
    }
    
    // expect(";"); // 消耗分号（这里不用断言，因为在 parseStatement 中已经断言）
    // 解析条件表达式
    ASTBaseNode* condNode = parseLogicalOr();
    Expression* condition = dynamic_cast<Expression*>(condNode);
    
    if (!condition) {
        throw std::runtime_error("Invalid condition in for loop");
    }
    
    expect(";"); // 消耗分号（这里需要断言，用于跳过分隔符）
    // 解析更新语句（不需要分号的表达式）
    ASTBaseNode* updateStmt = parseExpressionStatement(); // 使用 parseExpressionStatement 来解析完整表达式
    
    if (!updateStmt) {
        throw std::runtime_error("Missing update statement in for loop");
    }
    
    expect(")"); // 解析右括号
    // 解析循环体
    ASTBaseNode* body = parseStatement();
    
    if (!body) {
        throw std::runtime_error("Missing body in for loop");
    }
    
    // expect(";"); // 消耗分号（这里不用断言，因为在 parseStatement 中已经断言）
    return new ForStatement(initStmt, condition, updateStmt, body);
}

// 修改 parseFunctionDeclaration 函数 (形如：func(int amint b))
ASTBaseNode* AST::parseFunctionDeclaration() {
    // 解析返回类型
    Token returnTypeToken = expect("int");
    std::string returnType = returnTypeToken.getToken().second;
    // 解析函数名
    Token nameToken = peek();
    auto [nameType, funcName] = nameToken.getToken();
    
    if (nameType != Identifiers) {
        throw std::runtime_error("Expected function name identifier");
    }
    
    consume(); // 消耗函数名
    expect("("); // 消耗左括号
    std::vector<std::pair<std::string, std::string>> params = parseParameters(); // 解析参数列表
    expect(")"); // 消耗右括号
    // 解析函数体
    ASTBaseNode* body = parseStatementBlock();
    FunctionDeclaration* func = new FunctionDeclaration(returnType, funcName, params);
    func->addChild(body);
    return func;
}

// 解析 syscall 语句（新增，支持多参数）
ASTBaseNode* AST::parseSyscallStatement() {
    consume(); // 消耗 syscall 关键字
    expect("("); // 消耗左括号
    
    // 解析系统调用号表达式
    ASTBaseNode* exprNode = parseLogicalOr();
    Expression* syscallExpr = dynamic_cast<Expression*>(exprNode);
    
    if (!syscallExpr) {
        throw std::runtime_error("Invalid syscall number expression");
    }
    
    // ✅ 新增：解析可选的参数列表
    std::vector<Expression*> arguments;
    if (!match(")")) {  // 如果不是空参数列表
        do {
            ASTBaseNode* argExpr = parseLogicalOr();
            Expression* arg = dynamic_cast<Expression*>(argExpr);
            if (!arg) {
                throw std::runtime_error("Invalid syscall argument expression");
            }
            arguments.push_back(arg);
        } while (match(","));  // 支持逗号分隔的多个参数
        
        expect(")"); // 消耗右括号
    }
    
    expect(";"); // 消耗分号
    
    // 创建 syscall 语句节点（带参数）
    return new SyscallStatement(syscallExpr, arguments);
}

// 解析函数调用
ASTBaseNode* AST::parseFunctionCall(const std::string& funcName) {
    FunctionCall* call = new FunctionCall(funcName);
    expect("("); // 消耗左括号
    
    // 解析参数列表
    if (!match(")")) {
        do {
            ASTBaseNode* paramExpr = parseLogicalOr();//最后处理||
            Expression* param = dynamic_cast<Expression*>(paramExpr);
            
            if (!param) {
                throw std::runtime_error("Invalid parameter in function call");
            }
            
            call->addParameter(param);
            
        } while (match(",") && (consume(), true)); // 处理多个参数
    }
    
    expect(")"); // 消耗右括号
    return call; // 现在返回的是 Expression 子类
}

// 解析语句（支持 return 语句、变量声明、函数调用）
ASTBaseNode* AST::parseStatement() {
    if (isAtEnd())
        return nullptr;
        
    auto [type, content] = peek().getToken();
    
    // 处理 return 语句
    if (content == "return") {
        consume(); // 消耗 return 关键字
        Statement* returnStmt = new Statement(Statement::RETURN);
        
        // 解析 return 后的表达式
        if (!match(";")) {
            ASTBaseNode* expr = parseLogicalOr();//最后处理||
            returnStmt->addChild(expr);
        }
        
        expect(";"); // 消耗分号
        return returnStmt;
    }
    
    // 处理 if 语句（新增）
    if (content == "if") {
        return parseIfStatement();
    }
    
    // 处理变量声明（以 int 关键字开头）
    if (content == "int" && peek(1).getToken().second != "(") {
        return parseVariableDeclaration();
    }
    
    // 处理 for 语句
    if (content == "for") {
        return parseForStatement();
    }
    
    // 处理 syscall 语句（新增）
    if (content == "syscall") {
        return parseSyscallStatement();
    }
    
    // 处理语句块
    if (content == "{") {
        return parseStatementBlock();
    }
    
    // 处理赋值语句（如 sum = sum + i;）
    if (type == Identifiers) {
        // 先记录左操作数（变量名）
        std::string varName = content;
        
        // 检查是否为赋值运算符
        if (peek(1).getToken().second == "=") { // 匹配"="
            consume(); // 消耗变量名
            consume(); // 消耗 "="
            // 解析右操作数表达式（支持逻辑运算、算术运算等）
            ASTBaseNode* rhsNode = parseLogicalOr();
            Expression* rhsExpr = dynamic_cast<Expression*>(rhsNode);
            
            if (!rhsExpr) {
                throw std::runtime_error("Invalid expression in assignment");
            }
            
            // 创建赋值表达式节点（左操作数为标识符，右为表达式）
            Expression* lhsExpr = new Expression(Expression::IDENTIFIER, varName);
            Expression* assignExpr = new Expression(
                Expression::ASSIGN_OPERATOR,
                "=",
                lhsExpr, // 左操作数：变量
                rhsExpr // 右操作数：表达式
            );
            expect(";"); // 消耗分号
            return assignExpr;
        }
        
        // 处理函数调用语句（以标识符开头，后跟 ()）
        if (peek(1).getToken().second == "(") {
            std::string funcName = content;
            consume(); // 消耗函数名
            ASTBaseNode* call = parseFunctionCall(funcName);
            expect(";"); // 函数调用后加分号
            return call;
        }
        
        // 处理后置自增表达式 i++
        if (peek(1).getToken().second == "++") {
            std::string varName = content;
            consume(); // 消耗变量名
            consume(); // 消耗++
            expect(";"); // 自增语句以分号结束
            // 创建自增表达式节点
            Expression* varExpr = new Expression(Expression::IDENTIFIER, varName);
            return new Expression(Expression::UNARY_OPERATOR, "++", varExpr);
        }
    }
    
    throw std::runtime_error("Unexpected statement token: " + content);
}

// 解析表达式语句（不消耗分号，用于 for 循环的 update 部分）
ASTBaseNode* AST::parseExpressionStatement() {
    if (isAtEnd())
        return nullptr;
        
    auto [type, content] = peek().getToken();
    
    // 处理赋值表达式（如 i = i + 1）
    if (type == Identifiers) {
        // 先记录左操作数（变量名）
        std::string varName = content;
        
        // 检查是否为赋值运算符
        if (peek(1).getToken().second == "=") { // 匹配"="
            consume(); // 消耗变量名
            consume(); // 消耗 "="
            // 解析右操作数表达式（支持逻辑运算、算术运算等）
            ASTBaseNode* rhsNode = parseLogicalOr();
            Expression* rhsExpr = dynamic_cast<Expression*>(rhsNode);
            
            if (!rhsExpr) {
                throw std::runtime_error("Invalid expression in assignment");
            }
            
            // 创建赋值表达式节点（左操作数为标识符，右为表达式）
            Expression* lhsExpr = new Expression(Expression::IDENTIFIER, varName);
            Expression* assignExpr = new Expression(
                Expression::ASSIGN_OPERATOR,
                "=",
                lhsExpr, // 左操作数：变量
                rhsExpr // 右操作数：表达式
            );
            return assignExpr;
        }
        
        // 处理后置自增表达式 i++
        if (peek(1).getToken().second == "++") {
            std::string varName = content;
            consume(); // 消耗变量名
            consume(); // 消耗++
            // 创建自增表达式节点（不消耗分号）
            Expression* varExpr = new Expression(Expression::IDENTIFIER, varName);
            return new Expression(Expression::UNARY_OPERATOR, "++", varExpr);
        }
        
        // 处理函数调用（以标识符开头，后跟 ()）
        if (peek(1).getToken().second == "(") {
            std::string funcName = content;
            consume(); // 消耗函数名
            return parseFunctionCall(funcName);
        }
        
        // 简单变量引用（如 i）
        return new Expression(Expression::IDENTIFIER, varName);
    }
    
    // 处理数字常量
    if (type == Literals) {
        return new Expression(Expression::LITERAL, content);
    }
    
    // 默认使用 parseLogicalOr 解析表达式
    return parseLogicalOr();
}

// 辅助工具：预览下一个 Token（用于判断函数调用）
Token AST::peek(int offset) {
    if (currentPos + offset >= tokens.size()) {
        return Token("", -1); // 返回空 Token 表示越界
    }
    
    return tokens[currentPos + offset];
}

// 构造函数
AST::AST(const std::vector<Token>& tokenList) : tokens(tokenList), currentPos(0) {}

// 构建 AST 根节点
ASTBaseNode* AST::buildAST() {
    StatementBlock* root = new StatementBlock(); // 根节点为语句块
    
    // 解析所有顶级语句（函数声明、全局变量等）
    while (!isAtEnd()) {
        auto [type, content] = peek().getToken();
        
        // 解析函数声明（int + 标识符 + (）
        if (content == "int" && peek(1).getToken().second != ";" &&
            peek(2).getToken().second == "(") {
            ASTBaseNode* func = parseFunctionDeclaration();
            root->addChild(func);
        }
        else {
            // 解析其他语句
            ASTBaseNode* stmt = parseStatement();
            
            if (stmt)
                root->addChild(stmt);
        }
    }
    
    return root;
}
