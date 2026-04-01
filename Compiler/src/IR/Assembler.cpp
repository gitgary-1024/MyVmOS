// Assembler.cpp - 简易 x86-64 汇编器实现
// 直接将 IR 指令编码为 x86-64 机器码
#include "Assembler.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

// ===== 构造函数 =====
Assembler::Assembler(const std::vector<IRNode>& ir) : irNodes(ir), 
                                                       currentAddress(0), 
                                                       dataSegmentBase(0) {
}

// ===== 工具函数 =====
void Assembler::emitByte(uint8_t byte) {
    machineCode.push_back(byte);
    currentAddress++;
}

void Assembler::emitDWord(uint32_t dword) {
    // 小端序
    emitByte((dword >> 0) & 0xFF);
    emitByte((dword >> 8) & 0xFF);
    emitByte((dword >> 16) & 0xFF);
    emitByte((dword >> 24) & 0xFF);
}

void Assembler::emitQWord(uint64_t qword) {
    // 小端序
    for (int i = 0; i < 8; i++) {
        emitByte((qword >> (i * 8)) & 0xFF);
    }
}

void Assembler::emitBytes(const std::vector<uint8_t>& bytes) {
    for (auto byte : bytes) {
        emitByte(byte);
    }
}

bool Assembler::isRegister(const std::string& str) {
    static const std::vector<std::string> regs = {
        "al", "ah", "bl", "bh", "cl", "ch", "dl", "dh",
        "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    return std::find(regs.begin(), regs.end(), str) != regs.end();
}

bool Assembler::isNumber(const std::string& str) {
    if (str.empty()) return false;
    std::string s = str;
    if (s[0] == '-') s = s.substr(1);
    
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return std::all_of(s.begin() + 2, s.end(), [](char c) {
            return std::isxdigit(c);
        });
    }
    
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::isdigit(c);
    });
}

int64_t Assembler::parseNumber(const std::string& str) {
    if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return std::stoll(str, nullptr, 16);
    }
    return std::stoll(str);
}

int Assembler::getRegCode(const std::string& reg) {
    std::string r = reg;
    // 转换为小写
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    
    if (r == "rax" || r == "eax" || r == "ax" || r == "al") return RAX;
    if (r == "rcx" || r == "ecx" || r == "cx" || r == "cl") return RCX;
    if (r == "rdx" || r == "edx" || r == "dx" || r == "dl") return RDX;
    if (r == "rbx" || r == "ebx" || r == "bx" || r == "bl") return RBX;
    if (r == "rsp" || r == "esp" || r == "sp") return RSP;
    if (r == "rbp" || r == "ebp" || r == "bp") return RBP;
    if (r == "rsi" || r == "esi" || r == "si") return RSI;
    if (r == "rdi" || r == "edi" || r == "di") return RDI;
    
    return RAX; // 默认
}

