// IRToAssembly.cpp - IR 到 x86 汇编代码生成器实现
#include "../include/IR/IRToAssembly.h"
#include <iostream>
#include <algorithm>
#include <cctype>

// ===== 构造函数 =====
IRToAssembly::IRToAssembly(const std::vector<IRNode>& ir, 
                           const std::unordered_map<std::string, std::string>& regMapping,
                           bool intelSyntax,
                           bool withComments)
    : irNodes(ir)
    , useIntelSyntax(intelSyntax)
    , generateComments(withComments)
    , regMap(regMapping)
{
}

// ===== 工具函数 =====
std::string IRToAssembly::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

bool IRToAssembly::isNumber(const std::string& str) {
    if (str.empty()) return false;
    
    std::string s = str;
    // 处理负数
    if (s[0] == '-') s = s.substr(1);
    
    // 检查十六进制
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return std::all_of(s.begin() + 2, s.end(), [](char c) {
            return std::isxdigit(c);
        });
    }
    
    // 检查十进制
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::isdigit(c);
    });
}

bool IRToAssembly::isRegister(const std::string& str) {
    if (str.empty()) return false;
    
    // 检查是否为物理寄存器（如 eax, ebx, rax, rbx 等）
    static const std::vector<std::string> physicalRegs = {
        "al", "ah", "bl", "bh", "cl", "ch", "dl", "dh",
        "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    return std::find(physicalRegs.begin(), physicalRegs.end(), lower) != physicalRegs.end();
}

bool IRToAssembly::isMemoryOperand(const std::string& str) {
    // 检查是否为内存操作数（如 [ebp-8], [rax], 等）
    return str.find('[') != std::string::npos;
}

std::string IRToAssembly::getPhysicalReg(const std::string& vreg) {
    // 如果已经是物理寄存器，直接返回
    if (isRegister(vreg)) {
        return vreg;
    }
    
    // 查找映射
    auto it = regMap.find(vreg);
    if (it != regMap.end()) {
        return it->second;
    }
    
    // 如果没有映射，假设是变量名，返回内存地址
    return "[" + vreg + "]";
}

std::string IRToAssembly::convertOperand(const std::string& operand) {
    std::string op = trim(operand);
    
    if (op.empty()) return "";
    
    // 如果是立即数
    if (isNumber(op)) {
        if (useIntelSyntax) {
            return op;
        } else {
            // AT&T 语法需要 $ 前缀
            return "$" + op;
        }
    }
    
    // 如果是寄存器
    if (isRegister(op)) {
        if (useIntelSyntax) {
            return op;
        } else {
            // AT&T 语法需要 % 前缀
            return "%" + op;
        }
    }
    
    // 如果是内存操作数
    if (isMemoryOperand(op)) {
        if (useIntelSyntax) {
            return op;
        } else {
            // AT&T 语法格式不同，这里简化处理
            return op;
        }
    }
    
    // 否则假设是变量或标签
    if (useIntelSyntax) {
        // Intel 语法：直接引用或使用 QWORD PTR
        if (op.find("QWORD") == std::string::npos && 
            op.find("DWORD") == std::string::npos) {
            return "QWORD [" + op + "]";
        }
        return op;
    } else {
        // AT&T 语法
        return op;
    }
}

// ===== 汇编指令生成 =====
void IRToAssembly::emitInstruction(const std::string& mnemonic, 
                                   const std::string& dest, 
                                   const std::string& src) {
    std::stringstream ss;
    
    if (useIntelSyntax) {
        // Intel 语法：INSTRUCTION DEST, SRC
        ss << "    " << mnemonic;
        if (!dest.empty()) {
            ss << " " << dest;
            if (!src.empty()) {
                ss << ", " << src;
            }
        }
    } else {
        // AT&T 语法：INSTRUCTION SRC, DEST
        ss << "    " << mnemonic;
        if (!src.empty() || !dest.empty()) {
            ss << " ";
            std::vector<std::string> ops;
            if (!src.empty()) ops.push_back(src);
            if (!dest.empty()) ops.push_back(dest);
            
            for (size_t i = 0; i < ops.size(); i++) {
                if (i > 0) ss << ", ";
                ss << ops[i];
            }
        }
    }
    
    assemblyCode.push_back(ss.str());
}

void IRToAssembly::emitLabel(const std::string& label) {
    if (useIntelSyntax) {
        assemblyCode.push_back(label + ":");
    } else {
        assemblyCode.push_back("." + label + ":");
    }
}

void IRToAssembly::emitComment(const std::string& comment) {
    if (generateComments) {
        if (useIntelSyntax) {
            assemblyCode.push_back("    ; " + comment);
        } else {
            assemblyCode.push_back("    # " + comment);
        }
    }
}

// ===== IR 指令翻译 =====
void IRToAssembly::translateMOV(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: MOV needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("mov", dest, src);
}

void IRToAssembly::translatePUSH(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: PUSH needs operand");
        return;
    }
    
    std::string src = convertOperand(node.operands[0]);
    emitInstruction("push", "", src);
}

