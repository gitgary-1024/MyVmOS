# ControlFlowGraph 集成到 X86CPU VM - 快速参考

## 🚀 快速开始

### 1. 构建 CFG

```cpp
X86CPUVM vm(vm_id, config);
vm.load_binary(code, entry_point);
vm.build_cfg(entry_point);  // 构建控制流图
```

### 2. 查询 CFG 信息

```cpp
// 获取基本块
const void* block = vm.get_basic_block_at(rip);

// 获取指令数量
size_t count = vm.get_block_instruction_count(addr);

// 获取后继地址
auto successors = vm.get_block_successors(addr);

// 检查是否是跳转目标
bool is_target = vm.is_jump_target(addr);

// 打印摘要
vm.print_cfg_summary();
```

### 3. 执行代码（自动使用 CFG）

```cpp
vm.start();
while (vm.get_x86_state() == X86VMState::RUNNING) {
    vm.execute_instruction();  // 自动优先使用基本块执行
}
vm.stop();
```

---

## 📚 API 参考

### CFG 构建

| 方法 | 说明 | 示例 |
|------|------|------|
| `build_cfg(entry_addr)` | 构建控制流图 | `vm.build_cfg(0x1000);` |
| `has_cfg()` | 检查是否有 CFG | `if (vm.has_cfg()) { ... }` |

### CFG 查询

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `get_basic_block_at(rip)` | `uint64_t rip` | `const void*` | 获取基本块指针 |
| `get_block_instruction_count(addr)` | `uint64_t addr` | `size_t` | 获取指令数量 |
| `get_block_successors(addr)` | `uint64_t addr` | `std::vector<uint64_t>` | 获取后继列表 |
| `is_jump_target(addr)` | `uint64_t addr` | `bool` | 检查是否是跳转目标 |
| `print_cfg_summary()` | 无 | void | 打印 CFG 摘要 |

### 执行控制

| 方法 | 说明 |
|------|------|
| `start()` | 启动 VM |
| `stop()` | 停止 VM |
| `execute_instruction()` | 执行一条指令或一个基本块 |
| `get_x86_state()` | 获取 X86 特定状态 |

---

## 🔧 内部实现

### execute_instruction() 流程

```
execute_instruction()
    ├─ 检查中断
    ├─ 尝试 execute_basic_block()
    │   ├─ 获取当前基本块
    │   ├─ 执行所有指令
    │   └─ 遇到终止指令则停止
    └─ 如果失败，回退到 decode_and_execute()
        └─ 单指令执行
```

### execute_basic_block() 逻辑

```cpp
int execute_basic_block() {
    const auto* block = get_current_basic_block();
    if (!block) return -1;  // 回退
    
    int executed = 0;
    for (const auto& insn : block->instructions) {
        if (get_rip() != insn->address) break;
        
        int len = decode_and_execute();
        if (len <= 0) return executed;
        
        set_rip(get_rip() + len);
        executed++;
        
        if (block->is_terminated && is_last_instruction) break;
    }
    
    return executed;
}
```

---

## ⚠️ 已知问题

### 问题 1: decode_and_execute() 需要修复

**症状**: 内存访问越界错误

**影响**: Test 2-4 无法通过

**临时方案**: 
- 等待 `src/vm/X86_decode.cpp` 修复
- 或使用其他已验证的测试代码

---

### 问题 2: 分支预测未实现

**当前状态**: 只有接口声明，没有实际实现

**影响**: 无性能优化

**计划**: 未来实现 BranchPredictor 类

---

## 📊 测试结果

| 测试 | 状态 | 说明 |
|------|------|------|
| Test 1: CFG 构建 | ✅ PASS | CFG 正确构建，查询接口正常 |
| Test 2: 基本块执行 | ⚠️ FAIL | decode_and_execute() 问题 |
| Test 3: 分支预测 | ⚠️ FAIL | decode_and_execute() 问题 |
| Test 4: 综合执行 | ⚠️ FAIL | decode_and_execute() 问题 |

---

## 🔗 相关文件

### 头文件
- `include/vm/X86CPU.h` - X86CPU VM 接口
- `include/vm/disassembly/ControlFlowGraph.h` - CFG 定义

### 实现文件
- `src/vm/X86CPU.cpp` - X86CPU VM 实现（包含 CFG 集成）
- `src/vm/disassembly/ControlFlowGraph.cpp` - CFG 实现
- `src/vm/disassembly/CapstoneDisassembler.cpp` - Capstone 反汇编器

### 测试文件
- `tests/test_x86vm_cfg_build.cpp` - CFG 构建测试
- `tests/test_x86vm_block_execution.cpp` - 基本块执行测试
- `tests/test_x86vm_branch_prediction.cpp` - 分支预测测试
- `tests/test_x86vm_full_integration.cpp` - 综合集成测试

### 文档
- `docs/ControlFlowGraph_X86CPU_Integration.md` - 详细编程文档
- `docs/CFG_VM_Integration_Summary.md` - 实施总结

---

## 💡 最佳实践

### 1. 始终检查 CFG 是否可用

```cpp
if (vm.has_cfg()) {
    // 使用 CFG 功能
} else {
    // 回退到传统方式
}
```

### 2. 使用 print_cfg_summary() 调试

```cpp
vm.build_cfg(entry_point);
vm.print_cfg_summary();  // 查看 CFG 结构
```

### 3. 验证基本块信息

```cpp
const void* block = vm.get_basic_block_at(rip);
if (block) {
    size_t count = vm.get_block_instruction_count(rip);
    auto succs = vm.get_block_successors(rip);
    std::cout << "Block has " << count << " instructions and " 
              << succs.size() << " successors" << std::endl;
}
```

---

## 🎯 下一步

1. **修复 decode_and_execute()** - 优先级最高
2. **运行所有测试** - 验证功能完整性
3. **实现分支预测器** - 提升性能
4. **添加性能分析** - 监控执行效率

---

**版本**: v1.0  
**最后更新**: 2026-04-05