// ===== 主编译函数 =====
std::vector<uint8_t> Assembler::compile() {
    machineCode.clear();
    labelToAddress.clear();
    variableOffsets.clear();
    currentAddress = 0;
    
    // ========== 第一遍：收集标签和变量信息 ==========
    uint64_t varOffset = 0;
    for (const auto& node : irNodes) {
        if (node.op == IROp::LABEL) {
            labelToAddress[node.operands[0]] = currentAddress;
        } else if (node.operands.size() > 0) {
            // 检查所有操作数，查找内存访问
            for (const auto& op : node.operands) {
                if (op.find('[') != std::string::npos && op.find(']') != std::string::npos) {
                    // 提取变量名 [var] -> var
                    std::string varName = op.substr(op.find('[') + 1, op.find(']') - op.find('[') - 1);
                    
                    // 如果是新变量，分配偏移
                    if (variableOffsets.find(varName) == variableOffsets.end()) {
                        variableOffsets[varName] = varOffset;
                        varOffset += 8; // 每个变量 8 字节
                    }
                }
            }
        }
        currentAddress += 15; // 估算每条指令的平均长度
    }
    
    // ========== 第二遍：编码所有指令 ==========
    machineCode.clear();
    currentAddress = 0;
    
    // 先估算代码段大小，确定数据段基址
    uint64_t estimatedCodeSize = 0;
    for (const auto& node : irNodes) {
        if (node.op == IROp::LABEL) {
            continue;  // LABEL 不生成代码
        }
        // 简单估算：每条指令平均 10 字节
        estimatedCodeSize += 10;
    }
    dataSegmentBase = estimatedCodeSize;  // 数据段放在代码段后面
    
    // 开始编码
    for (const auto& node : irNodes) {
        switch (node.op) {
            case IROp::MOV:     encodeMOV(node); break;
            case IROp::ADD:     encodeADD(node); break;
            case IROp::SUB:     encodeSUB(node); break;
            case IROp::IMUL:    encodeMUL(node); break;
            case IROp::IDIV:    encodeDIV(node); break;
            case IROp::AND:     encodeAND(node); break;
            case IROp::OR:      encodeOR(node); break;
            case IROp::XOR:     encodeXOR(node); break;
            case IROp::NOT:     encodeNOT(node); break;
            case IROp::NEG:     encodeNEG(node); break;
            case IROp::CMP:     encodeCMP(node); break;
            case IROp::JMP:     encodeJMP(node); break;
            case IROp::JE:      encodeJE(node); break;
            case IROp::JNE:     encodeJNE(node); break;
            case IROp::JL:      encodeJL(node); break;
            case IROp::JLE:     encodeJLE(node); break;
            case IROp::JG:      encodeJG(node); break;
            case IROp::JGE:     encodeJGE(node); break;
            case IROp::CALL:    encodeCALL(node); break;
            case IROp::RET:     encodeRET(node); break;
            case IROp::NOP:     encodeNOP(node); break;
            case IROp::PUSH:    encodePUSH(node); break;
            case IROp::POP:     encodePOP(node); break;
            case IROp::LABEL:   encodeLABEL(node); break;
            case IROp::LEA:     encodeLEA(node); break;
            case IROp::SYSCALL: encodeSYSCALL(node); break;
            default:
                std::cerr << "Warning: Unsupported IR operation: " 
                          << static_cast<int>(node.op) << std::endl;
        }
    }
    
    // ========== 第三遍：添加数据段 ==========
    // 在代码段末尾为每个变量预留空间
    for (const auto& [varName, offset] : variableOffsets) {
        emitQWord(0);  // 为每个变量预留 8 字节空间，初始化为 0
    }
    
    return machineCode;
}

const std::vector<uint8_t>& Assembler::getMachineCode() const {
    return machineCode;
}

bool Assembler::saveToFile(const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    
    out.write(reinterpret_cast<const char*>(machineCode.data()), machineCode.size());
    out.close();
    return true;
}

void Assembler::printMachineCode() const {
    std::cout << "Machine Code (" << machineCode.size() << " bytes):" << std::endl;
    
    for (size_t i = 0; i < machineCode.size(); i++) {
        if (i % 16 == 0) {
            std::cout << std::hex << std::setw(8) << std::setfill('0') << i << ": ";
        }
        
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)machineCode[i];
        
        if ((i + 1) % 16 == 0) {
            std::cout << std::endl;
        } else if ((i + 1) % 8 == 0) {
            std::cout << "  ";
        } else {
            std::cout << " ";
        }
    }
    
    if (machineCode.size() % 16 != 0) {
        std::cout << std::endl;
    }
    
    std::cout << std::dec;
}

// ===== x86-64 指令编码实现 =====

