# Phase 2: VM 集成与反汇编优化设计文档

## 📋 概述

Phase 2 的目标是将 Phase 1 实现的执行器系统集成到 X86CPUVM 中，并解决 x86 可变长度指令带来的反汇编挑战。

**总预计时间**: 3-5 天  
**开始日期**: ___  
**完成日期**: ___

---

## 🎯 Phase 2 架构演进

Phase 2 分为三个递进的子阶段，逐步完善反汇编和执行能力：

```
Phase 2.1 (基础) → Phase 2.2 (追踪) → Phase 2.3 (完整CFG)
    ↓                    ↓                    ↓
 线性执行          入口点标记          控制流图构建
 简单缓存          冲突检测            基本块分析
```

---

## 📋 Phase 2.1: 基础执行器逻辑

### 设计目标

实现控制流指令的执行器，支持基本的程序流程控制，但**暂不处理复杂的入口点追踪**。

**假设**: 
- 代码是线性执行的
- 跳转目标是已知的（通过测试手动指定）
- 不考虑指令重叠问题

### 核心组件

#### 1. 控制流执行器

```cpp
// JMP 执行器
struct JmpExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        
        // 提取跳转目标
        uint64_t target_addr = extract_jump_target(insn);
        
        // 更新 RIP
        vm->set_register(X86Reg::RIP, target_addr);
        
        return insn.size;
    }
};

// CALL 执行器
struct CallExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        
        // 压入返回地址
        uint64_t ret_addr = vm->get_register(X86Reg::RIP) + insn.size;
        push_to_stack(vm, ret_addr);
        
        // 跳转到目标
        uint64_t target_addr = extract_call_target(insn);
        vm->set_register(X86Reg::RIP, target_addr);
        
        return insn.size;
    }
};

// RET 执行器
struct RetExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        
        // 弹出返回地址
        uint64_t ret_addr = pop_from_stack(vm);
        
        // 跳转到返回地址
        vm->set_register(X86Reg::RIP, ret_addr);
        
        return insn.size;
    }
};
```

#### 2. 条件跳转执行器

```cpp
// 条件跳转基类
struct ConditionalJmpExecutor {
    virtual bool check_condition(uint64_t flags) const = 0;
    
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        uint64_t flags = vm->get_rflags();
        
        if (check_condition(flags)) {
            // 条件满足，跳转
            uint64_t target = extract_jump_target(insn);
            vm->set_register(X86Reg::RIP, target);
        } else {
            // 条件不满足，顺序执行
            vm->set_register(X86Reg::RIP, 
                vm->get_register(X86Reg::RIP) + insn.size);
        }
        
        return insn.size;
    }
};

// JE (Jump if Equal)
struct JeExecutor : public ConditionalJmpExecutor {
    bool check_condition(uint64_t flags) const override {
        return (flags & FLAG_ZF) != 0;
    }
};

// JNE (Jump if Not Equal)
struct JneExecutor : public ConditionalJmpExecutor {
    bool check_condition(uint64_t flags) const override {
        return (flags & FLAG_ZF) == 0;
    }
};
```

### 执行流程

```
X86CPUVM::execute_instruction_ir()
    ↓
fetch_instruction(RIP)
    ↓
检查缓存
    ├─ 命中 → 返回缓存的指令
    └─ 未命中 → Capstone 反汇编 → 存入缓存
    ↓
insn->execute(exec_context_)
    ↓
执行器更新 RIP
    ↓
返回指令长度
```

### 局限性

❌ **问题 1**: 无法处理间接跳转（JMP RAX）
- 跳转目标在运行时才能确定
- 静态反汇编无法预知所有入口点

❌ **问题 2**: 可能导致指令重叠
```
地址:  0x1000    0x1001    0x1002    0x1003
字节:  [ EB 02 ] [ 90      ] [ 90      ]
       JMP +2     NOP        NOP

如果从 0x1000 开始反汇编: JMP 0x1004, NOP, NOP
如果从 0x1001 开始反汇编: NOP, NOP, ...
→ 同一地址被解释为不同指令！
```

❌ **问题 3**: 重复反汇编
- 每次执行到同一地址都要重新反汇编
- 性能开销大

---

## 📋 Phase 2.2: 入口点追踪机制

