// IRbase.h - 中间表示基础定义
#ifndef IRBASE_H
#define IRBASE_H

#include <string>
#include <vector>
#include <unordered_map>

// IR 操作类型（x86 汇编风格）
enum class IROp {
    // 数据传送指令
    MOV,        // mov - 传送数据（如 mov eax, ebx）
    PUSH,       // push - 压栈
    POP,        // pop - 出栈
    
    // 算术运算指令
    ADD,        // add - 加法
    SUB,        // sub - 减法
    IMUL,       // imul - 有符号乘法
    IDIV,       // idiv - 有符号除法
    INC,        // inc - 自增 1
    DEC,        // dec - 自减 1
    NEG,        // neg - 取反
    
    // 逻辑运算指令
    AND,        // and - 逻辑与
    OR,         // or - 逻辑或
    NOT,        // not - 逻辑非
    XOR,        // xor - 异或
    
    // 比较指令
    CMP,        // cmp - 比较
    TEST,       // test - 测试
    
    // 跳转指令
    JMP,        // jmp - 无条件跳转
    JE,         // je - 等于则跳转
    JNE,        // jne - 不等于则跳转
    JL,         // jl - 小于则跳转
    JLE,        // jle - 小于等于则跳转
    JG,         // jg - 大于则跳转
    JGE,        // jge - 大于等于则跳转
    
    // 标签和函数
    LABEL,      // 标签（如 .L1:）
    CALL,       // call - 函数调用
    RET,        // ret - 函数返回
    
    // 其他
    NOP,        // nop - 空操作
    LEA,        // lea - 加载有效地址
    SYSCALL,    // syscall - 系统调用
};

extern std::unordered_map<IROp, std::string> IRopTOStr;

// IR 节点结构（存储一条 IR 指令）
struct IRNode {
    IROp op;                              // 操作类型
    std::vector<std::string> operands;    // 操作数（寄存器/变量/常量/标签）这里采用三址码
    std::string comment;                  // 注释（辅助调试）
    int line;                             // 行号
    
    IRNode(IROp op, const std::vector<std::string>& operands, 
           const std::string& comment = "", int line = 0);
    ~IRNode();
};

#endif // IRBASE_H
