#include "disassembly/x86/X86Instruction.h"
#include "disassembly/x86/X86ExecutorRegistry.h"  // 引入执行器注册表
#include "disassembly/InstructionWithExecutor.h"   // 引入带执行器的指令类
#include <capstone/capstone.h>
#include <capstone/x86.h>
#include <sstream>

namespace disassembly {

// ===== 内存操作数的字符串表示 =====
std::string MemoryOperand::to_string() const {
    std::ostringstream oss;
    oss << "[";
    
    if (base_reg != -1) {
        // 优先使用提供的寄存器名称，否则回退到映射表
        if (!base_reg_name.empty()) {
            oss << base_reg_name;
        } else {
            oss << x86::X86InstructionData::get_reg_name(static_cast<x86::X86Reg>(base_reg));
        }
    }
    
    if (index_reg != -1) {
        if (base_reg != -1) oss << "+";
        // 优先使用提供的寄存器名称，否则回退到映射表
        if (!index_reg_name.empty()) {
            oss << index_reg_name;
        } else {
            oss << x86::X86InstructionData::get_reg_name(static_cast<x86::X86Reg>(index_reg));
        }
        if (scale > 1) {
            oss << "*" << scale;
        }
    }
    
    if (displacement != 0) {
        if (base_reg != -1 || index_reg != -1) {
            oss << (displacement > 0 ? "+" : "");
        }
        oss << displacement;
    }
    
    oss << "]";
    return oss.str();
}

namespace x86 {

// ===== 寄存器名称映射 =====
std::string X86InstructionData::get_reg_name(X86Reg reg) {
    static const std::string reg_names[] = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "rip", "rflags",
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
        "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
        "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh"
    };
    