// MOV 指令编码
void Assembler::encodeMOV(const IRNode& node) {
    if (node.operands.size() < 2) return;
    
    std::string dest = node.operands[0];
    std::string src = node.operands[1];
    
    // 情况 1: MOV reg, imm (寄存器 = 立即数)
    if (isRegister(dest) && isNumber(src)) {
        int64_t imm = parseNumber(src);
        int reg = getRegCode(dest);
        
        // REX.W + B8+rd imm64
        emitByte(0x48);  // REX.W
        emitByte(0xB8 + reg);
        emitQWord(imm);
    }
    // 情况 2: MOV reg, reg (寄存器 = 寄存器)
    else if (isRegister(dest) && isRegister(src)) {
        int destReg = getRegCode(dest);
        int srcReg = getRegCode(src);
        
        // REX.W + 89 C0+src*8+dest
        emitByte(0x48);  // REX.W
        emitByte(0x89);
        emitByte(0xC0 + (srcReg << 3) + destReg);
    }
    // 情况 3: MOV [mem], reg (内存 = 寄存器)
    else if (dest.find('[') != std::string::npos && isRegister(src)) {
        int srcReg = getRegCode(src);
        
        // 提取变量名 [var] -> var
        std::string varName = dest.substr(dest.find('[') + 1, dest.find(']') - dest.find('[') - 1);
        
        // 查找变量偏移
        auto it = variableOffsets.find(varName);
        if (it == variableOffsets.end()) {
            std::cerr << "Warning: Undefined variable '" << varName << "'" << std::endl;
            return;  // 未定义的变量
        }
        
        uint64_t varOffset = it->second;
        
        // 计算 RIP 相对偏移
        // 当前指令地址 + 指令长度 (7 字节) = 下一条指令地址
        // 数据段基址 + 变量偏移 = 变量地址
        // offset = 变量地址 - 下一条指令地址
        int64_t currentPos = currentAddress + 7;  // MOV [rip+disp32], reg 的长度
        int64_t varAddr = dataSegmentBase + varOffset;
        int32_t ripOffset = varAddr - currentPos;
        
        // REX.W + 89 /r  (ModR/M: [rip+disp32], r64)
        emitByte(0x48);  // REX.W
        emitByte(0x89);  // MOV r/m64, r64
        emitByte(0x05);  // ModR/M: 00 000 101 (使用 RIP 相对寻址)
        emitDWord(ripOffset);  // 32 位 RIP 相对偏移
    }
    // 情况 4: MOV reg, [mem] (寄存器 = 内存)
    else if (isRegister(dest) && src.find('[') != std::string::npos) {
        int destReg = getRegCode(dest);
        
        // 提取变量名 [var] -> var
        std::string varName = src.substr(src.find('[') + 1, src.find(']') - src.find('[') - 1);
        
        // 查找变量偏移
        auto it = variableOffsets.find(varName);
        if (it == variableOffsets.end()) return;  // 未定义的变量
        
        uint64_t varOffset = it->second;
        
        // 计算 RIP 相对偏移
        int64_t currentPos = currentAddress + 7;  // MOV reg, [rip+disp32] 的长度
        int64_t varAddr = dataSegmentBase + varOffset;
        int32_t ripOffset = varAddr - currentPos;
        
        // REX.W + 8B /r  (ModR/M: r64, [rip+disp32])
        emitByte(0x48);  // REX.W
        emitByte(0x8B);  // MOV r64, r/m64
        emitByte(0x05);  // ModR/M: 00 000 101 (使用 RIP 相对寻址)
        emitDWord(ripOffset);  // 32 位 RIP 相对偏移
    }
}

// ADD 指令编码
void Assembler::encodeADD(const IRNode& node) {
    if (node.operands.size() < 2) return;
    
    std::string dest = node.operands[0];
    std::string src1 = node.operands.size() > 1 ? node.operands[1] : "";
    std::string src2 = node.operands.size() > 2 ? node.operands[2] : "";
    
    // ADD reg, reg
    if (isRegister(dest) && isRegister(src1)) {
        int destReg = getRegCode(dest);
        int srcReg = getRegCode(src1);
        
        emitByte(0x48);  // REX.W
        emitByte(0x01);  // ADD r/m64, r64
        emitByte(0xC0 + (srcReg << 3) + destReg);
    }
    // ADD reg, imm
    else if (isRegister(dest) && isNumber(src1)) {
        int64_t imm = parseNumber(src1);
        int reg = getRegCode(dest);
        
        if (imm >= -128 && imm <= 127) {
            // 短立即数
            emitByte(0x48);  // REX.W
            emitByte(0x83);  // ADD r/m64, imm8
            emitByte(0xC0 + reg);
            emitByte(imm & 0xFF);
        } else {
            // 长立即数
            emitByte(0x48);  // REX.W
            emitByte(0x81);  // ADD r/m64, imm32
            emitByte(0xC0 + reg);
            emitDWord(imm & 0xFFFFFFFF);
        }
    }
}

