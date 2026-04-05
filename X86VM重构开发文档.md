# X86VM 重构开发文档

## 📋 文档信息

- **版本**: v1.0
- **日期**: 2026-03-15
- **状态**: 设计阶段
- **作者**: AI Assistant
- **审核**: Pending

---

## 🎯 项目概述

### **背景**

当前的 `X86CPUVM` 使用手写解码器架构，存在以下问题：

1. **解码和执行耦合**：`decode_and_execute()` 函数同时负责解析字节码和执行指令
2. **难以调试**：无法方便地查看即将执行的指令及其详细信息
3. **扩展困难**：每增加一条指令都需要修改核心 switch-case 逻辑
4. **代码冗余**：大量重复的 ModR/M 解析和寄存器访问代码
5. **缺乏灵活性**：无法动态替换指令行为（断点、热补丁等）

### **目标**

重构 X86VM，采用 **Capstone 反汇编 + 通用 IR + 访问者模式执行器** 的新架构：

```
字节码 → Capstone 反汇编 → 通用 IR + x86特有数据 → std::variant<Executor> → 访问者调用
```

### **核心优势**

✅ **职责分离**：解码、IR 表示、执行逻辑完全解耦  
✅ **零开销抽象**：使用 `std::variant` + 访问者模式，性能接近 switch-case  
✅ **类型安全**：编译期类型检查，无虚函数调用开销  
✅ **易于调试**：可以观察、修改、替换任意指令的执行逻辑  
✅ **可扩展性**：添加新指令只需扩展 variant 类型和访问者  
✅ **测试友好**：执行器可以独立单元测试  

---

## 🏗️ 架构设计

### **整体架构图**

```
┌─────────────────────────────────────────────────────────────┐
│                    X86CPUVM (重构后)                         │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐    ┌──────────────────┐                   │
│  │ 字节码内存    │───→│ Capstone         │                   │
│  │ (physical_   │    │ Disassembler     │                   │
│  │  _memory_)   │    │                  │                   │
│  └──────────────┘    └────────┬─────────┘                   │
│                                │                              │
│                                ▼                              │
│                       ┌──────────────────┐                   │
│                       │ Instruction IR   │                   │
│                       │                  │                   │
│                       │ - mnemonic       │                   │
│                       │ - operands[]     │                   │
│                       │ - x86_data       │                   │
│                       │ - executor<> ⭐  │ ← 关键创新        │
│                       └────────┬─────────┘                   │
│                                │                              │
│                                ▼                              │
│                       ┌──────────────────┐                   │
│                       │ Executor Registry│                   │
│                       │                  │                   │
│                       │ - mov_executor   │                   │
│                       │ - add_executor   │                   │
│                       │ - sub_executor   │                   │
│                       │ - ...            │                   │
│                       └────────┬─────────┘                   │
│                                │                              │
│                                ▼                              │
│                       ┌──────────────────┐                   │
│                       │ ExecutionContext │                   │
│                       │                  │                   │
│                       │ - vm_instance    │                   │
│                       │ - rip            │                   │
│                       └────────┬─────────┘                   │
│                                │                              │
│                                ▼                              │
│                       ┌──────────────────┐                   │
│                       │ VM State Update  │                   │
│                       │ (registers,      │                   │
│                       │  memory, flags)  │                   │
│                       └──────────────────┘                   │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 🚀 性能优化：访问者模式 + std::variant

### **问题分析**

使用 `std::function` 虽然灵活，但存在性能开销：

1. **类型擦除开销**：`std::function` 内部使用虚函数调用
2. **堆分配**：lambda 捕获可能导致堆分配
3. **间接调用**：无法内联优化
4. **缓存不友好**：指令对象分散在堆上

**性能对比**：
- `std::function` 调用：~10-20 ns
- 直接函数调用：~1-2 ns
- switch-case：~0.5-1 ns

---

### **解决方案：访问者模式 + std::variant**

#### **核心思想**

将执行器定义为**编译期已知的类型集合**，使用 `std::variant` 存储，通过**访问者模式**分发执行。

```cpp
// 定义各种执行器类型（struct with operator()）
struct MovExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const;
};

struct AddExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const;
};

struct SubExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const;
};

// ... 更多执行器

// 使用 variant 存储执行器
using ExecutorVariant = std::variant<
    MovExecutor,
    AddExecutor,
    SubExecutor,
    HltExecutor,
    JmpExecutor,
    // ... 更多
