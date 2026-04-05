#ifndef LIVS_VM_DISASSEMBLY_X86_EXECUTORS_H
#define LIVS_VM_DISASSEMBLY_X86_EXECUTORS_H

#include "X86ExecutorsFwd.h"
#include "../IR.h"
#include <iostream>
#include <cstdint>

namespace disassembly {
namespace x86 {

// ===== MOV 执行器 =====
struct MovExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        // TODO: 实际的 MOV 逻辑需要访问 VM 实例
        // 目前仅返回指令长度作为占位实现
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] MOV at 0x" << std::hex << insn.address 
                  << std::dec << " - " << insn.to_string() << std::endl;
        #endif
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
        return insn.size;
    }
};

// ===== HLT 执行器 =====
struct HltExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        #ifdef DEBUG_EXECUTION
        std::cout << "[EXEC] HLT at 0x" << std::hex << insn.address 
                  << std::dec << " - Halting" << std::endl;
        #endif
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
