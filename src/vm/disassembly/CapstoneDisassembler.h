#ifndef DISASSEMBLY_CAPSTONE_DISASSEMBLER_H
#define DISASSEMBLY_CAPSTONE_DISASSEMBLER_H

#include "BasicBlock.h"
#include <memory>
#include <string>

// Capstone 前向声明
typedef uintptr_t csh;
typedef struct cs_insn cs_insn;

namespace disassembly {

// 使用 cfg 命名空间的 SimpleInstruction
using cfg::SimpleInstruction;

/**
 * @brief Capstone 反汇编器封装（简化版）
 * 
 * 专门为 ControlFlowGraph 设计，提供指令识别功能
 */
class CapstoneDisassembler {
public:
    CapstoneDisassembler();
    ~CapstoneDisassembler();
    
    // 禁止拷贝
    CapstoneDisassembler(const CapstoneDisassembler&) = delete;
    CapstoneDisassembler& operator=(const CapstoneDisassembler&) = delete;
    
    /**
     * @brief 反汇编单条指令
     * @param code 代码指针
     * @param code_size 代码总大小
     * @param address 当前地址
     * @return 解析后的指令，失败返回 nullptr
     */
    std::shared_ptr<SimpleInstruction> disassemble_instruction(
        const uint8_t* code,
        size_t code_size,
        uint64_t address
    ) const;
    
    /**
     * @brief 判断是否为无条件跳转
     */
    static bool is_unconditional_jump(const SimpleInstruction* insn);
    
    /**
     * @brief 判断是否为条件跳转
     */
    static bool is_conditional_jump(const SimpleInstruction* insn);
    
    /**
     * @brief 判断是否为 CALL 指令
     */
    static bool is_call(const SimpleInstruction* insn);
    
    /**
     * @brief 判断是否为终止指令
     */
    static bool is_terminator(const SimpleInstruction* insn);
    
    /**
     * @brief 提取跳转目标地址（RIP-relative）
     */
    static uint64_t extract_jump_target(
        const SimpleInstruction* insn,
        uint64_t current_addr
    );
    
private:
    csh handle_;  // Capstone 句柄
};

} // namespace phase2_3

#endif // PHASE2_3_CAPSTONE_DISASSEMBLER_H