>;

// 在 Instruction 中存储
struct Instruction {
    ExecutorVariant executor;  // ⭐ 替代 std::function
    
    // 执行指令
    int execute(ExecutionContext& ctx) {
        return std::visit(
            [&](const auto& exec) { 
                return exec.execute(ctx, *this); 
            },
            executor
        );
    }
};
```

---

### **优势对比**

| 特性 | std::function | std::variant + Visitor |
|------|--------------|------------------------|
| **调用开销** | ~10-20 ns (虚函数) | ~1-2 ns (模板展开) |
| **内联优化** | ❌ 不可能 | ✅ 编译器可内联 |
| **类型安全** | ⚠️ 运行期检查 | ✅ 编译期检查 |
| **内存占用** | ~32-64 bytes | ~1-8 bytes (索引) |
| **灵活性** | ✅ 动态绑定 | ⚠️ 编译期确定 |
| **扩展性** | ✅ 容易 | ⚠️ 需修改 variant |

**结论**：对于 x86 指令集这种**固定且已知**的场景，`std::variant` 是更好的选择！

---

### **完整实现示例**

#### **1. 定义执行器类型**

```cpp
// include/vm/disassembly/x86/X86Executors.h

namespace disassembly {
namespace x86 {

// 前向声明
struct ExecutionContext;
class Instruction;

// ===== MOV 执行器 =====
struct MovExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        
        if (insn.operands.size() != 2) {
            return 0;
        }
        
        auto& dest = insn.operands[0];
        auto& src = insn.operands[1];
        
        // MOV reg, imm
        if (dest->type == OperandType::REGISTER && 
            src->type == OperandType::IMMEDIATE) {
            
            auto reg_op = std::static_pointer_cast<RegisterOperand>(dest);
            auto imm_op = std::static_pointer_cast<ImmediateOperand>(src);
            
            vm->set_register(
                static_cast<X86Reg>(reg_op->reg_id), 
                imm_op->value
            );
            
            return insn.size;
        }
        
        // MOV reg, reg
        if (dest->type == OperandType::REGISTER && 
            src->type == OperandType::REGISTER) {
            
            auto dest_reg = std::static_pointer_cast<RegisterOperand>(dest);
            auto src_reg = std::static_pointer_cast<RegisterOperand>(src);
            
            uint64_t value = vm->get_register(
                static_cast<X86Reg>(src_reg->reg_id));
            vm->set_register(
                static_cast<X86Reg>(dest_reg->reg_id), 
                value
            );
            
            return insn.size;
        }
        
        return 0;  // 不支持的变体
    }
};

// ===== ADD 执行器 =====
struct AddExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        
        if (insn.operands.size() != 2) {
            return 0;
        }
        
        auto& dest = insn.operands[0];
        auto& src = insn.operands[1];
        
        if (dest->type == OperandType::REGISTER && 
            src->type == OperandType::REGISTER) {
            
            auto dest_reg = std::static_pointer_cast<RegisterOperand>(dest);
            auto src_reg = std::static_pointer_cast<RegisterOperand>(src);
            
            uint64_t dest_val = vm->get_register(
                static_cast<X86Reg>(dest_reg->reg_id));
            uint64_t src_val = vm->get_register(
                static_cast<X86Reg>(src_reg->reg_id));
            uint64_t result = dest_val + src_val;
            
            vm->set_register(
                static_cast<X86Reg>(dest_reg->reg_id), 
                result
            );
            
            // TODO: 更新标志位
            
            return insn.size;
        }
        
        return 0;
    }
};

// ===== HLT 执行器 =====
struct HltExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        vm->stop();
        return insn.size;
    }
};

// ... 更多执行器

} // namespace x86
} // namespace disassembly
```

---

#### **2. 定义 Variant 类型**

```cpp
// include/vm/disassembly/x86/X86ExecutorVariant.h

#include "X86Executors.h"
#include <variant>

namespace disassembly {
namespace x86 {

// ⭐ 定义执行器 variant
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
    // ... 根据需要添加更多
>;

// ⭐ 助记符到 variant 索引的映射表
ExecutorVariant create_executor_for_mnemonic(const std::string& mnemonic);

} // namespace x86
} // namespace disassembly
```

---

#### **3. 实现创建函数**

```cpp
// src/vm/disassembly/x86/X86ExecutorVariant.cpp

