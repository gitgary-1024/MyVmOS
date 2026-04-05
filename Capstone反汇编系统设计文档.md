# Capstone 反汇编系统设计文档

## 概述

本项目实现了一个基于 Capstone 反汇编引擎的新反汇编系统，采用**组合模式**设计，将通用 IR 与 x86 特有 IR 分离，使得 VM 只需操作通用 IR 即可。

## 架构设计

### 1. 目录结构

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
```

### 2. 核心设计理念：组合模式

```
┌─────────────────────────────────────┐
│   Instruction (通用 IR)              │
│  - address, size, mnemonic          │
│  - operands (通用操作数列表)         │
│  - category (指令类别)               │
│  - arch_specific_data* ──────────┐  │
└─────────────────────────────────────┘│
                                       │
                                       ▼
                            ┌──────────────────────┐
                            │ X86InstructionData   │
                            │  (x86 特有数据)       │
                            │  - prefix, modrm     │
                            │  - sib, displacement │
                            │  - Capstone 原始信息  │
                            └──────────────────────┘
```

**关键优势**：
- VM 代码只需操作 `Instruction`，无需关心架构细节
- 需要 x86 特定信息时，通过 `arch_specific_data` 访问
- 未来添加 ARM/MIPS 支持时，只需新增对应的 `*InstructionData`

## 主要组件

### 1. 通用 IR (`IR.h`)

#### 操作数层次结构
```cpp
Operand (基类)
├── RegisterOperand    // 寄存器操作数
├── ImmediateOperand   // 立即数操作数
└── MemoryOperand      // 内存操作数
```

#### 指令类别
```cpp
enum class InstructionCategory {
    DATA_TRANSFER,     // MOV, PUSH, POP
    ARITHMETIC,        // ADD, SUB, MUL, DIV
    LOGICAL,           // AND, OR, XOR
    CONTROL_FLOW,      // JMP, CALL, RET
    STACK_OPERATION,   // PUSH, POP
    INTERRUPT,         // INT, IRET
    HALT,              // HLT
    SYSTEM,            // SYSCALL, CPUID
    ...
};
```

### 2. x86 特有 IR (`X86Instruction.h`)

```cpp
struct X86InstructionData {
    X86Prefix prefix;      // 指令前缀 (REX, OPSIZE, etc.)
    X86ModRM modrm;        // ModR/M 字节解析
    X86SIB sib;            // SIB 字节解析
    int64_t displacement;  // 位移量
    uint64_t immediate;    // 立即数
    
    // Capstone 原始信息
    unsigned int cs_insn_id;
    uint8_t cs_groups[8];
};
```

### 3. Capstone 封装 (`CapstoneDisassembler`)

```cpp
class CapstoneDisassembler {
public:
    // 初始化
    CapstoneDisassembler(Architecture arch, Mode mode);
    
    // 反汇编
    std::shared_ptr<InstructionStream> disassemble(
        const uint8_t* code,
        size_t code_size,
        uint64_t start_address = 0
    );
};
```

## 使用示例

### 基本用法

```cpp
#include "vm/disassembly/CapstoneDisassembler.h"
#include "vm/disassembly/x86/X86Instruction.h"

using namespace disassembly;

// 1. 创建反汇编器
CapstoneDisassembler disasm(Architecture::X86, Mode::MODE_64);

// 2. 反汇编代码
uint8_t code[] = {
    0x48, 0xC7, 0xC0, 0x05, 0x00, 0x00, 0x00,  // MOV RAX, 5
    0x48, 0x01, 0xD8,                           // ADD RAX, RBX
    0xF4                                        // HLT
};

auto stream = disasm.disassemble(code, sizeof(code), 0x100000);

