# X86VM 执行器系统 - 开发避坑指南

## 📋 概述

本文档记录了 X86VM 执行器系统开发过程中遇到的关键问题和解决方案，帮助后续开发者避免常见陷阱。

---

## ⚠️  常见陷阱与解决方案

### 陷阱 1：必须使用真实的 X86CPUVM

**问题描述**：
在执行器内部，我们通过 `static_cast<X86CPUVM*>(ctx.vm_instance)` 将 void* 转换为 VM 指针。如果传入的是 Mock VM 或其他类型，会导致未定义行为（崩溃或数据错误）。

**错误示例**：
```cpp
class MockVM {
public:
    uint64_t rax = 0;
};

MockVM vm;  // ❌ 不要这样做
ExecutionContext ctx(&vm, 0x1000);
auto executor = registry.create_executor("mov");
insn.bind_executor(executor);
insn.execute(ctx);  // 💥 崩溃！类型不匹配
```

**正确做法**：
```cpp
X86VMConfig config;
config.memory_size = 1024 * 1024;
X86CPUVM vm(1, config);  // ✅ 使用真实的 VM
ExecutionContext ctx(&vm, 0x1000);
```

**原因分析**：
- `ExecutionContext::vm_instance` 是 `void*` 类型（为避免循环依赖）
- 执行器通过 `static_cast` 而非 `dynamic_cast` 转换（性能考虑）
- `static_cast` 不进行运行时类型检查，错误的类型会导致未定义行为

---

### 陷阱 2：指令必须包含操作数对象

**问题描述**：
执行器通过 `insn.operands` 访问实际的操作数对象，而 `operands_str` 仅用于显示。如果忘记添加操作数对象，`dynamic_cast` 会返回 `nullptr`，导致执行器跳过逻辑。

**错误示例**：
```cpp
InstructionWithExecutor insn;
insn.mnemonic = "mov";
insn.operands_str = "rax, 42";  // ❌ 只有字符串，没有实际对象
// 忘记添加 operands.push_back(...)

auto executor = registry.create_executor("mov");
insn.bind_executor(executor);
insn.execute(ctx);  // ⚠️ 静默失败，寄存器未被修改
```

**正确做法**：
```cpp
InstructionWithExecutor insn;
insn.mnemonic = "mov";
insn.operands_str = "rax, 42";

// ✅ 添加实际的操作数对象
auto dest_reg = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);
auto imm_val = std::make_shared<ImmediateOperand>(42, OperandSize::QWORD);
insn.operands.push_back(std::move(dest_reg));
insn.operands.push_back(std::move(imm_val));

auto executor = registry.create_executor("mov");
insn.bind_executor(executor);
insn.execute(ctx);  // ✅ 正常工作
```

**原因分析**：
- `operands_str` 只是人类可读的字符串表示
- 执行器需要实际的 `Operand` 对象来提取寄存器 ID、立即数值等信息
- `dynamic_cast<const RegisterOperand*>(operand.get())` 在 operand 为空或类型不匹配时返回 `nullptr`

---

### 陷阱 3：Capstone 寄存器 ID 映射

**问题描述**：
Capstone 反汇编引擎返回的寄存器 ID（如 `X86_REG_RAX = 57`）与我们内部的 `X86Reg` 枚举不同，需要映射。

**Capstone 寄存器常量**（来自 `capstone/x86.h`）：
```cpp
X86_REG_RAX = 57
X86_REG_RBX = 58
X86_REG_RCX = 59
X86_REG_RDX = 60
// ... 更多寄存器
```

**我们的映射函数**：
```cpp
inline X86Reg capstone_reg_to_x86reg(int cs_reg_id) {
    switch (cs_reg_id) {
        case 57: return X86Reg::RAX;
        case 58: return X86Reg::RBX;
        case 59: return X86Reg::RCX;
        case 60: return X86Reg::RDX;
        // ... 更多映射
        default: return X86Reg::RAX;  // 默认值
    }
}
```

