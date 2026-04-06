# IRToAssembly 快速参考

## 🚀 快速开始

```cpp
#include "IR/IRToAssembly.h"
#include <vector>
#include <iostream>

int main() {
    // 创建 IR 指令
    std::vector<IRNode> ir;
    ir.push_back(IRNode(IROp::MOV, {"eax", "5"}, ""));
    ir.push_back(IRNode(IROp::ADD, {"eax", "3"}, ""));
    
    // 转换为汇编
    IRToAssembly assembler(ir);
    std::string asm_code = assembler.compile();
    
    std::cout << asm_code << std::endl;
    return 0;
}
```

**输出：**
```asm
section .text
global main

    mov eax, 5
    add eax, 3
```

---

## 📋 支持的指令（28 条）

### 数据传送
- `MOV dest, src` → `mov dest, src`
- `PUSH src` → `push src`
- `POP dest` → `pop dest`
- `LEA dest, addr` → `lea dest, addr`
- `NOP` → `nop`

### 算术运算
- `ADD dest, src` → `add dest, src`
- `SUB dest, src` → `sub dest, src`
- `IMUL dest, src` → `imul dest, src`
- `IDIV src` → `idiv src`
- `INC dest` → `inc dest`
- `DEC dest` → `dec dest`
- `NEG dest` → `neg dest`

### 逻辑运算
- `AND dest, src` → `and dest, src`
- `OR dest, src` → `or dest, src`
- `NOT dest` → `not dest`
- `XOR dest, src` → `xor dest, src`

### 比较跳转
- `CMP dest, src` → `cmp dest, src`
- `TEST dest, src` → `test dest, src`
- `JMP label` → `jmp label`
- `JE label` → `je label`
- `JNE label` → `jne label`
- `JL label` → `jl label`
- `JLE label` → `jle label`
- `JG label` → `jg label`
- `JGE label` → `jge label`

### 函数调用
- `CALL func` → `call func`
- `RET` → `ret`
- `LABEL name` → `name:`

---

## ⚙️ 配置选项

```cpp
IRToAssembly(
    ir,                    // IR 指令列表
    regMap,                // 寄存器映射（可选）
    true,                  // true=Intel 语法，false=AT&T 语法
    true                   // 是否生成注释
);
```

### Intel vs AT&T

```cpp
// Intel: mov eax, 5
IRToAssembly asm1(ir, {}, true, true);

// AT&T: mov $5, %eax
IRToAssembly asm2(ir, {}, false, true);
```

---

## 💡 常用示例

### 1. 变量操作

```cpp
ir.push_back(IRNode(IROp::MOV, {"eax", "[var]"}, ""));
ir.push_back(IRNode(IROp::ADD, {"eax", "1"}, ""));
ir.push_back(IRNode(IROp::MOV, {"[var]", "eax"}, ""));
```

### 2. 条件语句

```cpp
ir.push_back(IRNode(IROp::CMP, {"eax", "0"}, ""));
ir.push_back(IRNode(IROp::JE, {".is_zero"}, ""));
// ... else branch
ir.push_back(IRNode(IROp::JMP, {".end"}, ""));
ir.push_back(IRNode(IROp::LABEL, {".is_zero"}, ""));
// ... then branch
ir.push_back(IRNode(IROp::LABEL, {".end"}, ""));
```

### 3. 循环结构

```cpp
ir.push_back(IRNode(IROp::LABEL, {".loop"}, ""));
// ... loop body
ir.push_back(IRNode(IROp::CMP, {"ecx", "10"}, ""));
ir.push_back(IRNode(IROp::JL, {".loop"}, ""));
```

### 4. 函数调用

```cpp
ir.push_back(IRNode(IROp::PUSH, {"rbp"}, ""));
ir.push_back(IRNode(IROp::MOV, {"rbp", "rsp"}, ""));
ir.push_back(IRNode(IROp::CALL, {"func"}, ""));
ir.push_back(IRNode(IROp::POP, {"rbp"}, ""));
ir.push_back(IRNode(IROp::RET, {}, ""));
```

---

## 🔧 编译命令

```bash
cd Compiler/build
cmake .. -G "MinGW Makefiles"
cmake --build .

# 运行测试
./bin/test_ir_to_assembly.exe
```

---

## 📖 完整文档

- **使用指南**: `IRToAssembly 使用指南.md`
- **实现总结**: `IR 到汇编实现总结.md`
- **测试代码**: `tests/test_ir_to_assembly.cpp`

---

## 🎯 API 速查

| 方法 | 说明 |
|------|------|
| `compile()` | 编译 IR 为汇编字符串 |
| `getAssemblyLines()` | 获取汇编代码行向量 |
| `setRegisterMap(map)` | 设置寄存器映射 |

---

**版本**: 1.0  
**日期**: 2026-03-15  
**状态**: ✅ 完成并测试通过
