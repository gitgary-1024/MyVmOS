# Disassembly Module - Capstone-based 反汇编系统

## 概述

本模块提供基于 Capstone 引擎的反汇编功能，采用组合模式设计，支持多架构扩展。

## 文件结构

```
include/vm/disassembly/
├── IR.h                          # 通用指令表示（架构无关）
├── CapstoneDisassembler.h        # Capstone 封装接口
└── x86/
    └── X86Instruction.h          # x86 特有指令数据

src/vm/disassembly/
├── CapstoneDisassembler.cpp      # Capstone 封装实现
└── x86/
    └── X86Instruction.cpp        # x86 指令转换实现

tests/
└── test_new_disassembly.cpp      # 测试程序
```

## 快速开始

### 1. 基本用法

```cpp
#include "vm/disassembly/CapstoneDisassembler.h"

using namespace disassembly;

// 创建 x86-64 反汇编器
CapstoneDisassembler disasm(Architecture::X86, Mode::MODE_64);

// 反汇编代码
uint8_t code[] = { 0xB8, 0x05, 0x00, 0x00, 0x00, 0xF4 }; // MOV EAX, 5; HLT
auto stream = disasm.disassemble(code, sizeof(code), 0x100000);

// 遍历结果
for (size_t i = 0; i < stream->size(); i++) {
    auto insn = (*stream)[i];
    std::cout << insn->to_string() << std::endl;
}
```

### 2. 访问 x86 特有信息

```cpp
if (insn->arch_specific_data) {
    auto x86_data = static_cast<x86::X86InstructionData*>(
        insn->arch_specific_data
    );
    
    // 检查是否有 REX 前缀
    if (x86_data->prefix.has_rex) {
        std::cout << "REX value: 0x" 
                  << std::hex << x86_data->prefix.rex_value << std::endl;
    }
    
    // 检查是否是 64 位指令
    if (x86_data->is_64bit()) {
        std::cout << "64-bit instruction" << std::endl;
    }
}
```

### 3. 在 VM 中使用

```cpp
class MyVM {
    std::unique_ptr<CapstoneDisassembler> disassembler_;
    
public:
    MyVM() {
        disassembler_ = std::make_unique<CapstoneDisassembler>(
            Architecture::X86, Mode::MODE_64
        );
    }
    
    void execute_at(uint64_t address) {
        uint8_t* code = get_memory(address);
        auto stream = disassembler_->disassemble(code, 15, address, 1);
        
        if (stream->size() > 0) {
            auto insn = (*stream)[0];
            
            // 根据指令类别分发
            switch (insn->category) {
                case InstructionCategory::ARITHMETIC:
                    handle_arithmetic(insn);
                    break;
                case InstructionCategory::CONTROL_FLOW:
                    handle_branch(insn);
                    break;
                // ...
            }
        }
    }
};
```

## 编译

```bash
cd build
cmake ..
cmake --build . --target test_new_disassembly
```

## 运行测试

```bash
./bin/test_new_disassembly.exe
```

## 架构设计

### 组合模式

```
Instruction (通用)
  ├── mnemonic, operands_str
  ├── operands (通用操作数列表)
  └── arch_specific_data* ──→ X86InstructionData (x86 特有)
                                   ├── prefix, modrm, sib
                                   └── Capstone 原始信息
```

**优势**：
- VM 代码只需操作通用 `Instruction`
- 需要架构特定信息时通过 `arch_specific_data` 访问
- 易于扩展新架构（ARM, MIPS 等）

### 指令类别

- `DATA_TRANSFER` - MOV, PUSH, POP
- `ARITHMETIC` - ADD, SUB, MUL, DIV
- `LOGICAL` - AND, OR, XOR, NOT
- `CONTROL_FLOW` - JMP, CALL, RET, Jcc
- `STACK_OPERATION` - PUSH, POP, ENTER, LEAVE
- `INTERRUPT` - INT, IRET
- `HALT` - HLT
- `SYSTEM` - SYSCALL, CPUID

## API 参考

### CapstoneDisassembler

```cpp
// 构造函数
CapstoneDisassembler(Architecture arch, Mode mode);

// 反汇编
std::shared_ptr<InstructionStream> disassemble(
    const uint8_t* code,
    size_t code_size,
    uint64_t start_address = 0,
    size_t max_instructions = 0  // 0 = 全部
);

// 设置详细模式
void set_detail_mode(bool enable);

// 获取版本
std::string get_version() const;
```

### Instruction

```cpp
struct Instruction {
    uint64_t address;              // 指令地址
    uint16_t size;                 // 指令长度
    std::string mnemonic;          // 助记符
    std::string operands_str;      // 操作数字符串
    InstructionCategory category;  // 指令类别
    
    std::vector<std::shared_ptr<Operand>> operands;  // 操作数列表
    void* arch_specific_data;      // 架构特定数据
    
    std::string to_string() const; // 完整字符串表示
    bool is_category(InstructionCategory cat) const;
};
```

### Operand

```cpp
// 基类
struct Operand {
    OperandType type;
    OperandSize size;
    virtual std::string to_string() const = 0;
};

// 寄存器操作数
struct RegisterOperand : Operand {
    int reg_id;
    std::string name;
};

// 立即数操作数
struct ImmediateOperand : Operand {
    uint64_t value;
};

// 内存操作数
struct MemoryOperand : Operand {
    int base_reg;
    int index_reg;
    int scale;
    int64_t displacement;
};
```

## 依赖

- **Capstone Engine v6.0** - 位于 `src/vm/capstone-Release/`
- C++17 或更高版本

## 已知问题

1. 寄存器名称映射需要使用 Capstone API 改进
2. REX 前缀检测逻辑需要完善
3. `arch_specific_data` 的内存管理需要改进（建议使用智能指针）

## 未来计划

- [ ] 添加 ARM 架构支持
- [ ] 添加 MIPS 架构支持
- [ ] 实现指令缓存机制
- [ ] 改进内存管理（使用 `unique_ptr`）
- [ ] 添加 JIT 编译支持

## 许可证

本项目使用 Capstone Engine，遵循 BSD 许可证。

## 参考资料

- [Capstone Engine Documentation](https://www.capstone-engine.org/)
- [Capstone反汇编系统设计文档.md](../../Capstone反汇编系统设计文档.md)
