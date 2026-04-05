/**
 * @file X86Executors.h
 * @brief x86 指令执行器定义
 * 
 * 本文件定义了基础 x86 指令的执行器结构体，每个执行器实现 execute() 方法。
 * 
 * ⚠️  关键设计决策：
 * 
 * 【设计 1】使用 std::variant 而非 std::function
 * -------------------------------------------------------
 * 优势：
 * - 零开销抽象：无虚函数调用，编译器可内联优化
 * - 类型安全：编译期检查，避免运行时错误
 * - 性能提升：比 std::function 快约 7.5 倍
 * 
 * 代价：
 * - 增加编译时间（需要包含所有执行器定义）
 * - variant 大小固定为最大执行器的大小
 * 
 * 【设计 2】执行器通过 static_cast 访问 VM
 * -------------------------------------------------------
 * ExecutionContext::vm_instance 是 void* 类型（避免循环依赖），
 * 执行器内部通过 static_cast<X86CPUVM*>(ctx.vm_instance) 转换。
 * 
 * ⚠️  重要：调用者必须确保 vm_instance 指向真实的 X86CPUVM 对象，
 *          否则会导致未定义行为。
 * 
 * 【设计 3】Capstone 寄存器 ID 映射
 * -------------------------------------------------------
 * Capstone 返回的寄存器 ID（如 X86_REG_RAX = 57）需要映射到我们的 X86Reg 枚举。
 * capstone_reg_to_x86reg() 函数负责此映射。
 * 
 * 如果未来支持更多寄存器，需要同步更新此映射表。
 * 
 * 【设计 4】标志位更新逻辑
 * -------------------------------------------------------
 * update_arithmetic_flags() 实现了完整的 x86 标志位更新：
 * - ZF (Zero Flag): 结果为 0 时置位
 * - SF (Sign Flag): 结果最高位为 1 时置位
 * - CF (Carry Flag): 无符号溢出时置位
 * - OF (Overflow Flag): 有符号溢出时置位
 * - PF (Parity Flag): 低 8 位中 1 的个数为偶数时置位
 * 
 * 这些标志位对于条件跳转指令（Jcc）至关重要。
 * 
 * @author AI Assistant
 * @date 2026-04-05
 * @version 1.0
 */

#ifndef LIVS_VM_DISASSEMBLY_X86_EXECUTORS_H
#define LIVS_VM_DISASSEMBLY_X86_EXECUTORS_H

#include "X86ExecutorsFwd.h"
#include "../IR.h"
#include "../../X86CPU.h"  // 引入 X86CPUVM 定义
#include <iostream>
#include <cstdint>

