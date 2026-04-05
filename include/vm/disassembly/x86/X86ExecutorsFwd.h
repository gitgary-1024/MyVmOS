#ifndef LIVS_VM_DISASSEMBLY_X86_EXECUTORS_FWD_H
#define LIVS_VM_DISASSEMBLY_X86_EXECUTORS_FWD_H

#include <variant>
#include <cstdint>

namespace disassembly {

// 前向声明
struct ExecutionContext;
class Instruction;

namespace x86 {

// ===== 前向声明：执行器类型 =====
// 这些 struct 将在 X86Executors.h 中完整定义
struct MovExecutor;
struct AddExecutor;
struct SubExecutor;
struct HltExecutor;
struct JmpExecutor;
struct CallExecutor;
struct RetExecutor;
struct PushExecutor;
struct PopExecutor;
struct CmpExecutor;
struct TestExecutor;

// ===== ExecutorVariant 类型定义 =====
// 使用 variant 存储所有可能的执行器类型
using ExecutorVariant = std::variant<
    MovExecutor,
    AddExecutor,
    SubExecutor,
    HltExecutor,
    JmpExecutor,
    CallExecutor,
    RetExecutor,
    PushExecutor,
    PopExecutor,
    CmpExecutor,
    TestExecutor
>;

} // namespace x86
} // namespace disassembly

#endif // LIVS_VM_DISASSEMBLY_X86_EXECUTORS_FWD_H