### 设计目标

使用 `vector<uint8_t>` 追踪哪些地址已被作为入口点反汇编过，解决 Phase 2.1 的局限性问题。

### 核心数据结构

```cpp
class DisassemblyTracker {
private:
    enum State : uint8_t {
        UNPROCESSED = 0,   // 未处理
        ENTRY_POINT = 1,   // 显式指定的入口点
        JUMP_TARGET = 2,   // 跳转发现的目标
        PROCESSED = 3      // 已反汇编
    };
    
    std::vector<uint8_t> state_;  // 每个字节的状态
    size_t code_size_;
    
public:
    DisassemblyTracker(size_t code_size) 
        : state_(code_size, UNPROCESSED), code_size_(code_size) {}
    
    // 检查是否是入口点
    bool is_entry_point(uint64_t offset) const {
        return offset < code_size_ && state_[offset] >= ENTRY_POINT;
    }
    
    // 标记为入口点
    void mark_as_entry(uint64_t offset) {
        if (offset < code_size_) {
            state_[offset] = ENTRY_POINT;
        }
    }
    
    // 标记为跳转目标
    void mark_as_jump_target(uint64_t offset) {
        if (offset < code_size_) {
            state_[offset] = JUMP_TARGET;
        }
    }
    
    // 标记为已处理
    void mark_as_processed(uint64_t offset) {
        if (offset < code_size_) {
            state_[offset] = PROCESSED;
        }
    }
    
    // 检测冲突：同一地址被不同方式反汇编
    bool has_conflict(uint64_t offset, uint16_t instruction_size) const {
        for (uint16_t i = 1; i < instruction_size; i++) {
            if (offset + i < code_size_ && state_[offset + i] == PROCESSED) {
                return true;  // 指令内部有已处理的字节
            }
        }
        return false;
    }
};
```

### 工作流程

```
1. 初始化 Tracker
   ↓
2. 标记初始入口点（如 0x1000）
   ↓
3. 反汇编入口点
   ├─ 检查是否已处理
   ├─ 调用 Capstone 反汇编
   ├─ 检测冲突
   └─ 标记为 PROCESSED
   ↓
4. 遇到跳转指令
   ├─ 提取跳转目标地址
   ├─ 标记为 JUMP_TARGET
   └─ 加入待处理队列
   ↓
5. 处理下一个入口点
   ↓
6. 重复直到队列为空
```

### 集成到执行器

```cpp
struct JmpExecutor {
    int execute(ExecutionContext& ctx, const Instruction& insn) const {
        auto* vm = static_cast<X86CPUVM*>(ctx.vm_instance);
        auto* tracker = vm->get_disassembly_tracker();
        
        // 提取跳转目标
        uint64_t target_offset = calculate_target_offset(insn);
        
        // 标记为新入口点
        if (tracker) {
            tracker->mark_as_jump_target(target_offset);
        }
        
        // 更新 RIP
        vm->set_register(X86Reg::RIP, target_offset);
        
        return insn.size;
    }
};
```

### 优势

✅ **解决重复反汇编**: 每个地址只反汇编一次  
✅ **检测指令重叠**: 发现潜在的解码冲突  
✅ **自动发现入口点**: 跳转指令自动标记新目标  
✅ **低内存开销**: 每字节只需 2 bits（实际用 1 byte 简化实现）

### 局限性

⚠️ **仍然无法处理间接跳转**: `JMP RAX` 的目标在编译时未知  
⚠️ **无法构建完整的控制流图**: 只知道入口点，不知道基本块边界  
⚠️ **不支持代码优化**: 没有结构化的 CFG 信息

---

## 📋 Phase 2.3: 完整反汇编图

### 设计目标

构建完整的控制流图（Control Flow Graph, CFG），支持：
- 基本块（Basic Block）划分
- 控制流边（Edges）
- 递归反汇编所有可达代码
- 为后续优化提供基础

### 核心数据结构