// SUB 指令编码
void Assembler::encodeSUB(const IRNode& node) {
    if (node.operands.size() < 2) return;
    
    std::string dest = node.operands[0];
    std::string src1 = node.operands.size() > 1 ? node.operands[1] : "";
    
    if (isRegister(dest) && isRegister(src1)) {
        int destReg = getRegCode(dest);
        int srcReg = getRegCode(src1);
        
        emitByte(0x48);  // REX.W
        emitByte(0x29);  // SUB r/m64, r64
        emitByte(0xC0 + (srcReg << 3) + destReg);
    }
    else if (isRegister(dest) && isNumber(src1)) {
        int64_t imm = parseNumber(src1);
        int reg = getRegCode(dest);
        
        if (imm >= -128 && imm <= 127) {
            emitByte(0x48);
            emitByte(0x83);
            emitByte(0xE8 + reg);
            emitByte(imm & 0xFF);
        } else {
            emitByte(0x48);
            emitByte(0x81);
            emitByte(0xE8 + reg);
            emitDWord(imm & 0xFFFFFFFF);
        }
    }
}

// CMP 指令编码
void Assembler::encodeCMP(const IRNode& node) {
    if (node.operands.size() < 2) return;
    
    std::string dest = node.operands[0];
    std::string src = node.operands[1];
    
    if (isRegister(dest) && isRegister(src)) {
        int destReg = getRegCode(dest);
        int srcReg = getRegCode(src);
        
        emitByte(0x48);
        emitByte(0x39);
        emitByte(0xC0 + (srcReg << 3) + destReg);
    }
    else if (isRegister(dest) && isNumber(src)) {
        int64_t imm = parseNumber(src);
        int reg = getRegCode(dest);
        
        if (imm >= -128 && imm <= 127) {
            emitByte(0x48);
            emitByte(0x83);
            emitByte(0xF8 + reg);
            emitByte(imm & 0xFF);
        } else {
            emitByte(0x48);
            emitByte(0x81);
            emitByte(0xF8 + reg);
            emitDWord(imm & 0xFFFFFFFF);
        }
    }
}

// JMP 指令编码
void Assembler::encodeJMP(const IRNode& node) {
    if (node.operands.empty()) return;
    
    std::string label = node.operands[0];
    auto it = labelToAddress.find(label);
    
    if (it != labelToAddress.end()) {
        // 相对跳转：E9 rel32
        int64_t targetAddr = it->second;
        int64_t currentPos = currentAddress + 5; // JMP 指令长度
        int32_t relOffset = targetAddr - currentPos;
        
        emitByte(0xE9);
        emitDWord(relOffset);
    }
}

// JE 指令编码
void Assembler::encodeJE(const IRNode& node) {
    if (node.operands.empty()) return;
    
    std::string label = node.operands[0];
    auto it = labelToAddress.find(label);
    
    if (it != labelToAddress.end()) {
        int64_t targetAddr = it->second;
        int64_t currentPos = currentAddress + 6; // 0F 84 + rel32
        int32_t relOffset = targetAddr - currentPos;
        
        emitByte(0x0F);
        emitByte(0x84);
        emitDWord(relOffset);
    }
}

// JNE 指令编码
void Assembler::encodeJNE(const IRNode& node) {
    if (node.operands.empty()) return;
    
    std::string label = node.operands[0];
    auto it = labelToAddress.find(label);
    
    if (it != labelToAddress.end()) {
        int64_t targetAddr = it->second;
        int64_t currentPos = currentAddress + 6;
        int32_t relOffset = targetAddr - currentPos;
        
        emitByte(0x0F);
        emitByte(0x85);
        emitDWord(relOffset);
    }
}

// PUSH 指令编码
void Assembler::encodePUSH(const IRNode& node) {
    if (node.operands.empty()) return;
    
    std::string reg = node.operands[0];
    int regCode = getRegCode(reg);
    
    emitByte(0x50 + regCode);
}

// POP 指令编码
void Assembler::encodePOP(const IRNode& node) {
    if (node.operands.empty()) return;
    
    std::string reg = node.operands[0];
    int regCode = getRegCode(reg);
    
    emitByte(0x58 + regCode);
}

// CALL 指令编码
void Assembler::encodeCALL(const IRNode& node) {
    if (node.operands.empty()) return;
    
    std::string func = node.operands[0];
    
    // E8 rel32
    emitByte(0xE8);
    emitDWord(0); // 占位符
}

