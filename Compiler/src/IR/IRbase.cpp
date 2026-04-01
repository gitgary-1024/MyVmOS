// IRbase.cpp - IR 基础实现
#include "IRbase.h"

std::unordered_map<IROp, std::string> IRopTOStr = {
    // 数据传送指令
    {IROp::MOV, "mov"},
    {IROp::PUSH, "push"},
    {IROp::POP, "pop"},
    
    // 算术运算指令
    {IROp::ADD, "add"},
    {IROp::SUB, "sub"},
    {IROp::IMUL, "imul"},
    {IROp::IDIV, "idiv"},
    {IROp::INC, "inc"},
    {IROp::DEC, "dec"},
    {IROp::NEG, "neg"},
    
    // 逻辑运算指令
    {IROp::AND, "and"},
    {IROp::OR, "or"},
    {IROp::NOT, "not"},
    {IROp::XOR, "xor"},
    
    // 比较指令
    {IROp::CMP, "cmp"},
    {IROp::TEST, "test"},
    
    // 跳转指令
    {IROp::JMP, "jmp"},
    {IROp::JE, "je"},
    {IROp::JNE, "jne"},
    {IROp::JL, "jl"},
    {IROp::JLE, "jle"},
    {IROp::JG, "jg"},
    {IROp::JGE, "jge"},
    
    // 标签和函数
    {IROp::LABEL, "LABEL"},
    {IROp::CALL, "call"},
    {IROp::RET, "ret"},
    
    // 其他
    {IROp::NOP, "nop"},
    {IROp::LEA, "lea"},
    {IROp::SYSCALL, "syscall"},
};

IRNode::IRNode(IROp op, const std::vector<std::string>& operands, 
               const std::string& comment, int line)
    : op(op), operands(operands), comment(comment), line(line) {
}

IRNode::~IRNode() {
}
