#ifndef LIVS_VM_DISASSEMBLY_INSTRUCTION_WITH_EXECUTOR_H
#define LIVS_VM_DISASSEMBLY_INSTRUCTION_WITH_EXECUTOR_H

#include "IR.h"
#include "disassembly/x86/X86Executors.h"  // 包含完整定义
#include <variant>

namespace disassembly {

// ===== 带执行器的指令类（扩展 Instruction）=====
class InstructionWithExecutor : public Instruction {
public:
    // ⭐ 执行器 variant（零开销抽象）
    x86::ExecutorVariant executor;
    
    InstructionWithExecutor() : Instruction() {}
    
    virtual ~InstructionWithExecutor() = default;
    
    // 执行指令（使用 std::visit 分发）
    int execute(ExecutionContext& ctx) {
        return std::visit(
            [&](const auto& exec) {
                return exec.execute(ctx, *this);
            },
            executor
        );
    }
    
    // 绑定执行器
    void bind_executor(x86::ExecutorVariant exec) {
        executor = std::move(exec);
    }
};

} // namespace disassembly

#endif // LIVS_VM_DISASSEMBLY_INSTRUCTION_WITH_EXECUTOR_H