// RET 指令编码
void Assembler::encodeRET(const IRNode& node) {
    emitByte(0xC3);
}

// NOP 指令编码
void Assembler::encodeNOP(const IRNode& node) {
    emitByte(0x90);
}

// LABEL 指令编码（实际上不生成代码，只是标记位置）
void Assembler::encodeLABEL(const IRNode& node) {
    // Label 已经在第一遍扫描时记录了地址
    // 这里不需要生成任何代码
}

// SYSCALL 指令编码
void Assembler::encodeSYSCALL(const IRNode& node) {
    // x86-64 syscall 指令编码：0F 05
    emitByte(0x0F);
    emitByte(0x05);
}

// IMUL 指令编码
void Assembler::encodeMUL(const IRNode& node) {
    if (node.operands.size() < 2) return;
    std::string dest = node.operands[0];
    std::string src = node.operands[1];
    
    if (isRegister(dest) && isRegister(src)) {
        int destReg = getRegCode(dest);
        int srcReg = getRegCode(src);
        
        emitByte(0x48);
        emitByte(0x0F);
        emitByte(0xAF);
        emitByte(0xC0 + (destReg << 3) + srcReg);
    }
}

// IDIV 指令编码
void Assembler::encodeDIV(const IRNode& node) {
    if (node.operands.size() < 2) return;
    std::string dest = node.operands[0];
    std::string src = node.operands[1];
    
    // 简化版本：假设使用 RAX 作为被除数
    if (isRegister(src)) {
        int srcReg = getRegCode(src);
        
        // DIV r64: F7 /6
        emitByte(0x48);
        emitByte(0xF7);
        emitByte(0xF0 + srcReg);  // ModR/M: 11 110 rrr
    }
}

// AND 指令编码
void Assembler::encodeAND(const IRNode& node) {
    if (node.operands.size() < 2) return;
    std::string dest = node.operands[0];
    std::string src = node.operands[1];
    
    if (isRegister(dest) && isRegister(src)) {
        int destReg = getRegCode(dest);
        int srcReg = getRegCode(src);
        
        emitByte(0x48);
        emitByte(0x21);  // AND r/m64, r64
        emitByte(0xC0 + (srcReg << 3) + destReg);
    } else if (isRegister(dest) && isNumber(src)) {
        int reg = getRegCode(dest);
        int64_t imm = parseNumber(src);
        
        if (imm >= -128 && imm <= 127) {
            // AND r64, imm8: 83 /4 ib
            emitByte(0x48);
            emitByte(0x83);
            emitByte(0xE0 + reg);  // ModR/M: 11 100 rrr
            emitByte(imm & 0xFF);
        } else {
            // AND r64, imm32: 81 /4 id
            emitByte(0x48);
            emitByte(0x81);
            emitByte(0xE0 + reg);
            emitDWord(imm & 0xFFFFFFFF);
        }
    }
}

// OR 指令编码
void Assembler::encodeOR(const IRNode& node) {
    if (node.operands.size() < 2) return;
    std::string dest = node.operands[0];
    std::string src = node.operands[1];
    
    if (isRegister(dest) && isRegister(src)) {
        int destReg = getRegCode(dest);
        int srcReg = getRegCode(src);
        
        emitByte(0x48);
        emitByte(0x09);  // OR r/m64, r64
        emitByte(0xC0 + (srcReg << 3) + destReg);
    } else if (isRegister(dest) && isNumber(src)) {
        int reg = getRegCode(dest);
        int64_t imm = parseNumber(src);
        
        if (imm >= -128 && imm <= 127) {
            emitByte(0x48);
            emitByte(0x83);
            emitByte(0xC8 + reg);
            emitByte(imm & 0xFF);
        } else {
            emitByte(0x48);
            emitByte(0x81);
            emitByte(0xC8 + reg);
            emitDWord(imm & 0xFFFFFFFF);
        }
    }
}

