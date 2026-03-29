// Token.cpp - 词法分析记号实现
#include "Token.h"

std::unordered_map<std::string, TokenType> StringToTokenType = {
    {"int", Keywords}, {"return", Keywords},
    {"if", Keywords}, {"else", Keywords},
    {"for", Keywords}, // 关键字
    
    {"+", Operators}, {"-", Operators},
    {"*", Operators}, {"/", Operators}, // 算符
    {"&&", Operators}, {"||", Operators}, {"!", Operators}, // 添加逻辑运算符
    {">", Operators}, {"<", Operators}, {"==", Operators},
    {"!=", Operators}, {">=", Operators}, {"<=", Operators}, // 新增比较运算符
    {"++", Operators}, // 新增自增算符
    
    {"(", Punctuators}, {")", Punctuators},
    {"[", Punctuators}, {"]", Punctuators},
    {"{", Punctuators}, {"}", Punctuators},
    {";", Punctuators}, {",", Punctuators} // 符号
};

bool isStringDigit(const std::string& str) {
    for (const char& c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    
    return true;
}

TokenType getTokenType(const std::string& str) {
    if (StringToTokenType.find(str) != StringToTokenType.end()) {
        return StringToTokenType[str];
    }
    else if (isStringDigit(str)) {
        return Literals;
    }
    
    return Identifiers;
}

Token::Token() : type(Keywords), content(""), c(0) {}

Token::Token(const std::string& content, int c) : content(content), c(c) {
    type = getTokenType(content);
}

std::pair<TokenType, std::string> Token::getToken() {
    return {type, content};
}

Token::~Token() {
}
