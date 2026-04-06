# X86VM 指令解码器 Bug 修复总结

## 用户报告的问题
```
[X86VM-180113898704] Unknown arithmetic opcode: 0x0
```

## 根本原因分析

### 问题 1：算术运算指令实现不完整
**位置**: `src/vm/X86_instructions.cpp::execute_arithmetic()`

**症状**: 只实现了 0x01 和 0x03（64 位 ADD/SUB），缺少对 0x00、0x02 等 8 位指令的支持

**x86 规范**:
- 0x00: ADD r/m8, r8
- 0x01: ADD r/m64, r64 ✅
- 0x02: ADD r8, r/m8  
- 0x03: ADD r64, r/m64 ✅
- 0x28: SUB r/m8, r8
- 0x29: SUB r/m64, r64 ✅
- 0x2A: SUB r8, r/m8
- 0x2B: SUB r64, r/m64

**修复**: 添加了所有缺失的 8 位和 64 位 ADD/SUB 指令实现

---

### 问题 2：MOV 指令缺少 8 位支持
**位置**: `src/vm/X86_instructions.cpp::execute_mov()`

**症状**: 只实现了 0xB8-0xBF (MOV r64, imm64)，缺少 0xB0-0xB7 (MOV r8, imm8)

**修复**: 添加了 0xB0-0xB7 指令的实现

---

### 问题 3：REX 前缀字节数未累加 ⚠️ **关键 Bug**
**位置**: `src/vm/X86_decode.cpp::decode_and_execute()`

**症状**: 
```cpp
// 错误代码（旧）
return execute_mov(rip);  // 只返回指令本身长度，未包含 REX 前缀
```

当指令有 REX 前缀（0x48）时：
1. 解码器读取 REX 前缀，`rip++`，`prefix_bytes = 1`
2. 调用 `execute_mov(rip)` 返回 9（1 字节操作码 + 8 字节立即数）
3. **直接返回 9**，但实际指令长度应该是 **10**（1 REX + 9）
4. RIP 更新错误：`set_rip(get_rip() + 9)` 而不是 `+10`
5. 下一条指令从错误的地址开始执行，导致解码出完全错误的"操作码"

**影响范围**: 所有带 REX 前缀的指令（64 位操作都需要 REX.W 前缀 0x48）

**修复**:
```cpp
// 正确代码（新）
return prefix_bytes + execute_mov(rip);  // ✅ 加上前缀字节数
```

此修复应用到所有 `execute_*` 函数调用：
- `execute_mov()`
- `execute_push_pop()`
- `execute_arithmetic()`
- `execute_logical()`
- `execute_branch()`
- `execute_call_ret()`
- `execute_stack_ops()`
- `execute_flag_ops()`
- `execute_interrupt()`
- `execute_string_ops()`

---

## 当前状态

虽然上述修复已经提交，但测试仍然失败。经过调试发现：

### 遗留问题
测试程序中的内存访问地址异常：`0xbb00000005000000`

**可能原因**:
1. 指令编码或加载逻辑仍有问题
2. ModR/M 解码逻辑可能存在 bug
3. 立即数读取可能有字节序问题

### 下一步建议
1. 使用更简单的测试程序（不使用 REX 前缀）
2. 添加详细的调试输出，打印每条指令的解码过程
3. 验证 `load_program()` 的字节序转换是否正确
4. 检查 `read_qword()` 等内存读取函数的对齐和字节序处理

---

## 文件修改清单

### 修改的文件
1. `src/vm/X86_instructions.cpp` - 添加 8 位和 64 位 ADD/SUB 指令实现
2. `src/vm/X86_decode.cpp` - 修复所有 `execute_*` 调用的前缀字节累加

### 新增的文件
- `tests/test_vm_arithmetic.cpp` - 算术运算测试程序
- `CMakeLists.txt` - 添加新的测试目标

---

## 技术要点

### x86 指令格式
```
[REX 前缀] [Opcode] [ModR/M] [SIB] [Displacement] [Immediate]
   1 字节    可变      可变     可变       可变          可变
```

### REX 前缀（0x40-0x4F）
- Bit 7-4: 固定为 0100
- Bit 3 (W): 0=32 位，1=64 位操作
- Bit 2 (R): 扩展 ModR/M 的 reg 字段
- Bit 1 (X): 扩展 SIB 的 index 字段
- Bit 0 (B): 扩展 ModR/M 的 r/m 字段或 SIB 的 base 字段

### 指令长度计算
**总长度 = REX 前缀 (如果有) + 主操作码 + ModR/M + SIB (如果有) + 位移 + 立即数**

---

**生成时间**: 2026-03-15
**修复者**: AI Assistant
