// Assembler.h - 简易 x86-64 汇编器声明
// 直接将 IR 指令编码为 x86-64 机器码
#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include "./IR/IR.h"

class Assembler {
private:
    std::vector<IRNode> irNodes;
    std::vector<uint8_t> machineCode;  // 生成的机器码
    std::unordered_map<std::string, uint64_t> labelToAddress;  // 标签地址映射
    std::unordered_map<std::string, uint64_t> variableOffsets; // 变量偏移量（相对于数据段起点）
    uint64_t currentAddress;  // 当前代码地址
    uint64_t dataSegmentBase;  // 数据段在代码中的起始地址
    
    // 寄存器编码
    enum Reg64 {
        RAX = 0, RCX = 1, RDX = 2, RBX = 3,
        RSP = 4, RBP = 5, RSI = 6, RDI = 7
    };
    
    // 工具函数
    void emitByte(uint8_t byte);
    void emitDWord(uint32_t dword);
    void emitQWord(uint64_t qword);
    void emitBytes(const std::vector<uint8_t>& bytes);
    
    // IR 指令编码（核心编码函数）
    void encodeMOV(const IRNode& node);
    void encodeADD(const IRNode& node);
    void encodeSUB(const IRNode& node);
    void encodeMUL(const IRNode& node);
    void encodeDIV(const IRNode& node);
    void encodeAND(const IRNode& node);
    void encodeOR(const IRNode& node);
    void encodeXOR(const IRNode& node);
    void encodeNOT(const IRNode& node);
    void encodeNEG(const IRNode& node);
    void encodeCMP(const IRNode& node);
    void encodeJMP(const IRNode& node);
    void encodeJE(const IRNode& node);
    void encodeJNE(const IRNode& node);
    void encodeJL(const IRNode& node);
    void encodeJLE(const IRNode& node);
    void encodeJG(const IRNode& node);
    void encodeJGE(const IRNode& node);
    void encodeCALL(const IRNode& node);
    void encodeRET(const IRNode& node);
    void encodeNOP(const IRNode& node);
    void encodePUSH(const IRNode& node);
    void encodePOP(const IRNode& node);
    void encodeLABEL(const IRNode& node);
    void encodeLEA(const IRNode& node);
    void encodeSYSCALL(const IRNode& node);  // 新增：系统调用
    void encodeSAL(const IRNode& node);
    void encodeSAR(const IRNode& node);
    void encodeSHL(const IRNode& node);
    void encodeSHR(const IRNode& node);
    void encodeSETcc(const IRNode& node);
    void encodeMOVSX(const IRNode& node);
    void encodeMOVZX(const IRNode& node);
    
    // 操作数编码辅助函数
    int getRegCode(const std::string& reg);
    bool isRegister(const std::string& str);
    bool isNumber(const std::string& str);
    int64_t parseNumber(const std::string& str);
    
public:
    Assembler(const std::vector<IRNode>& ir);
    
    // 编译为机器码
    std::vector<uint8_t> compile();
    
    // 获取机器码
    const std::vector<uint8_t>& getMachineCode() const;
    
    // 保存为二进制文件
    bool saveToFile(const std::string& filename);
    
    // 打印机器码（用于调试）
    void printMachineCode() const;
};

#endif // ASSEMBLER_H