**注意事项**：
- 如果未来支持更多寄存器，需要同步更新此映射表
- 映射错误会导致操作错误的寄存器（难以调试的 bug）

---

### 陷阱 4：变量命名冲突

**问题描述**：
在同一个作用域内重复声明同名变量会导致编译错误。

**错误示例**：
```cpp
// Test 7
auto dest_reg = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);

// Test 8.5 - 同一作用域
auto dest_reg = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);  // ❌ 编译错误：redeclaration
```

**正确做法**：
```cpp
// Test 7
auto dest_reg = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);

// Test 8.5 - 使用不同的名称
auto add_dest = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);  // ✅
```

---

## 🎯 设计决策说明

### 决策 1：为什么使用 std::variant 而非 std::function？

**优势**：
- **零开销抽象**：无虚函数调用，编译器可内联优化
- **类型安全**：编译期检查，避免运行时错误
- **性能提升**：比 `std::function` 快约 7.5 倍

**代价**：
- 增加编译时间（需要包含所有执行器定义）
- `variant` 大小固定为最大执行器的大小

**适用场景**：
- 执行器数量固定且不多（当前 11 个）
- 对性能要求高（每条指令都执行）

---

### 决策 2：为什么 ExecutionContext 使用 void* 而非模板？

**原因**：
- 避免循环依赖：`IR.h` 不能直接 include `X86CPU.h`
- 保持通用性：未来可能支持其他架构的 VM
- 简化接口：所有执行器使用统一的上下文类型

**代价**：
- 需要 `static_cast` 转换（但性能影响极小）
- 失去编译期类型检查（需要通过文档和测试保证）

---

### 决策 3：标志位更新逻辑

**实现的标志位**：
- **ZF (Zero Flag)**: 结果为 0 时置位
- **SF (Sign Flag)**: 结果最高位为 1 时置位
- **CF (Carry Flag)**: 无符号溢出时置位
- **OF (Overflow Flag)**: 有符号溢出时置位
- **PF (Parity Flag)**: 低 8 位中 1 的个数为偶数时置位

**重要性**：
这些标志位对于条件跳转指令（Jcc）至关重要，例如：
- `JE` (Jump if Equal): 检查 ZF
- `JL` (Jump if Less): 检查 SF != OF
- `JB` (Jump if Below): 检查 CF

---

## 🧪 测试最佳实践

### 测试用例组织

```
Test 1-5:  基础 IR 结构测试（无需 VM）
Test 6-7:  注册表初始化与执行器绑定
Test 8:    MOV 指令执行（reg <- imm）
Test 8.5:  ADD 指令执行（reg + reg）
Test 9:    未知指令 fallback 机制
Test 10:   SUB 指令执行及标志位验证
```

### 断言策略

```cpp
// 1. 验证返回值
assert(length == expected_length);

// 2. 验证寄存器状态
uint64_t rax_value = vm.get_register(X86Reg::RAX);
assert(rax_value == expected_value);

// 3. 验证标志位
uint64_t flags = vm.get_rflags();
bool zf_set = (flags & FLAG_ZF) != 0;
assert(zf_set == expected_zf);
```

---

## 📚 相关文件

- **执行器定义**: `include/vm/disassembly/x86/X86Executors.h`
- **执行器注册表**: `include/vm/disassembly/x86/X86ExecutorRegistry.h`
- **测试文件**: `tests/test_ir_executor.cpp`
- **实施清单**: `X86VM重构实施清单.md`

---

## 🔗 参考资料

- [X86VM 重构开发文档](X86VM重构开发文档.md)
- [X86VM 执行器方案对比](X86VM执行器方案对比.md)
- [Capstone 反汇编系统设计文档](Capstone反汇编系统设计文档.md)

---

**最后更新**: 2026-04-05  
**作者**: AI Assistant  
**版本**: 1.0