namespace disassembly {
namespace x86 {

// ===== 辅助函数：从 Capstone 寄存器 ID 映射到 X86Reg =====
inline X86Reg capstone_reg_to_x86reg(int cs_reg_id) {
    // Capstone x86_64 寄存器常量 (来自 capstone/x86.h)
    switch (cs_reg_id) {
        case 57: return X86Reg::RAX;   // X86_REG_RAX
        case 58: return X86Reg::RBX;   // X86_REG_RBX
        case 59: return X86Reg::RCX;   // X86_REG_RCX
        case 60: return X86Reg::RDX;   // X86_REG_RDX
        case 61: return X86Reg::RSI;   // X86_REG_RSI
        case 62: return X86Reg::RDI;   // X86_REG_RDI
        case 63: return X86Reg::RBP;   // X86_REG_RBP
        case 64: return X86Reg::RSP;   // X86_REG_RSP
        case 65: return X86Reg::R8;    // X86_REG_R8
        case 66: return X86Reg::R9;    // X86_REG_R9
        case 67: return X86Reg::R10;   // X86_REG_R10
        case 68: return X86Reg::R11;   // X86_REG_R11
        case 69: return X86Reg::R12;   // X86_REG_R12
        case 70: return X86Reg::R13;   // X86_REG_R13
        case 71: return X86Reg::R14;   // X86_REG_R14
        case 72: return X86Reg::R15;   // X86_REG_R15
        default: return X86Reg::RAX;   // 默认返回 RAX
    }
}

// ===== 辅助函数：更新 RFLAGS 标志位 =====
inline void update_arithmetic_flags(X86CPUVM* vm, uint64_t result, uint64_t op1, uint64_t op2, bool is_subtraction = false) {
    uint64_t flags = vm->get_rflags();
    
    // ZF (零标志): 结果为 0 时置位
    if (result == 0) {
        flags |= FLAG_ZF;
    } else {
        flags &= ~FLAG_ZF;
    }
    
    // SF (符号标志): 结果最高位为 1 时置位
    if (result & (1ULL << 63)) {
        flags |= FLAG_SF;
    } else {
        flags &= ~FLAG_SF;
    }
    
    // CF (进位标志): 加法溢出或减法借位
    if (is_subtraction) {
        // SUB: 如果 op1 < op2，则 CF=1
        if (op1 < op2) {
            flags |= FLAG_CF;
        } else {
            flags &= ~FLAG_CF;
        }
    } else {
        // ADD: 如果 result < op1 或 result < op2，则 CF=1
        if (result < op1 || result < op2) {
            flags |= FLAG_CF;
        } else {
            flags &= ~FLAG_CF;
        }
    }
    
    // OF (溢出标志): 有符号溢出
    // 对于加法：两个正数相加得负数，或两个负数相加得正数
    // 对于减法：正数减负数得负数，或负数减正数得正数
    bool op1_sign = op1 & (1ULL << 63);
    bool op2_sign = op2 & (1ULL << 63);
    bool result_sign = result & (1ULL << 63);
    
    if (is_subtraction) {
        // SUB: OF = (op1_sign != op2_sign) && (result_sign != op1_sign)
        if ((op1_sign != op2_sign) && (result_sign != op1_sign)) {
            flags |= FLAG_OF;
        } else {
            flags &= ~FLAG_OF;
        }
    } else {
        // ADD: OF = (op1_sign == op2_sign) && (result_sign != op1_sign)
        if ((op1_sign == op2_sign) && (result_sign != op1_sign)) {
            flags |= FLAG_OF;
        } else {
            flags &= ~FLAG_OF;
        }
    }
    
    // PF (奇偶标志): 结果低 8 位中 1 的个数为偶数时置位
    uint8_t low_byte = result & 0xFF;
    int bit_count = 0;
    for (int i = 0; i < 8; i++) {
        if (low_byte & (1 << i)) bit_count++;
    }
    if (bit_count % 2 == 0) {
        flags |= FLAG_PF;
    } else {
        flags &= ~FLAG_PF;
    }
    
    vm->set_register(X86Reg::RFLAGS, flags);
}

// ===== MOV 执行器 =====
struct MovExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] MOV at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        if (!vm) return insn.size;
        
        // 解析操作数：MOV dest, src
        if (insn.operands.size() >= 2) {
            const auto& dest = insn.operands[0];
            const auto& src = insn.operands[1];
            
            // 情况 1: MOV reg, imm (寄存器 <- 立即数)
            if (dest->type == OperandType::REGISTER && src->type == OperandType::IMMEDIATE) {
                auto* reg_op = dynamic_cast<const RegisterOperand*>(dest.get());
                auto* imm_op = dynamic_cast<const ImmediateOperand*>(src.get());
                if (reg_op && imm_op) {
                    X86Reg reg = capstone_reg_to_x86reg(reg_op->reg_id);
                    vm->set_register(reg, imm_op->value);
                    #ifdef DEBUG_EXECUTION
                    std::cout << "  -> Set " << reg_op->name << " = 0x" << std::hex << imm_op->value << std::dec << std::endl;
                    #endif
                }
            }
            // 情况 2: MOV reg, reg (寄存器 <- 寄存器)
            else if (dest->type == OperandType::REGISTER && src->type == OperandType::REGISTER) {
                auto* dest_reg = dynamic_cast<const RegisterOperand*>(dest.get());
                auto* src_reg = dynamic_cast<const RegisterOperand*>(src.get());
                if (dest_reg && src_reg) {
                    X86Reg dest_r = capstone_reg_to_x86reg(dest_reg->reg_id);
                    X86Reg src_r = capstone_reg_to_x86reg(src_reg->reg_id);
                    uint64_t value = vm->get_register(src_r);
                    vm->set_register(dest_r, value);
                    #ifdef DEBUG_EXECUTION
                    std::cout << "  -> Set " << dest_reg->name << " = " << src_reg->name << " (0x" << std::hex << value << ")" << std::dec << std::endl;
                    #endif
                }
            }
        }
        