    int idx = static_cast<int>(reg);
    if (idx >= 0 && idx < 50) {
        return reg_names[idx];
    }
    return "invalid";
}

// ===== x86 指令类别映射 =====
InstructionCategory map_x86_category(unsigned int cs_insn_id) {
    // 根据 Capstone x86 指令 ID 映射到通用类别
    // 这里只列出常用指令，完整映射需要参考 capstone/x86.h
    
    // DATA_TRANSFER
    if (cs_insn_id >= X86_INS_MOV && cs_insn_id <= X86_INS_MOVZX) {
        return InstructionCategory::DATA_TRANSFER;
    }
    
    // ARITHMETIC
    if ((cs_insn_id >= X86_INS_ADD && cs_insn_id <= X86_INS_ADCX) ||
        (cs_insn_id >= X86_INS_SUB && cs_insn_id <= X86_INS_SBB) ||
        cs_insn_id == X86_INS_MUL || cs_insn_id == X86_INS_IMUL ||
        cs_insn_id == X86_INS_DIV || cs_insn_id == X86_INS_IDIV ||
        cs_insn_id == X86_INS_INC || cs_insn_id == X86_INS_DEC ||
        cs_insn_id == X86_INS_NEG) {
        return InstructionCategory::ARITHMETIC;
    }
    
    // LOGICAL
    if (cs_insn_id == X86_INS_AND || cs_insn_id == X86_INS_OR ||
        cs_insn_id == X86_INS_XOR || cs_insn_id == X86_INS_NOT ||
        cs_insn_id == X86_INS_TEST) {
        return InstructionCategory::LOGICAL;
    }
    
    // CONTROL_FLOW
    if (cs_insn_id == X86_INS_JMP || cs_insn_id == X86_INS_CALL ||
        cs_insn_id == X86_INS_RET || cs_insn_id == X86_INS_RETF ||
        (cs_insn_id >= X86_INS_JAE && cs_insn_id <= X86_INS_JS)) {
        return InstructionCategory::CONTROL_FLOW;
    }
    
    // STACK_OPERATION
    if (cs_insn_id == X86_INS_PUSH || cs_insn_id == X86_INS_POP ||
        cs_insn_id == X86_INS_ENTER || cs_insn_id == X86_INS_LEAVE) {
        return InstructionCategory::STACK_OPERATION;
    }
    
    // INTERRUPT
    if (cs_insn_id == X86_INS_INT || cs_insn_id == X86_INS_INT3 ||
        cs_insn_id == X86_INS_IRET || cs_insn_id == X86_INS_IRETD ||
        cs_insn_id == X86_INS_IRETQ) {
        return InstructionCategory::INTERRUPT;
    }
    
    // HALT
    if (cs_insn_id == X86_INS_HLT) {
        return InstructionCategory::HALT;
    }
    
    // SYSTEM
    if (cs_insn_id == X86_INS_SYSCALL || cs_insn_id == X86_INS_SYSRET ||
        cs_insn_id == X86_INS_CPUID) {
        return InstructionCategory::SYSTEM;
    }
    
    return InstructionCategory::UNKNOWN;
}

// ===== 从 Capstone 指令创建通用 IR（带执行器）=====
std::shared_ptr<InstructionWithExecutor> create_instruction_from_capstone(
    const void* cs_insn_ptr,
    uint64_t address,
    void* capstone_handle
) {
    csh handle = reinterpret_cast<csh>(capstone_handle);
    const struct cs_insn* capstone_insn = reinterpret_cast<const struct cs_insn*>(cs_insn_ptr);
    
    auto instruction = std::make_shared<InstructionWithExecutor>();
    instruction->address = capstone_insn->address;
    instruction->size = capstone_insn->size;
    instruction->mnemonic = capstone_insn->mnemonic;
    instruction->operands_str = capstone_insn->op_str;
    instruction->category = map_x86_category(capstone_insn->id);
    
    // 创建 x86 特有数据（使用 shared_ptr 管理内存）
    auto x86_data = std::make_shared<X86InstructionData>();
    
    // 提取 x86 详细信息
    if (capstone_insn->detail) {
        const cs_x86* x86_detail = &capstone_insn->detail->x86;
        
        // 提取前缀信息（改进 REX 前缀检测）
        for (int i = 0; i < 4; i++) {
            uint8_t prefix = x86_detail->prefix[i];
            if (prefix >= 0x40 && prefix <= 0x4F) {
                x86_data->prefix.has_rex = true;
                x86_data->prefix.rex_value = prefix;
            } else if (prefix == 0x66) {
                x86_data->prefix.has_operand_size = true;
            } else if (prefix == 0x67) {
                x86_data->prefix.has_address_size = true;
            } else if (prefix == 0xF0) {
                x86_data->prefix.has_lock = true;
            } else if (prefix == 0xF3) {
                x86_data->prefix.has_rep = true;
            } else if (prefix == 0xF2) {
                x86_data->prefix.has_repne = true;
            }
        }
        
        // 改进 REX 前缀检测：检查 Capstone 的 rex 字段
        if (capstone_insn->detail->x86.rex != 0) {
            x86_data->prefix.has_rex = true;
            x86_data->prefix.rex_value = capstone_insn->detail->x86.rex;
        }
        
        // 提取 ModR/M 和 SIB（从 modrm 和 sib 字节中解析）
        if (x86_detail->modrm != 0) {
            x86_data->modrm.mod = (x86_detail->modrm >> 6) & 0x03;
            x86_data->modrm.reg = (x86_detail->modrm >> 3) & 0x07;
            x86_data->modrm.rm = x86_detail->modrm & 0x07;
        }
        
        if (x86_detail->sib != 0) {
            x86_data->sib.scale = (x86_detail->sib >> 6) & 0x03;
            x86_data->sib.index = (x86_detail->sib >> 3) & 0x07;
            x86_data->sib.base = x86_detail->sib & 0x07;
            x86_data->has_sib = true;
        }
        
        // 提取位移和立即数
        x86_data->displacement = x86_detail->disp;
        
        // 提取操作码
        x86_data->opcode_length = 0;
        for (int i = 0; i < 4 && x86_detail->opcode[i] != 0; i++) {
            x86_data->opcode[i] = x86_detail->opcode[i];
            x86_data->opcode_length++;
        }
        
        // 保存 Capstone 原始信息
        x86_data->cs_insn_id = capstone_insn->id;
        x86_data->cs_group_count = capstone_insn->detail->groups_count;
        for (uint8_t i = 0; i < capstone_insn->detail->groups_count && i < 8; i++) {
            x86_data->cs_groups[i] = capstone_insn->detail->groups[i];
        }
        
        // 创建通用操作数
        for (uint8_t i = 0; i < x86_detail->op_count; i++) {
            const cs_x86_op& op = x86_detail->operands[i];
            
            switch (op.type) {
                case X86_OP_REG: {
                    OperandSize size = OperandSize::QWORD;
                    if (op.size == 1) size = OperandSize::BYTE;
                    else if (op.size == 2) size = OperandSize::WORD;
                    else if (op.size == 4) size = OperandSize::DWORD;
                    
                    // 使用 Capstone 的 cs_reg_name() API 获取寄存器名称
                    const char* reg_name = cs_reg_name(handle, op.reg);
                    std::string reg_name_str = reg_name ? reg_name : "unknown";
                    
                    auto reg_op = std::make_shared<RegisterOperand>(
                        op.reg,
                        reg_name_str,
                        size
                    );
                    instruction->operands.push_back(reg_op);
                    break;
                }
                
                case X86_OP_IMM: {
                    OperandSize size = OperandSize::QWORD;
                    if (op.size == 1) size = OperandSize::BYTE;
                    else if (op.size == 2) size = OperandSize::WORD;
                    else if (op.size == 4) size = OperandSize::DWORD;
                    
                    auto imm_op = std::make_shared<ImmediateOperand>(
                        op.imm,
                        size
                    );
                    instruction->operands.push_back(imm_op);
                    break;
                }
                
                case X86_OP_MEM: {
                    OperandSize size = OperandSize::QWORD;
                    if (op.size == 1) size = OperandSize::BYTE;
                    else if (op.size == 2) size = OperandSize::WORD;
                    else if (op.size == 4) size = OperandSize::DWORD;
                    
                    int base = (op.mem.base != X86_REG_INVALID) ? 
                               static_cast<int>(op.mem.base) : -1;
                    int index = (op.mem.index != X86_REG_INVALID) ? 
                                static_cast<int>(op.mem.index) : -1;
                    
                    // 使用 Capstone API 获取寄存器名称
                    std::string base_name = "unknown";
                    std::string index_name = "unknown";
                    if (base != -1) {
                        const char* name = cs_reg_name(handle, op.mem.base);
                        if (name) base_name = name;
                    }
                    if (index != -1) {
                        const char* name = cs_reg_name(handle, op.mem.index);
                        if (name) index_name = name;
                    }
                    
                    auto mem_op = std::make_shared<MemoryOperand>(
                        base,
                        index,
                        (1 << op.mem.scale),
                        op.mem.disp,
                        size,
                        base_name,
                        index_name
                    );
                    instruction->operands.push_back(mem_op);
                    break;
                }
                
                default:
                    break;
            }
        }
    }
    
    // 将 x86 特有数据附加到通用指令（shared_ptr 自动管理内存）
    instruction->arch_specific_data = x86_data;
    
    // Task 1.4: 绑定执行器
    // 根据助记符查找对应的执行器并绑定到指令
    try {
        auto& registry = x86::X86ExecutorRegistry::instance();
        auto executor = registry.create_executor(instruction->mnemonic);
        instruction->bind_executor(executor);
    } catch (const std::exception& e) {
        // 异常情况下也使用 fallback（HltExecutor），保证系统稳定性
        auto& registry = x86::X86ExecutorRegistry::instance();
        auto fallback = registry.create_executor("hlt");  // 强制使用 HLT
        instruction->bind_executor(fallback);
    }
    
    return instruction;
}

} // namespace x86
} // namespace disassembly
