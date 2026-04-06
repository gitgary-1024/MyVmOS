# Lab vs 主代码 - 文件对照表

**目的**: 清晰展示 Lab 目录中的增强代码如何映射到主代码

---

## 📁 文件结构对比

### Lab 目录（原型）
```
Lab/Phase2.3_CFG/
├── CallGraph.h                          # ✅ 新增：CallGraph 数据结构
├── EnhancedCapstoneDisassembler.h       # ✅ 新增：is_call() 辅助函数
├── test_call_ret_enhancement.cpp        # ✅ 演示程序
├── Implementation_Report_Call_Ret_Enhancement.md  # ✅ 实施报告
└── FILE_MAPPING.md                      # 📄 本文件
```

### 主代码目录（待修改）
```
src/vm/disassembly/
├── ControlFlowGraph.h                   # ⚠️ 需要修改：添加 CallGraph 成员
├── ControlFlowGraph.cpp                 # ⚠️ 需要修改：添加 CALL 处理逻辑
├── CapstoneDisassembler.h               # ⚠️ 需要修改：添加 is_call() 声明
├── CapstoneDisassembler.cpp             # ⚠️ 需要修改：实现 is_call()
├── CallGraph.h                          # 🆕 需要从 Lab 复制
├── BasicBlock.h                         # ✅ 无需修改
├── DisassemblyTracker.h                 # ✅ 无需修改
└── ...
```

---

## 🔄 代码映射关系

### 1. CallGraph 数据结构

| Lab 文件 | 主代码位置 | 操作 |
|---------|-----------|------|
| `Lab/Phase2.3_CFG/CallGraph.h` | `src/vm/disassembly/CallGraph.h` | **复制** |

**说明**: 
- 直接复制文件，无需修改
- 命名空间已经是 `disassembly::`，与主代码一致

---

### 2. is_call() 辅助函数

| Lab 文件 | 主代码位置 | 操作 |
|---------|-----------|------|
| `Lab/Phase2.3_CFG/EnhancedCapstoneDisassembler.h` | `src/vm/disassembly/CapstoneDisassembler.h` + `.cpp` | **合并** |

**Lab 实现**:
```cpp
// EnhancedCapstoneDisassembler.h
class EnhancedCapstoneDisassembler : public CapstoneDisassembler {
public:
    static bool is_call(const SimpleInstruction* insn) {
        return (mnem == "call" || mnem == "callq");
    }
};
```

**主代码实现**（推荐）:
```cpp
// CapstoneDisassembler.h
class CapstoneDisassembler {
public:
    // 添加静态方法
    static bool is_call(const SimpleInstruction* insn);
};

// CapstoneDisassembler.cpp
bool CapstoneDisassembler::is_call(const SimpleInstruction* insn) {
    if (!insn) return false;
    
    const std::string& mnem = insn->mnemonic;
    
    return (
        mnem == "call"   ||
        mnem == "callq"
    );
}
```

**说明**: 
- 不需要创建新的类，直接在现有类中添加静态方法
- 保持与 `is_unconditional_jump`、`is_conditional_jump` 等方法的风格一致

---

### 3. CALL 处理逻辑

| Lab 演示 | 主代码位置 | 操作 |
|---------|-----------|------|
| `test_call_ret_enhancement.cpp` 中的伪代码 | `ControlFlowGraph::create_basic_block` | **插入** |

**Lab 演示代码**（第 107-145 行）:
```cpp
// 模拟的 CALL 处理逻辑
if (is_call(insn)) {
    pending_edges[current_block_start].push_back(call_target);
    pending_entries.insert(call_target);
    
    pending_edges[current_block_start].push_back(fallthrough);
    pending_entries.insert(fallthrough);
    
    block.is_terminated = true;
}
```

**主代码修改位置**:
在 `src/vm/disassembly/ControlFlowGraph.cpp` 的 `create_basic_block` 方法中，
找到条件跳转处理之后、终止指令判断之前的位置（约 L234），插入：

```cpp
} else if (is_conditional_jump(insn.get())) {
    // ... 现有代码（L207-233）
    
} else if (CapstoneDisassembler::is_call(insn.get())) {  // ← 新增
    // ✅ CALL 有两个后继：调用目标 + Fall-through
    block.is_terminated = true;
    
    // 提取调用目标
    uint64_t call_target = extract_jump_target(insn.get(), current_addr);
    
    if (call_target < code_size_) {
        // 记录调用关系
        call_graph_.add_call(start_addr, call_target);
        
        // 标记为函数入口
        tracker_.mark_as_jump_target(call_target);
        pending_entries_.insert(call_target);
        pending_edges[start_addr].push_back(call_target);
        
        std::cout << "    [CALL] Call to 0x" 
                  << std::hex << call_target << std::dec << std::endl;
    }
    
    // Fall-through（返回地址）
    uint64_t fallthrough = current_addr + insn->size;
    if (fallthrough < code_size_) {
        tracker_.mark_as_jump_target(fallthrough);
        pending_entries_.insert(fallthrough);
        pending_edges[start_addr].push_back(fallthrough);
        
        std::cout << "    [FALLTHROUGH] Return address at 0x" 
                  << std::hex << fallthrough << std::dec << std::endl;
    }
    
    should_continue = false;
    
} else if (is_terminator(insn.get())) {
    // ... 现有代码（L235-243）
}
```

