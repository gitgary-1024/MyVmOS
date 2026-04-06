#pragma once

#include "../src/vm/disassembly/CapstoneDisassembler.h"

namespace disassembly {

/**
 * @brief 增强的 Capstone 反汇编器（支持 CALL 检测）
 */
class EnhancedCapstoneDisassembler : public CapstoneDisassembler {
public:
    /**
     * @brief 判断是否是 CALL 指令
     * 
     * CALL 指令包括：
     * - call rel32 (E8)
     * - call rel8 (短调用，很少见)
     * - call r/m64 (FF /2)
     * - callq (AT&T 语法)
     */
    static bool is_call(const SimpleInstruction* insn) {
        if (!insn) return false;
        
        const std::string& mnem = insn->mnemonic;
        
        // Intel 语法：call, callq
        return (
            mnem == "call"   ||  // CALL rel32 / CALL rel8 / CALL r/m64
            mnem == "callq"      // AT&T 语法的 CALL
        );
    }
};

} // namespace disassembly
