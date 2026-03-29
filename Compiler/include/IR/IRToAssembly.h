// IRToAssembly.h - IR 到 x86 汇编代码生成器
#ifndef IRTOASSEMBLY_H
#define IRTOASSEMBLY_H

#include "IRbase.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <iomanip>

/**
 * IR 到 x86 汇编代码生成器
 * 
 * 功能：
 * 1. 将 IR 指令转换为 x86-64 汇编代码
 * 2. 支持 AT&T 语法和 Intel 语法
 * 3. 处理寄存器分配结果（物理寄存器或内存访问）
 * 4. 生成可被汇编器接受的汇编代码
 */
class IRToAssembly {
private:
    std::vector<IRNode> irNodes;           // IR 指令列表
    std::vector<std::string> assemblyCode; // 生成的汇编代码
    std::unordered_map<std::string, int> labelToIndex; // 标签 -> 指令索引映射
    
    // 配置选项
    bool useIntelSyntax;  // true = Intel 语法，false = AT&T 语法
    bool generateComments; // 是否生成注释
    
    // 寄存器映射（虚拟寄存器 -> 物理寄存器或内存位置）
    std::unordered_map<std::string, std::string> regMap;
    
    // 工具函数
    std::string trim(const std::string& str);
    bool isNumber(const std::string& str);
    bool isRegister(const std::string& str);
    bool isMemoryOperand(const std::string& str);
    
    // 操作数转换
    std::string convertOperand(const std::string& operand);
    std::string getPhysicalReg(const std::string& vreg);
    
    // IR 指令翻译
    void translateMOV(const IRNode& node);
    void translatePUSH(const IRNode& node);
    void translatePOP(const IRNode& node);
    void translateADD(const IRNode& node);
    void translateSUB(const IRNode& node);
    void translateIMUL(const IRNode& node);
    void translateIDIV(const IRNode& node);
    void translateINC(const IRNode& node);
    void translateDEC(const IRNode& node);
    void translateNEG(const IRNode& node);
    void translateAND(const IRNode& node);
    void translateOR(const IRNode& node);
    void translateNOT(const IRNode& node);
    void translateXOR(const IRNode& node);
    void translateCMP(const IRNode& node);
    void translateTEST(const IRNode& node);
    void translateJMP(const IRNode& node);
    void translateJE(const IRNode& node);
    void translateJNE(const IRNode& node);
    void translateJL(const IRNode& node);
    void translateJLE(const IRNode& node);
    void translateJG(const IRNode& node);
    void translateJGE(const IRNode& node);
    void translateLABEL(const IRNode& node);
    void translateCALL(const IRNode& node);
    void translateRET(const IRNode& node);
    void translateNOP(const IRNode& node);
    void translateLEA(const IRNode& node);
    
    // 生成汇编指令
    void emitInstruction(const std::string& mnemonic, 
                        const std::string& dest = "", 
                        const std::string& src = "");
    void emitLabel(const std::string& label);
    void emitComment(const std::string& comment);
    
public:
    /**
     * 构造函数
     * @param ir IR 指令列表
     * @param regMapping 寄存器映射（虚拟寄存器 -> 物理寄存器）
     * @param intelSyntax 是否使用 Intel 语法（默认 true）
     * @param withComments 是否生成注释（默认 true）
     */
    IRToAssembly(const std::vector<IRNode>& ir, 
                 const std::unordered_map<std::string, std::string>& regMapping = {},
                 bool intelSyntax = true,
                 bool withComments = true);
    
    /**
     * 编译 IR 到汇编代码
     * @return 汇编代码字符串
     */
    std::string compile();
    
    /**
     * 获取汇编代码行
     * @return 汇编代码行向量
     */
    std::vector<std::string> getAssemblyLines() const;
    
    /**
     * 设置寄存器映射
     */
    void setRegisterMap(const std::unordered_map<std::string, std::string>& mapping);
};

#endif // IRTOASSEMBLY_H
