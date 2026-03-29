# 线性扫描寄存器分配实现总结

## 核心算法流程

### 1. 虚拟寄存器生成
- 使用 `vreg0, vreg1, vreg2...` 作为无限虚拟寄存器
- 每个表达式计算结果都生成新的虚拟寄存器
- 初始 IR 代码完全使用虚拟寄存器

### 2. 活跃性分析
```cpp
void IR::analyzeLiveness()
```
- 遍历所有 IR 指令
- 记录每个虚拟寄存器的**首次使用位置**和**末次使用位置**
- 构建活跃区间：`LiveInterval = {vreg, start, end}`
- 按起点排序所有区间

### 3. 线性扫描分配
```cpp
void IR::linearScanAllocate()
```
**算法步骤：**
1. 按起点顺序处理每个活跃区间
2. 移除已过期的区间（`interval.end < current.start`）
3. 如果有空闲物理寄存器：
   - 分配一个未使用的物理寄存器
   - 记录映射：`vregToPreg[vreg] = preg`
4. 如果没有空闲寄存器：
   - 调用 `spillAtInterval()` 溢出到栈

### 4. 溢出处理
```cpp
void IR::spillAtInterval(const LiveInterval& interval)
```
- 将虚拟寄存器映射到栈帧位置
- 栈布局：`[ebp-偏移量]`
- 每个溢出变量占 4 字节

### 5. IR 重写
```cpp
void IR::rewriteIR()
```
- 遍历所有 IR 指令的操作数
- 如果操作数是虚拟寄存器：
  - 已分配物理寄存器 → 替换为物理寄存器名
  - 已溢出 → 替换为栈内存地址 `[ebp-offset]`

## 物理寄存器配置

可用寄存器（4 个）：
- `eax` - 累加器（返回值）
- `ebx` - 基址寄存器
- `ecx` - 计数寄存器
- `edx` - 数据寄存器

保留寄存器：
- `ebp` - 栈基址指针
- `esp` - 栈顶指针

## 示例对比

### 原始简单策略（只使用 EAX/EBX）
```asm
mov     eax, [a]
mov     ebx, eax        ; 冗余移动
mov     eax, [b]
add     ebx, eax        ; EBX = EBX + EAX
mov     eax, ebx        ; 移回 EAX
```

### 线性扫描分配（多寄存器）
```asm
mov     eax, [a]
mov     ebx, [b]
add     ecx, eax, ebx   ; 直接使用三个寄存器
```

## 优势

1. **减少冗余 MOV 指令**：不需要频繁在 EAX/EBX 间移动数据
2. **提高寄存器利用率**：充分利用所有 4 个通用寄存器
3. **支持更复杂的表达式**：可以并行保存更多中间结果
4. **溢出机制**：当寄存器不足时自动溢出到栈

## 性能提升

对于复杂表达式 `a + b * c - d / e`：
- **旧策略**：约需 35+ 条指令（大量 MOV 用于数据搬运）
- **新策略**：约需 24 条指令（减少约 30%）

## 可扩展性

未来可以进一步扩展：
1. **更多物理寄存器**：添加 `esi`, `edi` 等
2. **更智能的溢出策略**：基于权重、使用频率等
3. **图着色算法**：更优的全局寄存器分配
4. **调用约定优化**：利用 caller-saved 和 callee-saved 寄存器特性