void IRToAssembly::translatePOP(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: POP needs operand");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    emitInstruction("pop", dest, "");
}

void IRToAssembly::translateADD(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: ADD needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("add", dest, src);
}

void IRToAssembly::translateSUB(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: SUB needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("sub", dest, src);
}

void IRToAssembly::translateIMUL(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: IMUL needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("imul", dest, src);
}

void IRToAssembly::translateIDIV(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: IDIV needs operand");
        return;
    }
    
    std::string src = convertOperand(node.operands[0]);
    emitInstruction("idiv", "", src);
}

void IRToAssembly::translateINC(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: INC needs operand");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    emitInstruction("inc", dest, "");
}

void IRToAssembly::translateDEC(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: DEC needs operand");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    emitInstruction("dec", dest, "");
}

void IRToAssembly::translateNEG(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: NEG needs operand");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    emitInstruction("neg", dest, "");
}

void IRToAssembly::translateAND(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: AND needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("and", dest, src);
}

void IRToAssembly::translateOR(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: OR needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("or", dest, src);
}

void IRToAssembly::translateNOT(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: NOT needs operand");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    emitInstruction("not", dest, "");
}

void IRToAssembly::translateXOR(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: XOR needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("xor", dest, src);
}

void IRToAssembly::translateCMP(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: CMP needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("cmp", dest, src);
}

void IRToAssembly::translateTEST(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: TEST needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("test", dest, src);
}

void IRToAssembly::translateJMP(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: JMP needs label");
        return;
    }
    
    std::string label = node.operands[0];
    if (!useIntelSyntax) {
        label = "." + label;
    }
    emitInstruction("jmp", label, "");
}

void IRToAssembly::translateJE(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: JE needs label");
        return;
    }
    
    std::string label = node.operands[0];
    if (!useIntelSyntax) {
        label = "." + label;
    }
    emitInstruction("je", label, "");
}

void IRToAssembly::translateJNE(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: JNE needs label");
        return;
    }
    
    std::string label = node.operands[0];
    if (!useIntelSyntax) {
        label = "." + label;
    }
    emitInstruction("jne", label, "");
}

void IRToAssembly::translateJL(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: JL needs label");
        return;
    }
    
    std::string label = node.operands[0];
    if (!useIntelSyntax) {
        label = "." + label;
    }
    emitInstruction("jl", label, "");
}

void IRToAssembly::translateJLE(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: JLE needs label");
        return;
    }
    
    std::string label = node.operands[0];
    if (!useIntelSyntax) {
        label = "." + label;
    }
    emitInstruction("jle", label, "");
}

void IRToAssembly::translateJG(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: JG needs label");
        return;
    }
    
    std::string label = node.operands[0];
    if (!useIntelSyntax) {
        label = "." + label;
    }
    emitInstruction("jg", label, "");
}

void IRToAssembly::translateJGE(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: JGE needs label");
        return;
    }
    
    std::string label = node.operands[0];
    if (!useIntelSyntax) {
        label = "." + label;
    }
    emitInstruction("jge", label, "");
}

void IRToAssembly::translateLABEL(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: LABEL needs name");
        return;
    }
    
    std::string label = node.operands[0];
    emitLabel(label);
}

void IRToAssembly::translateCALL(const IRNode& node) {
    if (node.operands.empty()) {
        emitComment("ERROR: CALL needs function name");
        return;
    }
    
    std::string func = node.operands[0];
    emitInstruction("call", func, "");
}