#include "disassembly/x86/X86ExecutorVariant.h"
#include <unordered_map>

namespace disassembly {
namespace x86 {

ExecutorVariant create_executor_for_mnemonic(const std::string& mnemonic) {
    static const std::unordered_map<std::string, ExecutorVariant> registry = {
        {"mov", MovExecutor{}},
        {"add", AddExecutor{}},
        {"sub", SubExecutor{}},
        {"hlt", HltExecutor{}},
        {"jmp", JmpExecutor{}},
        {"call", CallExecutor{}},
        {"ret", RetExecutor{}},
        {"push", PushExecutor{}},
        {"pop", PopExecutor{}},
        {"cmp", CmpExecutor{}},
        {"test", TestExecutor{}}
        // ... 更多
    };
    
    auto it = registry.find(mnemonic);
    if (it != registry.end()) {
        return it->second;
    }
    
    // 未知指令，返回一个错误执行器
    struct UnknownExecutor {
        int execute(ExecutionContext& ctx, const Instruction& insn) const {
            std::cerr << "[EXEC] Unknown instruction: " << insn.mnemonic << std::endl;
            return 0;
        }
    };
    
    return UnknownExecutor{};
}

} // namespace x86
} // namespace disassembly
```

---

#### **4. 修改 Instruction 类**

```cpp
// include/vm/disassembly/IR.h

#include "disassembly/x86/X86ExecutorVariant.h"

namespace disassembly {

class Instruction {
public:
    uint64_t address;
    uint16_t size;
    std::string mnemonic;
    std::string operands_str;
    InstructionCategory category;
    
    std::vector<std::shared_ptr<Operand>> operands;
    std::shared_ptr<void> arch_specific_data;
    
    // ⭐ 使用 variant 替代 std::function
    x86::ExecutorVariant executor;
    
    Instruction()
        : address(0), size(0), category(InstructionCategory::UNKNOWN) {}
    
    virtual ~Instruction() = default;
    
    std::string to_string() const {
        return mnemonic + " " + operands_str;
    }
    
    // ⭐ 执行指令（使用 std::visit）
    int execute(ExecutionContext& ctx) {
        return std::visit(
            [&](const auto& exec) {
                return exec.execute(ctx, *this);
            },
            executor
        );
    }
    
    // ⭐ 绑定执行器
    void bind_executor(x86::ExecutorVariant exec) {
        executor = std::move(exec);
    }
};

} // namespace disassembly
```

---

#### **5. 集成到反汇编流程**

```cpp
// src/vm/disassembly/x86/X86Instruction.cpp

std::shared_ptr<Instruction> create_instruction_from_capstone(
    const void* cs_insn_ptr,
    uint64_t address,
    void* capstone_handle
) {
    // ... 原有的反汇编逻辑 ...
    
    auto instruction = std::make_shared<Instruction>();
    instruction->address = capstone_insn->address;
    instruction->size = capstone_insn->size;
    instruction->mnemonic = capstone_insn->mnemonic;
    instruction->operands_str = capstone_insn->op_str;
    instruction->category = map_x86_category(capstone_insn->id);
    
    // ... 提取 x86 特有数据 ...
    
    // ⭐ 自动绑定执行器（使用 variant）
    instruction->bind_executor(
        x86::create_executor_for_mnemonic(instruction->mnemonic)
    );
    
    return instruction;
}
```

---

### **性能测试对比**

```cpp
// tests/test_executor_performance.cpp

#include <benchmark/benchmark.h>
#include <functional>
#include <variant>

// std::function 版本
static void BM_StdFunction(benchmark::State& state) {
    std::function<int()> func = []() { return 42; };
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(func());
    }
}

// std::variant + visitor 版本
struct SimpleExecutor {
    int execute() const { return 42; }
};

using ExecutorVar = std::variant<SimpleExecutor>;