// 3. 遍历指令（通用 IR）
for (size_t i = 0; i < stream->size(); i++) {
    auto insn = (*stream)[i];
    
    // 通用信息
    std::cout << insn->mnemonic << " " << insn->operands_str << std::endl;
    std::cout << "Category: " << static_cast<int>(insn->category) << std::endl;
    
    // 访问操作数
    for (const auto& op : insn->operands) {
        std::cout << "  Operand: " << op->to_string() << std::endl;
    }
    
    // 4. 访问 x86 特有数据（组合模式）
    if (insn->arch_specific_data) {
        auto x86_data = static_cast<x86::X86InstructionData*>(
            insn->arch_specific_data
        );
        
        std::cout << "  Has REX: " << x86_data->prefix.has_rex << std::endl;
        std::cout << "  Is 64-bit: " << x86_data->is_64bit() << std::endl;
    }
}
```

### 在 VM 中集成

```cpp
class X86CPUVM {
private:
    std::unique_ptr<CapstoneDisassembler> disassembler_;
    
public:
    X86CPUVM(uint64_t vm_id) {
        disassembler_ = std::make_unique<CapstoneDisassembler>(
            Architecture::X86, 
            Mode::MODE_64
        );
    }
    
    bool execute_instruction() override {
        uint64_t rip = get_rip();
        uint8_t* code = get_memory_at(rip);
        
        // 反汇编当前指令
        auto stream = disassembler_->disassemble(code, 15, rip, 1);
        if (stream->size() == 0) return false;
        
        auto insn = (*stream)[0];
        
        // 根据通用类别分发
        switch (insn->category) {
            case InstructionCategory::DATA_TRANSFER:
                execute_data_transfer(insn);
                break;
            case InstructionCategory::ARITHMETIC:
                execute_arithmetic(insn);
                break;
            case InstructionCategory::CONTROL_FLOW:
                execute_control_flow(insn);
                break;
            case InstructionCategory::HALT:
                state_ = X86VMState::HALTED;
                return false;
            default:
                // 需要 x86 特有处理
                if (insn->arch_specific_data) {
                    auto x86_data = static_cast<x86::X86InstructionData*>(
                        insn->arch_specific_data
                    );
                    execute_x86_specific(insn, x86_data);
                }
                break;
        }
        
        // 更新 RIP
        set_rip(rip + insn->size);
        return true;
    }
};
```

## 编译配置

### CMakeLists.txt

```cmake
# Capstone 路径
set(CAPSTONE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/vm/capstone-Release/include)
set(CAPSTONE_LIB_DIR ${CMAKE_SOURCE_DIR}/src/vm/capstone-Release/lib)

include_directories(${CAPSTONE_INCLUDE_DIR})

# 添加源文件
set(SOURCES
    ...
    src/vm/disassembly/CapstoneDisassembler.cpp
    src/vm/disassembly/x86/X86Instruction.cpp
)

# 链接 Capstone 库
target_link_libraries(myos_lib ${CAPSTONE_LIB_DIR}/libcapstone.a)
```

## 已知问题与改进方向

### 当前限制

1. **寄存器名称映射不完整**
   - ✅ **已解决**：使用 Capstone 的 `cs_reg_name()` API 动态获取名称

2. **REX 前缀检测不准确**
   - ✅ **已解决**：添加了对 Capstone `x86.rex` 字段的检查，双重保障

3. **内存管理**
   - ✅ **已解决**：改用 `std::shared_ptr<void>` 自动管理内存

### 改进建议

1. **支持更多架构**
   - 添加 `ARMInstructionData`
   - 添加 `MIPSInstructionData`
   - VM 可以通过统一的 `Instruction` 接口执行不同架构的代码

2. **性能优化**
   - 缓存已反汇编的指令
   - 使用 `cs_disasm_iter()` 进行流式反汇编

## 测试

运行测试程序验证功能：

```bash
cd build
cmake --build . --target test_new_disassembly
../bin/test_new_disassembly.exe
```

预期输出：
```
=== Testing New Capstone-based Disassembly System ===
Capstone version: Capstone v6.0

--- Test 1: Simple 32-bit instructions ---
Disassembled 4 instructions:
  [0] 0x00100000:  mov     eax, 5
  [1] 0x00100005:  mov     ebx, 3
  [2] 0x0010000a:  add     eax, ebx
  [3] 0x0010000c:  hlt
```

## 总结

新的反汇编系统通过**组合模式**成功实现了：

✅ **架构无关的通用 IR** - VM 代码不依赖具体架构  
✅ **灵活的扩展性** - 轻松添加新架构支持  
✅ **完整的 x86 信息** - 保留所有底层细节供需要时使用  
✅ **清晰的职责分离** - 通用逻辑与架构特定逻辑分离  

这为未来的多架构 VM 支持奠定了坚实的基础。