void IRToAssembly::translateRET(const IRNode& node) {
    if (node.operands.empty()) {
        emitInstruction("ret", "", "");
    } else {
        // ret with value in register
        emitInstruction("ret", "", "");
    }
}

void IRToAssembly::translateNOP(const IRNode& node) {
    emitInstruction("nop", "", "");
}

void IRToAssembly::translateLEA(const IRNode& node) {
    if (node.operands.size() < 2) {
        emitComment("ERROR: LEA needs 2 operands");
        return;
    }
    
    std::string dest = convertOperand(node.operands[0]);
    std::string src = convertOperand(node.operands[1]);
    emitInstruction("lea", dest, src);
}

// ===== 主编译函数 =====
std::string IRToAssembly::compile() {
    assemblyCode.clear();
    
    // 添加文件头注释
    if (generateComments) {
        assemblyCode.push_back("; Generated by IRToAssembly Compiler");
        assemblyCode.push_back("; x86-64 Assembly Code");
        if (useIntelSyntax) {
            assemblyCode.push_back("; Syntax: Intel");
        } else {
            assemblyCode.push_back("; Syntax: AT&T");
        }
        assemblyCode.push_back("");
    }
    
    // 添加段声明（Intel 语法）
    if (useIntelSyntax) {
        assemblyCode.push_back("section .text");
        assemblyCode.push_back("global main");
        assemblyCode.push_back("");
    }
    
    // 遍历所有 IR 指令并翻译
    for (const auto& node : irNodes) {
        // 添加行号注释
        if (generateComments && node.line > 0) {
            emitComment("Line " + std::to_string(node.line));
        }
        
        // 根据操作类型调用对应的翻译函数
        switch (node.op) {
            case IROp::MOV:     translateMOV(node); break;
            case IROp::PUSH:    translatePUSH(node); break;
            case IROp::POP:     translatePOP(node); break;
            case IROp::ADD:     translateADD(node); break;
            case IROp::SUB:     translateSUB(node); break;
            case IROp::IMUL:    translateIMUL(node); break;
            case IROp::IDIV:    translateIDIV(node); break;
            case IROp::INC:     translateINC(node); break;
            case IROp::DEC:     translateDEC(node); break;
            case IROp::NEG:     translateNEG(node); break;
            case IROp::AND:     translateAND(node); break;
            case IROp::OR:      translateOR(node); break;
            case IROp::NOT:     translateNOT(node); break;
            case IROp::XOR:     translateXOR(node); break;
            case IROp::CMP:     translateCMP(node); break;
            case IROp::TEST:    translateTEST(node); break;
            case IROp::JMP:     translateJMP(node); break;
            case IROp::JE:      translateJE(node); break;
            case IROp::JNE:     translateJNE(node); break;
            case IROp::JL:      translateJL(node); break;
            case IROp::JLE:     translateJLE(node); break;
            case IROp::JG:      translateJG(node); break;
            case IROp::JGE:     translateJGE(node); break;
            case IROp::LABEL:   translateLABEL(node); break;
            case IROp::CALL:    translateCALL(node); break;
            case IROp::RET:     translateRET(node); break;
            case IROp::NOP:     translateNOP(node); break;
            case IROp::LEA:     translateLEA(node); break;
            default:
                emitComment("UNKNOWN IR OPERATION: " + IRopTOStr[node.op]);
        }
        
        // 添加原始 IR 作为注释
        if (generateComments) {
            std::string ir_str = IRopTOStr[node.op] + " ";
            for (size_t i = 0; i < node.operands.size(); i++) {
                if (i > 0) ir_str += ", ";
                ir_str += node.operands[i];
            }
            emitComment("IR: " + ir_str);
        }
    }
    
    // 拼接所有汇编代码行
    std::stringstream result;
    for (const auto& line : assemblyCode) {
        result << line << "\n";
    }
    
    return result.str();
}

// ===== 其他公共方法 =====
std::vector<std::string> IRToAssembly::getAssemblyLines() const {
    return assemblyCode;
}

void IRToAssembly::setRegisterMap(const std::unordered_map<std::string, std::string>& mapping) {
    regMap = mapping;
}
