#ifndef LIVS_VM_DISASSEMBLY_X86_INSTRUCTION_H
#define LIVS_VM_DISASSEMBLY_X86_INSTRUCTION_H

#include "../IR.h"
#include "../InstructionWithExecutor.h"  // 引入带执行器的指令类
#include <cstdint>
#include <string>

namespace disassembly {
namespace x86 {

// ===== x86 寄存器枚举（与 Capstone 对应）=====
enum class X86Reg {
    RAX = 0, RBX = 1, RCX = 2, RDX = 3,
    RSI = 4, RDI = 5, RBP = 6, RSP = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    RIP = 16, RFLAGS = 17,
    
    // 32 位寄存器
    EAX = 18, EBX = 19, ECX = 20, EDX = 21,
    ESI = 22, EDI = 23, EBP = 24, ESP = 25,
    R8D = 26, R9D = 27, R10D = 28, R11D = 29,
    R12D = 30, R13D = 31, R14D = 32, R15D = 33,
    
    // 16 位寄存器
    AX = 34, BX = 35, CX = 36, DX = 37,
    SI = 38, DI = 39, BP = 40, SP = 41,
    
    // 8 位寄存器
    AL = 42, BL = 43, CL = 44, DL = 45,
    AH = 46, BH = 47, CH = 48, DH = 49,
    
    INVALID = -1
};

// ===== x86 指令前缀 =====
struct X86Prefix {
    bool has_rex;           // 是否有 REX 前缀
    uint8_t rex_value;      // REX 前缀值 (0x40-0x4F)
    bool has_operand_size;  // 操作数大小前缀 (0x66)
    bool has_address_size;  // 地址大小前缀 (0x67)
    bool has_lock;          // LOCK 前缀 (0xF0)
    bool has_rep;           // REP 前缀 (0xF3)
    bool has_repne;         // REPNE 前缀 (0xF2)
    
    X86Prefix()
        : has_rex(false), rex_value(0),
          has_operand_size(false), has_address_size(false),
          has_lock(false), has_rep(false), has_repne(false) {}
};

// ===== x86 ModR/M 字段 =====
struct X86ModRM {
    uint8_t mod;    // 模式 (0-3)
    uint8_t reg;    // 寄存器字段
    uint8_t rm;     // R/M 字段
    
    X86ModRM() : mod(0), reg(0), rm(0) {}
};

// ===== x86 SIB 字段 =====
struct X86SIB {
    uint8_t scale;  // 缩放因子 (0-3, 对应 1, 2, 4, 8)
    uint8_t index;  // 索引寄存器
    uint8_t base;   // 基址寄存器
    
    X86SIB() : scale(0), index(0), base(0) {}
};

// ===== x86 特有指令数据（组合到通用 Instruction 中）=====
struct X86InstructionData {
    X86Prefix prefix;       // 指令前缀
    X86ModRM modrm;         // ModR/M 字节
    X86SIB sib;             // SIB 字节
    bool has_sib;           // 是否有 SIB 字节
    
    int64_t displacement;   // 位移量
    uint64_t immediate;     // 立即数
    
    uint8_t opcode_length;  // 操作码长度（1 或 2 字节）
    uint8_t opcode[2];      // 操作码字节
    
    // Capstone 原始信息
    unsigned int cs_insn_id;        // Capstone 指令 ID
    unsigned int cs_group_count;    // Capstone 组数量
    uint8_t cs_groups[8];           // Capstone 组 ID
    
    X86InstructionData()
        : has_sib(false), displacement(0), immediate(0),
          opcode_length(1), cs_insn_id(0), cs_group_count(0) {
        opcode[0] = 0;
        opcode[1] = 0;
    }
    
    // 获取寄存器名称
    static std::string get_reg_name(X86Reg reg);
    
    // 判断是否是 64 位指令
    bool is_64bit() const {
        return prefix.has_rex && (prefix.rex_value & 0x08); // REX.W
    }
};

// ===== x86 指令类别映射 =====
InstructionCategory map_x86_category(unsigned int cs_insn_id);

// ===== 从 Capstone 指令创建通用 IR（带执行器）=====
std::shared_ptr<InstructionWithExecutor> create_instruction_from_capstone(
    const void* cs_insn,     // Capstone cs_insn* 指针
    uint64_t address,
    void* capstone_handle    // Capstone csh 句柄（用于 cs_reg_name）
);

} // namespace x86
} // namespace disassembly

#endif // LIVS_VM_DISASSEMBLY_X86_INSTRUCTION_H