```cpp
struct BasicBlock {
    uint64_t start_addr;
    uint64_t end_addr;
    
    // 指令列表
    std::vector<std::shared_ptr<InstructionWithExecutor>> instructions;
    
    // 控制流关系
    std::vector<uint64_t> successors;    // 后继块起始地址
    std::vector<uint64_t> predecessors;  // 前驱块起始地址
    
    // 属性
    bool is_terminated;   // 是否以 JMP/RET/HLT 结束
    bool is_entry_block;  // 是否是函数入口
    bool has_indirect_jmp;  // 是否包含间接跳转
    
    // 获取最后一条指令
    const InstructionWithExecutor* last_instruction() const {
        return instructions.empty() ? nullptr : instructions.back().get();
    }
};

class ControlFlowGraph {
private:
    std::unordered_map<uint64_t, BasicBlock> blocks_;
    std::set<uint64_t> pending_entries_;  // BFS 队列
    DisassemblyTracker tracker_;
    
public:
    // 构建 CFG
    void build(const uint8_t* code, uint64_t entry_addr, size_t code_size);
    
    // 查询
    const BasicBlock* get_block(uint64_t addr) const;
    bool has_block(uint64_t addr) const;
    const std::unordered_map<uint64_t, BasicBlock>& get_all_blocks() const;
    
    // 遍历
    void for_each_block(std::function<void(const BasicBlock&)> callback) const;
    
private:
    // BFS 反汇编
    void process_pending_entries(const uint8_t* code, size_t code_size);
    
    // 创建基本块
    BasicBlock create_basic_block(const uint8_t* code, 
                                  uint64_t start_addr, 
                                  size_t code_size);
    
    // 添加控制流边
    void add_edge(uint64_t from, uint64_t to);
};
```

### CFG 构建算法

```
Algorithm: BuildCFG
Input: code[], entry_addr, code_size
Output: ControlFlowGraph

1. Initialize CFG with empty blocks
2. Add entry_addr to pending_entries
3. Mark entry_addr as ENTRY_POINT in tracker
4. 
5. While pending_entries is not empty:
6.     addr = pop from pending_entries
7.     
8.     If addr already has a block:
9.         Continue
10.    
11.    block = CreateBasicBlock(code, addr, code_size)
12.    Add block to CFG
13.    
14.    For each instruction in block:
15.        If instruction is JMP/CALL:
16.            target = ExtractTarget(instruction)
17.            Add target to pending_entries
18.            Mark target as JUMP_TARGET
19.            Add edge (block.start → target)
20.        
21.        If instruction is conditional JMP:
22.            target = ExtractTarget(instruction)
23.            fallthrough = instruction.addr + instruction.size
24.            Add both to pending_entries
25.            Add edges (block.start → target) and (block.start → fallthrough)
26.        
27.        If instruction is RET/HLT:
28.            Mark block as terminated
29.            Break
30.    
31. Return CFG
```

### 基本块划分规则

**基本块定义**: 
- 单一入口（第一条指令）
- 单一出口（最后一条指令）
- 中间无跳转目标
- 中间无跳转指令

**划分点**:
1. 函数入口
2. 跳转目标地址
3. 跳转指令的下一条指令（fall-through）
4. RET/HLT 指令后

**示例**:
```asm
0x1000: MOV RAX, 42      ← 基本块 1 开始（入口）
0x1007: ADD RAX, RBX
0x100A: CMP RAX, 100
0x100E: JL 0x1020        ← 条件跳转

0x1010: MOV RBX, 0       ← 基本块 2 开始（fall-through）
0x1017: HLT              ← 基本块 2 结束

0x1020: SUB RAX, 10      ← 基本块 3 开始（跳转目标）
0x1024: JMP 0x1000       ← 基本块 3 结束（无条件跳转）
```

### 集成到 VM

```cpp
class X86CPUVM {
private:
    ControlFlowGraph cfg_;
    uint64_t current_block_addr_;
    size_t current_instruction_idx_;
    
public:
    // 加载代码时构建 CFG
    void load_code(const uint8_t* code, size_t size, uint64_t entry_addr) {
        cfg_.build(code, entry_addr, size);
        current_block_addr_ = entry_addr;
        current_instruction_idx_ = 0;
    }
    
    // 执行下一条指令（从 CFG 获取）
    void execute_next() {
        const auto* block = cfg_.get_block(current_block_addr_);
        if (!block || current_instruction_idx_ >= block->instructions.size()) {
            stop();
            return;
        }
        
        const auto& insn = block->instructions[current_instruction_idx_];
        insn->execute(exec_context_);
        
        current_instruction_idx_++;
        
        // 如果到达块末尾，根据控制流选择下一块
        if (current_instruction_idx_ >= block->instructions.size()) {
            transition_to_next_block(block);
        }
    }
    
private:
    void transition_to_next_block(const BasicBlock* current_block) {
        if (current_block->is_terminated) {
            // JMP/RET/HLT，RIP 已被执行器更新
            uint64_t next_addr = exec_context_.vm_instance->get_rip();
            current_block_addr_ = next_addr;
            current_instruction_idx_ = 0;
        } else {
            // Fall-through 到下一个块
            // TODO: 从 successors 中选择
        }
    }
};
```

