// Token.h - 词法分析记号定义
#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <unordered_map>

enum TokenType {
    Keywords,      // 关键词
    Identifiers,   // 变量/函数
    Literals,      // 常数
    Operators,     // 运算符（双目或单目）
    Punctuators,   // 分隔符
};

extern std::unordered_map<std::string, TokenType> StringToTokenType;

bool isStringDigit(const std::string& str);
TokenType getTokenType(const std::string& str);

class Token {
    TokenType type;
    std::string content;
    int c;
    
public:
    Token();
    Token(const std::string& content, int c);
    std::pair<TokenType, std::string> getToken();
    ~Token();
};

#endif // TOKEN_H