// XOR 指令编码
void Assembler::encodeXOR(const IRNode& node) {
    if (node.operands.size() < 2) return;
    std::string dest = node.operands[0];
    std::string src = node.operands[1];
    
    if (isRegister(dest) && isRegister(src)) {
        int destReg = getRegCode(dest);
        int srcReg = getRegCode(src);
        
        emitByte(0x48);
        emitByte(0x31);  // XOR r/m64, r64
        emitByte(0xC0 + (srcReg << 3) + destReg);
    } else if (isRegister(dest) && isNumber(src)) {
        int reg = getRegCode(dest);
        int64_t imm = parseNumber(src);
        
        if (imm >= -128 && imm <= 127) {
            emitByte(0x48);
            emitByte(0x83);
            emitByte(0xF0 + reg);
            emitByte(imm & 0xFF);
        } else {
            emitByte(0x48);
            emitByte(0x81);
            emitByte(0xF0 + reg);
            emitDWord(imm & 0xFFFFFFFF);
        }
    }
}

// NOT 指令编码
void Assembler::encodeNOT(const IRNode& node) {
    if (node.operands.empty()) return;
    std::string reg = node.operands[0];
    int regCode = getRegCode(reg);
    
    // NOT r64: F7 /2
    emitByte(0x48);
    emitByte(0xF7);
    emitByte(0xD0 + regCode);  // ModR/M: 11 010 rrr
}

// NEG 指令编码
void Assembler::encodeNEG(const IRNode& node) {
    if (node.operands.empty()) return;
    std::string dest = node.operands[0];
    
    if (isRegister(dest)) {
        int reg = getRegCode(dest);
        
        // NEG r64: F7 /3
        emitByte(0x48);
        emitByte(0xF7);
        emitByte(0xD8 + reg);  // ModR/M: 11 011 rrr
    }
}

// JL 指令编码
void Assembler::encodeJL(const IRNode& node) {
    if (node.operands.empty()) return;
    std::string label = node.operands[0];
    auto it = labelToAddress.find(label);
    
    if (it != labelToAddress.end()) {
        int64_t targetAddr = it->second;
        int64_t currentPos = currentAddress + 6;
        int32_t relOffset = targetAddr - currentPos;
        
        // JL rel32: 0F 7C cd
        emitByte(0x0F);
        emitByte(0x7C);
        emitDWord(relOffset);
    }
}

// JLE 指令编码
void Assembler::encodeJLE(const IRNode& node) {
    if (node.operands.empty()) return;
    std::string label = node.operands[0];
    auto it = labelToAddress.find(label);
    
    if (it != labelToAddress.end()) {
        int64_t targetAddr = it->second;
        int64_t currentPos = currentAddress + 6;
        int32_t relOffset = targetAddr - currentPos;
        
        // JLE rel32: 0F 7E cd
        emitByte(0x0F);
        emitByte(0x7E);
        emitDWord(relOffset);
    }
}

// JG 指令编码
void Assembler::encodeJG(const IRNode& node) {
    if (node.operands.empty()) return;
    std::string label = node.operands[0];
    auto it = labelToAddress.find(label);
    
    if (it != labelToAddress.end()) {
        int64_t targetAddr = it->second;
        int64_t currentPos = currentAddress + 6;
        int32_t relOffset = targetAddr - currentPos;
        
        // JG rel32: 0F 7F cd
        emitByte(0x0F);
        emitByte(0x7F);
        emitDWord(relOffset);
    }
}

// JGE 指令编码
void Assembler::encodeJGE(const IRNode& node) {
    if (node.operands.empty()) return;
    std::string label = node.operands[0];
    auto it = labelToAddress.find(label);
    
    if (it != labelToAddress.end()) {
        int64_t targetAddr = it->second;
        int64_t currentPos = currentAddress + 6;
        int32_t relOffset = targetAddr - currentPos;
        
        // JGE rel32: 0F 7D cd
        emitByte(0x0F);
        emitByte(0x7D);
        emitDWord(relOffset);
    }
}

// LEA 指令编码（Load Effective Address）
void Assembler::encodeLEA(const IRNode& node) {
    if (node.operands.size() < 2) return;
    std::string dest = node.operands[0];
    std::string src = node.operands[1];
    
    if (isRegister(dest)) {
        int destReg = getRegCode(dest);
        
        // LEA r64, [rip+disp32]: 48 8D 05 disp32
        emitByte(0x48);
        emitByte(0x8D);
        emitByte(0x05);  // ModR/M: 00 000 101 (RIP 相对寻址)
        emitDWord(0);    // 占位符，需要后续填充
    }
}