---

### 4. ControlFlowGraph 成员变量

| Lab 设计 | 主代码位置 | 操作 |
|---------|-----------|------|
| `EnhancedControlFlowGraph::call_graph_` | `ControlFlowGraph::call_graph_` | **添加** |

**主代码修改**:
在 `src/vm/disassembly/ControlFlowGraph.h` 的私有成员中添加：

```cpp
class ControlFlowGraph {
private:
    std::unordered_map<uint64_t, BasicBlock> blocks_;
    std::set<uint64_t> pending_entries_;
    DisassemblyTracker tracker_;
    const uint8_t* code_;
    size_t code_size_;
    CapstoneDisassembler disassembler_;
    
    CallGraph call_graph_;  // ← 新增：函数调用图
    
public:
    // ... 现有接口
    
    // 新增接口
    const CallGraph& get_call_graph() const { return call_graph_; }
    void print_call_graph() const { call_graph_.print(); }
};
```

---

## 📋 集成检查清单

### 阶段 1: 文件准备
- [ ] 复制 `Lab/Phase2.3_CFG/CallGraph.h` → `src/vm/disassembly/CallGraph.h`
- [ ] 确认 `CallGraph.h` 使用 `disassembly::` 命名空间
- [ ] 备份主代码文件（可选但推荐）

### 阶段 2: 头文件修改
- [ ] 修改 `ControlFlowGraph.h`：
  - [ ] 添加 `#include "CallGraph.h"`
  - [ ] 添加 `CallGraph call_graph_;` 成员变量
  - [ ] 添加 `get_call_graph()` 和 `print_call_graph()` 接口
- [ ] 修改 `CapstoneDisassembler.h`：
  - [ ] 添加 `static bool is_call(const SimpleInstruction* insn);` 声明

### 阶段 3: 源文件修改
- [ ] 修改 `CapstoneDisassembler.cpp`：
  - [ ] 实现 `is_call()` 方法
- [ ] 修改 `ControlFlowGraph.cpp`：
  - [ ] 在 `create_basic_block` 中添加 CALL 处理逻辑
  - [ ] 确保包含必要的头文件

### 阶段 4: 编译测试
- [ ] 重新编译项目：`cmake --build build`
- [ ] 修复编译错误（如果有）
- [ ] 运行现有测试：`./build/test_cfg_analysis.exe`

### 阶段 5: 功能验证
- [ ] 编写测试用例 1：简单函数调用
- [ ] 编写测试用例 2：递归函数
- [ ] 编写测试用例 3：多层调用
- [ ] 验证 Call Graph 输出正确

### 阶段 6: 清理
- [ ] 删除 Lab 目录中的临时文件（可选）
- [ ] 更新文档
- [ ] 提交 Git

---

## ⚡ 快速集成命令

```bash
# 1. 复制 CallGraph.h
cp Lab/Phase2.3_CFG/CallGraph.h src/vm/disassembly/CallGraph.h

# 2. 编辑 ControlFlowGraph.h（手动添加成员变量和接口）
# 3. 编辑 CapstoneDisassembler.h（手动添加 is_call 声明）
# 4. 编辑 CapstoneDisassembler.cpp（手动实现 is_call）
# 5. 编辑 ControlFlowGraph.cpp（手动添加 CALL 处理逻辑）

# 6. 重新编译
cd build
cmake ..
make -j8

# 7. 运行测试
./test_cfg_analysis.exe
```

---

## 🔍 验证要点

### 编译成功
```
[100%] Built target test_cfg_analysis
```

### 运行输出示例
```
=== Call Graph ===
Total functions: 2
Recursive functions: 0

Function at 0x0 calls: 0x10
Function at 0x10 (leaf function)
  Called by: 0x5
==================
```

### 关键指标
- ✅ 无编译错误
- ✅ 无运行时崩溃
- ✅ Call Graph 正确显示调用关系
- ✅ 递归函数被正确标记

---

## 📞 问题排查

### 问题 1: 编译错误 - "CallGraph 未定义"
**原因**: 忘记 `#include "CallGraph.h"`  
**解决**: 在 `ControlFlowGraph.h` 开头添加 `#include "CallGraph.h"`

### 问题 2: 链接错误 - "is_call 未定义"
**原因**: 忘记实现 `is_call()` 方法  
**解决**: 在 `CapstoneDisassembler.cpp` 中添加实现

### 问题 3: Call Graph 为空
**原因**: CALL 处理逻辑未被触发  
**解决**: 
1. 检查测试代码是否包含 `call` 指令
2. 添加调试输出确认 `is_call()` 返回 true
3. 检查 `call_target` 是否在有效范围内

---

## 🎯 总结

| 项目 | 状态 |
|-----|------|
| Lab 原型 | ✅ 完成并测试通过 |
| 文件映射 | ✅ 已明确 |
| 集成步骤 | ✅ 已文档化 |
| 测试计划 | ✅ 已制定 |
| 风险识别 | ✅ 已标注 |

**下一步**: 按照集成检查清单逐步将代码从 Lab 迁移到主代码。