### 优势

✅ **完整的控制流信息**: 知道所有可能的执行路径  
✅ **支持优化**: 可以进行死代码消除、常量传播等  
✅ **高效执行**: 无需重复反汇编，直接从 CFG 获取指令  
✅ **支持分析**: 可以计算圈复杂度、检测不可达代码等  

### 局限性

⚠️ **间接跳转仍需动态处理**: `JMP RAX` 的目标只能在运行时确定  
⚠️ **自修改代码不支持**: 如果代码在运行时被修改，CFG 需要重建  
⚠️ **构建开销**: 首次加载代码时需要完整的 CFG 构建

---

## 📊 三阶段对比

| 特性 | Phase 2.1 | Phase 2.2 | Phase 2.3 |
|------|-----------|-----------|-----------|
| **执行器实现** | ✅ | ✅ | ✅ |
| **入口点追踪** | ❌ | ✅ vector<bool> | ✅ CFG |
| **指令缓存** | ✅ 简单 | ✅ 带状态 | ✅ 结构化 |
| **冲突检测** | ❌ | ✅ | ✅ |
| **控制流图** | ❌ | ❌ | ✅ |
| **基本块分析** | ❌ | ❌ | ✅ |
| **间接跳转** | ⚠️ 运行时 | ⚠️ 运行时 | ⚠️ 运行时 |
| **性能** | 中等 | 较好 | 最优 |
| **实现复杂度** | 低 | 中 | 高 |
| **预计时间** | 1-2 天 | 1 天 | 1-2 天 |

---

## 🚀 实施建议

### 推荐路线

1. **先完成 Phase 2.1**
   - 验证执行器架构可行性
   - 确保基本功能正常工作
   - 建立测试基础设施

2. **再实现 Phase 2.2**
   - 解决最紧迫的重复反汇编问题
   - 检测指令重叠
   - 为 Phase 2.3 打下基础

3. **最后实现 Phase 2.3**
   - 构建完整的 CFG
   - 优化执行性能
   - 为未来扩展做准备

### 可选跳过

如果时间紧张，可以：
- ✅ **必须**: Phase 2.1（基础功能）
- ✅ **强烈推荐**: Phase 2.2（解决核心问题）
- ⏸️ **可延后**: Phase 2.3（可以等到需要优化时再做）

---

## 📝 相关文件

### Phase 2.1
- `include/vm/disassembly/x86/X86Executors.h` - 执行器定义
- `src/vm/disassembly/x86/X86ExecutorRegistry.cpp` - 注册表
- `include/vm/X86CPU.h` - VM 头文件
- `src/vm/X86CPU.cpp` - VM 实现

### Phase 2.2
- `include/vm/disassembly/DisassemblyTracker.h` - 追踪器（新建）
- `src/vm/disassembly/DisassemblyTracker.cpp` - 追踪器实现（新建）
- `tests/test_disassembly_tracker.cpp` - 单元测试（新建）

### Phase 2.3
- `include/vm/disassembly/ControlFlowGraph.h` - CFG（新建）
- `src/vm/disassembly/ControlFlowGraph.cpp` - CFG 实现（新建）
- `tests/test_control_flow_graph.cpp` - 单元测试（新建）

---

## 🔗 参考资料

- [Intel® 64 and IA-32 Architectures Software Developer's Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [Capstone Engine Documentation](https://www.capstone-engine.org/)
- [Compiler Design: Control Flow Graphs](https://en.wikipedia.org/wiki/Control_flow_graph)

---

**作者**: AI Assistant  
**审核人**: Me  
**审核日期**: 2026年4月5日  
**版本**: 1.0
