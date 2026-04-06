#pragma once

#include "BasicBlock.h"
#include <capstone/capstone.h>
#include <memory>
#include <string>
#include <iostream>

namespace disassembly {

/**
 * @brief 扩展的 SimpleInstruction（包含 Capstone 详细数据）
 * 
 * 在原有 SimpleInstruction 基础上，添加 Capstone 原始指令指针，
 * 以便直接访问操作数结构体，避免字符串解析。
 */
struct EnhancedSimpleInstruction : public cfg::SimpleInstruction {
    cs_insn* capstone_insn;  // Capstone 原始指令指针（可选）
    bool has_capstone_data;  // 标记是否有 Capstone 详细数据
    
    EnhancedSimpleInstruction() : capstone_insn(nullptr), has_capstone_data(false) {}
    
    ~EnhancedSimpleInstruction() {
        // 注意：不要在这里释放 capstone_insn
        // 它由 CapstoneDisassembler 统一管理
    }
};

/**
 * @brief 增强的跳转目标提取器
 * 
 * 使用 Capstone 详细模式直接访问操作数，避免字符串解析的不准确性。
 * 
 * 支持的跳转类型：
 * 1. 直接跳转：JMP rel32, JMP rel8 → 立即数操作数
 * 2. 条件跳转：JE, JNE, JG 等 → 立即数操作数
 * 3. CALL 指令：CALL rel32 → 立即数操作数
 * 4. 间接跳转：JMP RAX, CALL [RBP-8] → 寄存器/内存操作数（无法静态确定）
 * 5. RIP-relative：JMP [RIP+0x100] → 内存操作数（可计算）
 */
class EnhancedJumpTargetExtractor {
public:
    /**
     * @brief 提取跳转目标地址（增强版）
     * 
     * @param insn 指令指针
     * @param current_addr 当前指令地址（用于 RIP-relative 计算）
     * @return 跳转目标地址，如果无法确定返回 0
     */
    static uint64_t extract_jump_target(
        const cfg::SimpleInstruction* insn,
        uint64_t current_addr
    ) {
        if (!insn) return 0;
        
        // ✅ 检查是否有 Capstone 详细数据
        if (insn->has_capstone_data && insn->capstone_insn) {
            // 使用 Capstone 详细数据
            return extract_from_capstone_detail(static_cast<cs_insn*>(insn->capstone_insn), current_addr);
        } else {
            // ⚠️ 回退到字符串解析（兼容性）
            std::cout << "[WARNING] Falling back to string parsing for jump target extraction" << std::endl;
            return extract_from_string(insn);
        }
    }
    
private:
    /**
     * @brief 从 Capstone 详细数据中提取跳转目标
     */
    static uint64_t extract_from_capstone_detail(
        cs_insn* cs_insn,
        uint64_t current_addr
    ) {
        if (!cs_insn) {
            std::cout << "  [ERROR] cs_insn is null" << std::endl;
            return 0;
        }
        
        if (!cs_insn->detail) {
            std::cout << "  [ERROR] cs_insn->detail is null (detail mode not enabled?)" << std::endl;
            return 0;
        }
        
        cs_detail* detail = cs_insn->detail;
        const cs_x86* x86 = &detail->x86;
        
        if (x86->op_count == 0) {
            std::cout << "  [WARNING] No operands found" << std::endl;
            return 0;
        }
        
        const cs_x86_op& op = x86->operands[0];
        
        switch (op.type) {
            case CS_OP_IMM: {
                // ✅ 立即数：直接跳转目标
                uint64_t target = static_cast<uint64_t>(op.imm);
                
                // 调试输出
                std::cout << "  [EXTRACT] Immediate operand: 0x" 
                          << std::hex << target << std::dec << std::endl;
                
                return target;
            }
            
            case CS_OP_MEM: {
                // ⚠️ 内存操作数：可能是 RIP-relative 或间接跳转
                
                // 检查是否是 RIP-relative
                if (op.mem.base == X86_REG_RIP) {
                    // RIP-relative 寻址：target = RIP + disp
                    uint64_t next_rip = current_addr + cs_insn->size;
                    uint64_t target = next_rip + op.mem.disp;
                    
                    std::cout << "  [EXTRACT] RIP-relative: RIP(0x" 
                              << std::hex << next_rip 
                              << ") + disp(0x" << op.mem.disp 
                              << ") = 0x" << target << std::dec << std::endl;
                    
                    return target;
                } else {
                    // 其他内存操作数：间接跳转
                    std::cout << "  [WARNING] Indirect jump (memory): [" 
                              << get_reg_name(op.mem.base) << "+" << op.mem.disp << "]" 
                              << std::endl;
                    return 0;  // 无法静态确定
                }
            }
            
            case CS_OP_REG: {
                // ⚠️ 寄存器操作数：间接跳转
                std::cout << "  [WARNING] Indirect jump (register): " 
                          << get_reg_name(op.reg) << std::endl;
                return 0;  // 无法静态确定
            }
            
            default:
                std::cout << "  [WARNING] Unknown operand type: " << op.type << std::endl;
                return 0;
        }
    }
    
    /**
     * @brief 从字符串解析跳转目标（回退方案）
     */
    static uint64_t extract_from_string(const cfg::SimpleInstruction* insn) {
        if (!insn) return 0;
        
        const std::string& op_str = insn->operands;
        
        if (op_str.empty()) return 0;
        
        // 查找 "0x" 前缀
        size_t hex_pos = op_str.find("0x");
        if (hex_pos != std::string::npos) {
            try {
                uint64_t target = std::stoull(op_str.substr(hex_pos), nullptr, 16);
                return target;
            } catch (...) {
                // 解析失败
            }
        }
        
        return 0;
    }
    
    /**
     * @brief 获取寄存器名称
     */
    static std::string get_reg_name(x86_reg reg) {
        switch (reg) {
            case X86_REG_RAX: return "RAX";
            case X86_REG_RBX: return "RBX";
            case X86_REG_RCX: return "RCX";
            case X86_REG_RDX: return "RDX";
            case X86_REG_RSI: return "RSI";
            case X86_REG_RDI: return "RDI";
            case X86_REG_RBP: return "RBP";
            case X86_REG_RSP: return "RSP";
            case X86_REG_R8:  return "R8";
            case X86_REG_R9:  return "R9";
            case X86_REG_R10: return "R10";
            case X86_REG_R11: return "R11";
            case X86_REG_R12: return "R12";
            case X86_REG_R13: return "R13";
            case X86_REG_R14: return "R14";
            case X86_REG_R15: return "R15";
            case X86_REG_RIP: return "RIP";
            default: return "REG_" + std::to_string(reg);
        }
    }
};

} // namespace disassembly