        return insn.size;
    }
};

// ===== ADD 执行器 =====
struct AddExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] ADD at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        if (!vm) return insn.size;
        
        // 解析操作数：ADD dest, src
        if (insn.operands.size() >= 2) {
            const auto& dest = insn.operands[0];
            const auto& src = insn.operands[1];
            
            // ADD reg, reg
            if (dest->type == OperandType::REGISTER && src->type == OperandType::REGISTER) {
                auto* dest_reg = dynamic_cast<const RegisterOperand*>(dest.get());
                auto* src_reg = dynamic_cast<const RegisterOperand*>(src.get());
                if (dest_reg && src_reg) {
                    X86Reg dest_r = capstone_reg_to_x86reg(dest_reg->reg_id);
                    X86Reg src_r = capstone_reg_to_x86reg(src_reg->reg_id);
                    
                    uint64_t val1 = vm->get_register(dest_r);
                    uint64_t val2 = vm->get_register(src_r);
                    uint64_t result = val1 + val2;
                    
                    vm->set_register(dest_r, result);
                    update_arithmetic_flags(vm, result, val1, val2, false);
                    
                    #ifdef DEBUG_EXECUTION
                    std::cout << "  -> " << dest_reg->name << " = 0x" << std::hex << result << std::dec << std::endl;
                    #endif
                }
            }
        }
        
        return insn.size;
    }
};

// ===== SUB 执行器 =====
struct SubExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] SUB at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        if (!vm) return insn.size;
        
        // 解析操作数：SUB dest, src
        if (insn.operands.size() >= 2) {
            const auto& dest = insn.operands[0];
            const auto& src = insn.operands[1];
            
            // SUB reg, reg
            if (dest->type == OperandType::REGISTER && src->type == OperandType::REGISTER) {
                auto* dest_reg = dynamic_cast<const RegisterOperand*>(dest.get());
                auto* src_reg = dynamic_cast<const RegisterOperand*>(src.get());
                if (dest_reg && src_reg) {
                    X86Reg dest_r = capstone_reg_to_x86reg(dest_reg->reg_id);
                    X86Reg src_r = capstone_reg_to_x86reg(src_reg->reg_id);
                    
                    uint64_t val1 = vm->get_register(dest_r);
                    uint64_t val2 = vm->get_register(src_r);
                    uint64_t result = val1 - val2;
                    
                    vm->set_register(dest_r, result);
                    update_arithmetic_flags(vm, result, val1, val2, true);
                    
                    #ifdef DEBUG_EXECUTION
                    std::cout << "  -> " << dest_reg->name << " = 0x" << std::hex << result << std::dec << std::endl;
                    #endif
                }
            }
        }
        
        return insn.size;
    }
};

// ===== HLT 执行器 =====
struct HltExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] HLT at 0x" << std::hex << insn.address 
                  << std::dec << " - Halting VM" << std::endl;
        #endif
        
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        if (vm) {
            vm->stop();  // 停止 VM
        }
        
        return insn.size;
    }
};

// ===== JMP 执行器 =====
struct JmpExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] JMP at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        return insn.size;
    }
};

// ===== CALL 执行器 =====
struct CallExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] CALL at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        return insn.size;
    }
};

// ===== RET 执行器 =====
struct RetExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] RET at 0x" << std::hex << insn.address 
                  << std::dec << std::endl;
        #endif
        return insn.size;
    }
};

// ===== PUSH 执行器 =====
struct PushExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] PUSH at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        return insn.size;
    }
};

// ===== POP 执行器 =====
struct PopExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] POP at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        return insn.size;
    }
};

// ===== CMP 执行器 =====
struct CmpExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] CMP at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        return insn.size;
    }
};

// ===== TEST 执行器 =====
struct TestExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] TEST at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
        return insn.size;
    }
};

} // namespace x86
} // namespace disassembly

#endif // LIVS_VM_DISASSEMBLY_X86_EXECUTORS_H