static void BM_VariantVisitor(benchmark::State& state) {
    ExecutorVar exec = SimpleExecutor{};
    
    for (auto _ : state) {
        int result = std::visit(
            [](const auto& e) { return e.execute(); },
            exec
        );
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(BM_StdFunction);
BENCHMARK(BM_VariantVisitor);
```

**预期结果**：
```
BM_StdFunction       15 ns         15 ns     45000000
BM_VariantVisitor     2 ns          2 ns    350000000
```

**性能提升**: **7.5x** 🚀

---

### **进一步优化：索引表**

如果指令集很大，可以使用**索引表**进一步优化：

```cpp
// 为每个助记符分配一个整数 ID
enum class ExecutorID {
    MOV = 0,
    ADD = 1,
    SUB = 2,
    HLT = 3,
    // ...
    UNKNOWN = 255
};

// 使用数组而非 unordered_map
static const ExecutorVariant executor_table[] = {
    MovExecutor{},   // ID 0
    AddExecutor{},   // ID 1
    SubExecutor{},   // ID 2
    HltExecutor{},   // ID 3
    // ...
};

// O(1) 查找
ExecutorVariant get_executor(ExecutorID id) {
    return executor_table[static_cast<size_t>(id)];
}
```

---

### **总结**

✅ **零开销抽象**：`std::variant` + `std::visit` 的性能接近 switch-case  
✅ **类型安全**：编译期检查，无运行时错误  
✅ **易于维护**：添加新指令只需添加新的 struct  
✅ **可扩展**：支持默认实现、组合执行器等高级特性  
✅ **现代 C++**：充分利用 C++17 特性  

**推荐方案**：对于 X86VM 重构，**强烈推荐使用访问者模式 + std::variant**！

---

## 📦 模块设计

### **1. 扩展 IR 层**

#### **文件**: `include/vm/disassembly/IR.h`

**新增内容**:

```cpp
namespace disassembly {

// 执行上下文（传递给 functional）
struct ExecutionContext {
    void* vm_instance;  // VM 实例指针（类型擦除）
    uint64_t rip;       // 当前指令地址
    // 未来可扩展：性能计数器、追踪信息等
};

// 执行函数类型定义
using ExecutorFunction = std::function<int(ExecutionContext&)>;
// 返回值：指令长度（字节），0 表示执行失败

class Instruction {
public:
    // ... 现有字段 ...
    
    // ⭐ 新增：绑定的执行函数
    ExecutorFunction executor;
    
    // ⭐ 新增：执行指令
    int execute(ExecutionContext& ctx);
    
    // ⭐ 新增：绑定执行器
    void bind_executor(ExecutorFunction func);
};

} // namespace disassembly
```

**设计理由**:
- `void* vm_instance` 实现类型擦除，避免循环依赖
- `std::function` 提供灵活的 callable 支持（lambda、函数指针、bind 等）
- 返回值统一为指令长度，便于更新 RIP

---

### **2. X86 执行器注册表**

#### **文件**: `include/vm/disassembly/x86/X86Executor.h`

```cpp
namespace disassembly {
namespace x86 {

class X86ExecutorRegistry {
public:
    using ExecutorFactory = std::function<ExecutorFunction(
        const std::shared_ptr<Instruction>&)>;
    
private:
    std::unordered_map<std::string, ExecutorFactory> executors_;
    
public:
    // 单例模式
    static X86ExecutorRegistry& instance();
    
    // 注册执行器工厂
    void register_executor(const std::string& mnemonic, 
                          ExecutorFactory factory);
    
    // 为指令创建执行器
    ExecutorFunction create_executor(
        const std::shared_ptr<Instruction>& insn);
    
    // 初始化所有 x86 指令的执行器
    void initialize_all_executors();
    
private:
    // 各指令的执行器工厂
    static ExecutorFactory make_mov_executor();
    static ExecutorFactory make_add_executor();
    static ExecutorFactory make_sub_executor();
    static ExecutorFactory make_hlt_executor();
    static ExecutorFactory make_jmp_executor();
    static ExecutorFactory make_call_executor();
    static ExecutorFactory make_ret_executor();
    static ExecutorFactory make_push_executor();
    static ExecutorFactory make_pop_executor();
    static ExecutorFactory make_cmp_executor();
    static ExecutorFactory make_test_executor();
    // ... 更多指令
};

} // namespace x86
} // namespace disassembly
```

**设计理由**:
- **工厂模式**：每个指令类型对应一个工厂函数，延迟创建执行器
- **单例模式**：全局共享注册表，避免重复初始化
- **基于助记符**：使用字符串键值，与 Capstone 输出自然对应

---

### **3. 执行器实现示例**

#### **文件**: `src/vm/disassembly/x86/X86Executor.cpp`

```cpp
ExecutorFactory X86ExecutorRegistry::make_mov_executor() {
    return [](const std::shared_ptr<Instruction>& insn) -> ExecutorFunction {
        return [insn](ExecutionContext& ctx) -> int {
            // 1. 获取 VM 实例
            auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
            
            // 2. 获取 x86 特有数据（可选）
            auto x86_data = std::static_pointer_cast<X86InstructionData>(
                insn->arch_specific_data);
            
            // 3. 使用通用 IR 的操作数
            if (insn->operands.size() != 2) {
                std::cerr << "[EXEC] MOV expects 2 operands" << std::endl;
                return 0;
            }
            
            auto& dest = insn->operands[0];
            auto& src = insn->operands[1];
            
            // 4. 根据操作数类型分发
            if (dest->type == OperandType::REGISTER && 
                src->type == OperandType::IMMEDIATE) {
                
                auto reg_op = std::static_pointer_cast<RegisterOperand>(dest);
                auto imm_op = std::static_pointer_cast<ImmediateOperand>(src);
                
                vm->set_register(
                    static_cast<X86Reg>(reg_op->reg_id), 
                    imm_op->value
                );
                
                #ifdef DEBUG_EXECUTION
                std::cout << "[EXEC] MOV " << reg_op->name 
                          << ", 0x" << std::hex << imm_op->value 
                          << std::dec << std::endl;
                #endif
                
                return insn->size;
            }
            
            if (dest->type == OperandType::REGISTER && 
                src->type == OperandType::REGISTER) {
                
                auto dest_reg = std::static_pointer_cast<RegisterOperand>(dest);
                auto src_reg = std::static_pointer_cast<RegisterOperand>(src);
                
                uint64_t value = vm->get_register(
                    static_cast<X86Reg>(src_reg->reg_id));
                vm->set_register(
                    static_cast<X86Reg>(dest_reg->reg_id), 
                    value
                );
                
                return insn->size;
            }
            
            // TODO: 支持内存操作数
            
            std::cerr << "[EXEC] Unsupported MOV variant" << std::endl;
            return 0;
        };
    };
}
```

**关键点**:
- **闭包捕获**：lambda 捕获 `insn` 以访问指令详情
- **类型安全转换**：使用 `static_cast` 和 `static_pointer_cast`
- **错误处理**：返回 0 表示执行失败
- **调试支持**：条件编译的日志输出

---

### **4. 自动绑定执行器**

#### **文件**: `src/vm/disassembly/x86/X86Instruction.cpp`

在 `create_instruction_from_capstone()` 函数末尾添加：

```cpp
// ⭐ 自动绑定执行器
auto& registry = X86ExecutorRegistry::instance();
instruction->bind_executor(registry.create_executor(instruction));

return instruction;
```

**效果**: 每条反汇编生成的指令自动拥有可执行的 functional。

---

### **5. 重构 X86CPUVM**

#### **文件**: `include/vm/X86CPU.h`

**新增成员**:

```cpp
class X86CPUVM : public baseVM {
private:
    // ⭐ Capstone 反汇编器
    std::unique_ptr<disassembly::CapstoneDisassembler> disassembler_;
    
    // ⭐ 执行上下文
    disassembly::ExecutionContext exec_context_;
    
    // ⭐ 指令缓存（性能优化）
    std::unordered_map<uint64_t, std::shared_ptr<disassembly::Instruction>> 
        instruction_cache_;
    
public:
    // 新的执行接口
    bool execute_instruction_ir();
    
private:
    // 从内存中获取指令 IR
    std::shared_ptr<disassembly::Instruction> fetch_instruction(uint64_t addr);
    
    // 保留旧接口用于过渡
    bool execute_instruction_legacy();
};
```

#### **文件**: `src/vm/X86CPU.cpp`

**构造函数初始化**:

```cpp
X86CPUVM::X86CPUVM(uint64_t vm_id, const X86VMConfig& config)
    : vm_id_(vm_id), config_(config), state_(X86VMState::CREATED) {
    
    // ... 原有初始化代码 ...
    
    // ⭐ 初始化 Capstone 反汇编器
    disassembler_ = std::make_unique<disassembly::CapstoneDisassembler>(
        disassembly::Architecture::X86,
        disassembly::Mode::MODE_64
    );
    disassembler_->set_detail_mode(true);
    
    // ⭐ 初始化执行上下文
    exec_context_.vm_instance = this;
    exec_context_.rip = 0;
    
    // ⭐ 初始化执行器注册表（全局一次）
    static bool initialized = false;
    if (!initialized) {
        disassembly::x86::X86ExecutorRegistry::instance()
            .initialize_all_executors();
        initialized = true;
    }
}
```

**新的执行流程**:

```cpp
bool X86CPUVM::execute_instruction_ir() {
    if (!running_ || state_ != X86VMState::RUNNING) {
        return false;
    }
    
    uint64_t rip = get_rip();
    exec_context_.rip = rip;
    
    // 1. 获取指令 IR（带缓存）
    auto insn = fetch_instruction(rip);
    if (!insn) {
        std::cerr << "[X86VM-" << vm_id_ << "] Failed to fetch instruction at 0x"
                  << std::hex << rip << std::dec << std::endl;
        return false;
    }
    
    // 2. 调试输出（可选）
    #ifdef DEBUG_EXECUTION
    std::cout << "[0x" << std::hex << rip << "] " 
              << insn->to_string() << std::dec << std::endl;
    #endif
    
    // 3. 执行指令（通过 functional 调用）⭐
    int length = insn->execute(exec_context_);
    
    if (length > 0) {
        total_instructions_++;
        total_cycles_ += length;
        set_rip(rip + length);
    } else {
        std::cerr << "[X86VM-" << vm_id_ << "] Execution failed at 0x"
                  << std::hex << rip << std::dec << std::endl;
        state_ = X86VMState::ERROR;
        return false;
    }
    
    return true;
}

std::shared_ptr<disassembly::Instruction> 
X86CPUVM::fetch_instruction(uint64_t addr) {
    // ⭐ 检查缓存
    auto it = instruction_cache_.find(addr);
    if (it != instruction_cache_.end()) {
        return it->second;
    }
    
    // 从内存中读取最多 15 字节（x86 最大指令长度）
    std::vector<uint8_t> code_buffer(15);
    size_t available = std::min<size_t>(
        15, 
        physical_memory_.size() - addr
    );
    
    for (size_t i = 0; i < available; i++) {
        code_buffer[i] = read_byte(addr + i);
    }
    
    // 使用 Capstone 反汇编
    try {
        auto stream = disassembler_->disassemble(
            code_buffer.data(),
            available,
            addr,
            1  // 只反汇编 1 条指令
        );
        
        if (stream->size() > 0) {
            auto insn = (*stream)[0];
            
            // ⭐ 存入缓存
            instruction_cache_[addr] = insn;
            
            return insn;
        }
    } catch (const std::exception& e) {
        std::cerr << "[X86VM-" << vm_id_ << "] Disassembly error: " 
                  << e.what() << std::endl;
    }
    
    return nullptr;
}
```

**向后兼容**:

```cpp
// 保留旧接口，内部切换到新实现
bool X86CPUVM::execute_instruction() {
    return execute_instruction_ir();
}
```

---

## 📅 开发计划

### **Phase 1: 基础设施（预计 2-3 天）**

#### **任务 1.1**: 扩展 IR 类
- [ ] 修改 `include/vm/disassembly/IR.h`
  - [ ] 添加 `ExecutionContext` 结构体
  - [ ] 添加 `ExecutorFunction` 类型定义
  - [ ] 在 `Instruction` 类中添加 `executor` 字段
  - [ ] 实现 `execute()` 和 `bind_executor()` 方法
- [ ] 编译测试

#### **任务 1.2**: 创建执行器注册表
- [ ] 创建 `include/vm/disassembly/x86/X86Executor.h`
- [ ] 创建 `src/vm/disassembly/x86/X86Executor.cpp`
- [ ] 实现单例模式和注册表逻辑
- [ ] 实现 `initialize_all_executors()` 框架

#### **任务 1.3**: 实现基础指令执行器
- [ ] `make_mov_executor()` - MOV 指令
- [ ] `make_add_executor()` - ADD 指令
- [ ] `make_sub_executor()` - SUB 指令
- [ ] `make_hlt_executor()` - HLT 指令
- [ ] 单元测试验证

#### **任务 1.4**: 集成到反汇编流程
- [ ] 修改 `create_instruction_from_capstone()`
- [ ] 添加执行器自动绑定逻辑
- [ ] 测试反汇编后的指令是否可执行

**验收标准**:
- ✅ 能够反汇编并执行简单的 MOV/ADD/SUB/HLT 序列
- ✅ 无内存泄漏
- ✅ 编译无警告

---

### **Phase 2: VM 集成（预计 1-2 天）**

#### **任务 2.1**: 修改 X86CPUVM 头文件
- [ ] 添加 `disassembler_` 成员
- [ ] 添加 `exec_context_` 成员
- [ ] 添加 `instruction_cache_` 成员
- [ ] 声明新方法

#### **任务 2.2**: 实现构造函数初始化
- [ ] 初始化 Capstone 反汇编器
- [ ] 初始化执行上下文
- [ ] 初始化执行器注册表（全局一次）

#### **任务 2.3**: 实现新的执行流程
- [ ] 实现 `fetch_instruction()` 方法
- [ ] 实现 `execute_instruction_ir()` 方法
- [ ] 添加指令缓存逻辑
- [ ] 添加错误处理

#### **任务 2.4**: 向后兼容
- [ ] 修改 `execute_instruction()` 调用新实现
- [ ] 保留旧的手写解码器作为 `execute_instruction_legacy()`
- [ ] 添加编译开关控制切换

**验收标准**:
- ✅ VM 能够使用新架构运行测试程序
- ✅ 寄存器状态正确更新
- ✅ 性能可接受（缓存命中率 > 90%）

---

### **Phase 3: 完善指令集（预计 3-5 天）**

#### **任务 3.1**: 数据传输指令
- [ ] MOV (各种变体)
- [ ] PUSH / POP
- [ ] LEA
- [ ] XCHG

#### **任务 3.2**: 算术运算指令
- [ ] ADD / SUB
- [ ] MUL / DIV
- [ ] INC / DEC
- [ ] NEG

#### **任务 3.3**: 逻辑运算指令
- [ ] AND / OR / XOR
- [ ] NOT
- [ ] TEST
- [ ] SHL / SHR

#### **任务 3.4**: 控制流指令
- [ ] JMP (无条件跳转)
- [ ] Jcc (条件跳转)
- [ ] CALL / RET
- [ ] LOOP

#### **任务 3.5**: 标志位和中断
- [ ] CMP
- [ ] 标志位更新逻辑
- [ ] INT n
- [ ] CLI / STI

**验收标准**:
- ✅ 所有常用指令都能正确执行
- ✅ 标志位更新正确
- ✅ 分支跳转准确

---

### **Phase 4: 高级功能（可选，预计 2-3 天）**

#### **任务 4.1**: 调试支持
- [ ] 断点机制
- [ ] 单步执行
- [ ] 指令追踪日志
- [ ] 寄存器快照

#### **任务 4.2**: 性能优化
- [ ] 基本块缓存（类似 QEMU TB）
- [ ] 热点指令优化
- [ ] 缓存失效策略

#### **任务 4.3**: 动态代码修改
- [ ] 运行时替换执行器
- [ ] 热补丁支持
- [ ] 指令插桩

**验收标准**:
- ✅ 支持 GDB 风格的调试
- ✅ 性能接近手写解码器的 80%
- ✅ 支持动态代码修改

---

## 🧪 测试策略

### **单元测试**

```cpp
// tests/test_x86_executor.cpp

TEST(X86Executor, MovRegisterImmediate) {
    // 创建测试 VM
    MockX86VM vm;
    
    // 创建 MOV RAX, 42 指令
    auto insn = create_test_instruction("mov", {"rax", "42"});
    
    // 执行
    ExecutionContext ctx{&vm, 0x1000};
    int length = insn->execute(ctx);
    
    // 验证
    EXPECT_EQ(length, 10);  // MOV RAX, imm64 的长度
    EXPECT_EQ(vm.get_register(X86Reg::RAX), 42);
}

TEST(X86Executor, AddRegisterRegister) {
    MockX86VM vm;
    vm.set_register(X86Reg::RAX, 10);
    vm.set_register(X86Reg::RBX, 20);
    
    auto insn = create_test_instruction("add", {"rax", "rbx"});
    
    ExecutionContext ctx{&vm, 0x1000};
    int length = insn->execute(ctx);
    
    EXPECT_GT(length, 0);
    EXPECT_EQ(vm.get_register(X86Reg::RAX), 30);
}
```

### **集成测试**

```cpp
// tests/test_x86vm_ir_integration.cpp

TEST(X86VMIntegration, SimpleProgram) {
    X86VMConfig config;
    config.memory_size = 1024 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0x8000;
    
    X86CPUVM vm(1, config);
    
    // 加载测试程序：RAX = 10 + 20 - 5
    std::vector<uint8_t> program = {
        0x48, 0xB8, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // MOV RAX, 10
        0x48, 0xBB, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // MOV RBX, 20
        0x48, 0x01, 0xD8,  // ADD RAX, RBX
        0x48, 0xB9, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // MOV RCX, 5
        0x48, 0x29, 0xC8,  // SUB RAX, RCX
        0xF4  // HLT
    };
    
    vm.load_binary(program, 0x1000);
    vm.start();
    vm.run_loop();
    
    EXPECT_EQ(vm.get_register(X86Reg::RAX), 25);
    EXPECT_EQ(vm.get_state(), X86VMState::HALTED);
}
```

### **性能测试**

```cpp
// tests/test_x86vm_performance.cpp

TEST(X86VMPerformance, InstructionCacheHitRate) {
    // 执行循环程序，测试缓存命中率
    // 预期：第二次迭代后缓存命中率 > 95%
}

TEST(X86VMPerformance, ExecutionSpeed) {
    // 对比新旧架构的执行速度
    // 预期：新架构速度 >= 旧架构的 50%
}
```

---

## ⚠️ 风险与挑战

### **1. 性能开销**

**问题**: Capstone 反汇编比手写解码器慢 10-50 倍

**缓解措施**:
- ✅ 指令缓存（预期命中率 > 90%）
- ✅ 基本块缓存（未来优化）
- ✅ 仅调试时启用详细日志

**监控指标**:
- 缓存命中率
- 平均指令执行时间
- 总体吞吐量（instructions/sec）

---

### **2. 内存管理**

**问题**: `std::shared_ptr` 和指令缓存可能增加内存占用

**缓解措施**:
- ✅ 设置缓存大小上限（如 10000 条指令）
- ✅ LRU 淘汰策略
- ✅ 定期清理未命中缓存

**监控指标**:
- 缓存占用内存
- 指令对象数量
- 内存泄漏检测（valgrind）

---

### **3. 循环依赖**

**问题**: `X86Executor` 需要访问 `X86CPUVM`，而 VM 又依赖执行器

**缓解措施**:
- ✅ 使用 `void*` 类型擦除
- ✅ 前向声明
- ✅ 清晰的接口边界

---

### **4. 指令覆盖不全**

**问题**: x86 指令集庞大，难以一次性实现所有指令

**缓解措施**:
- ✅ 优先实现常用指令（覆盖 90% 场景）
- ✅ 未知指令回退到旧解码器
- ✅ 渐进式迁移策略

---

## 📊 成功指标

### **功能性指标**
- [ ] 支持至少 50 条常用 x86 指令
- [ ] 能够运行现有的测试程序
- [ ] 标志位更新正确率 100%
- [ ] 分支跳转准确率 100%

### **性能指标**
- [ ] 指令缓存命中率 > 90%
- [ ] 执行速度 >= 旧架构的 **80%** （使用 variant + visitor）
- [ ] 内存占用增加 < 20%
- [ ] std::visit 调用开销 < 2 ns

### **质量指标**
- [ ] 单元测试覆盖率 > 80%
- [ ] 无内存泄漏（valgrind  clean）
- [ ] 编译无警告
- [ ] 代码审查通过

---

## 🔗 相关文档

- [Capstone 反汇编系统设计文档](./Capstone反汇编系统设计文档.md)
- [Capstone 反汇编系统改进总结](./Capstone反汇编系统改进总结.md)
- [调度系统编程手册](./调度系统编程手册.md)
- [彩色日志系统实现文档](./彩色日志系统实现文档.md)

---

## 📝 修订历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|----------|
| v1.0 | 2026-03-15 | AI Assistant | 初始版本，完成架构设计 |
| v1.1 | 2026-03-15 | AI Assistant | 添加访问者模式 + std::variant 优化方案 |

---

## ✅ 下一步行动

1. **立即开始 Phase 1**：扩展 IR 类和创建执行器注册表
2. **编写单元测试框架**：确保测试驱动开发
3. **每日进度同步**：记录遇到的问题和解决方案
4. **代码审查**：每个 Phase 完成后进行 review

---

**文档状态**: ✅ 设计完成，准备进入实施阶段

**负责人**: TBD

**预计完成时间**: 2-3 周（取决于指令集覆盖范围）
