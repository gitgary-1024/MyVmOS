// IR.h - 中间表示生成器声明
#ifndef IR_H
#define IR_H

#include "../AST/ASTnode.h"
#include "IRbase.h"
#include <vector>
#include <string>

// 包含 X86Reg 枚举定义（用于 syscall 参数传递）
#include "../../include/vm/X86CPU.h"

// X86Reg 到字符串的转换函数声明
std::string getRegisterName(X86Reg reg);

class IR {
private:
    ASTBaseNode* ASTroot;
    std::vector<IRNode> irNodes; // 存储生成的 IR 指令
    int vregCounter; // 虚拟寄存器计数器
    int labCounter; // 标签计数器
    
    // 物理寄存器
    const std::string EAX = "eax";
    const std::string EBX = "ebx";
    const std::string ECX = "ecx";
    const std::string EDX = "edx";
    const std::string EBP = "ebp";
    const std::string ESP = "esp";
    std::vector<std::string> physicalRegs; // 可用物理寄存器列表
    
    // 活跃性分析相关
    struct LiveInterval {
        std::string vreg;
        int start; // 活跃起点（指令索引）
        int end;   // 活跃终点
        bool operator<(const LiveInterval& other) const {
            return start < other.start;
        }
    };
    std::vector<LiveInterval> intervals; // 所有虚拟寄存器的活跃区间
    std::unordered_map<std::string, std::string> vregToPreg; // 虚拟寄存器 -> 物理寄存器映射
    std::unordered_map<std::string, int> spillMap; // 溢出的虚拟寄存器 -> 栈偏移
    int stackOffset; // 当前栈偏移
    
    // 生成新虚拟寄存器名（如 vreg0, vreg1, ...）
    std::string newVReg();
    
    // 生成新标签名（如 .L0, .L1, ...）
    std::string newLab();
    
    // 活跃性分析：收集所有虚拟寄存器的生命周期
    void analyzeLiveness();
    
    // 线性扫描寄存器分配
    void linearScanAllocate();
    
    // 重写 IR 指令中的虚拟寄存器为物理寄存器
    void rewriteIR();
    
    // 溢出处理：将虚拟寄存器溢出到栈
    void spillAtInterval(const LiveInterval& interval);
    
    // 映射运算符到 IROp
    IROp opToIROp(const std::string& op);
    
    // 处理表达式节点，返回结果虚拟寄存器
    std::string generateExpression(ASTBaseNode* node);
    
    // 处理变量声明（包括初始化表达式）
    void generateVarDecl(VariableDeclaration* varDecl);
    
    // 处理 return 语句
    void generateReturn(Statement* stmt);
    
    // 递归处理所有 AST 节点
    void generate(ASTBaseNode* node);
    
public:
    IR(ASTBaseNode* ASTroot);
    
    // 生成所有的 IR 指令
    std::vector<IRNode> compileToIR();
    
    ~IR();
};

#endif // IR_H
