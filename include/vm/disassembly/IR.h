#ifndef LIVS_VM_DISASSEMBLY_IR_H
#define LIVS_VM_DISASSEMBLY_IR_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <functional>

namespace disassembly {

// ===== 执行上下文（传递给执行器）=====
struct ExecutionContext {
    void* vm_instance;  // VM 实例指针（类型擦除，避免循环依赖）
    uint64_t rip;       // 当前指令地址
    
    // 未来可扩展：性能计数器、追踪信息等
    
    ExecutionContext(void* vm = nullptr, uint64_t addr = 0)
        : vm_instance(vm), rip(addr) {}
};

// ===== 操作数类型 =====
enum class OperandType {
    NONE,           // 无操作数
    REGISTER,       // 寄存器
    IMMEDIATE,      // 立即数
    MEMORY,         // 内存地址
    LABEL           // 标签/跳转目标
};

// ===== 操作数大小 =====
enum class OperandSize {
    BYTE = 1,       // 8 位
    WORD = 2,       // 16 位
    DWORD = 4,      // 32 位
    QWORD = 8,      // 64 位
};

// ===== 通用操作数（基类）=====
struct Operand {
    OperandType type;
    OperandSize size;
    
    Operand(OperandType t = OperandType::NONE, OperandSize s = OperandSize::BYTE)
        : type(t), size(s) {}
    
    virtual ~Operand() = default;
    
    // 获取操作数的字符串表示
    virtual std::string to_string() const = 0;
};

// ===== 寄存器操作数 =====
struct RegisterOperand : public Operand {
    int reg_id;         // 寄存器 ID（架构特定）
    std::string name;   // 寄存器名称
    
    RegisterOperand(int id, const std::string& n, OperandSize s = OperandSize::QWORD)
        : Operand(OperandType::REGISTER, s), reg_id(id), name(n) {}
    
    std::string to_string() const override { return name; }
};

// ===== 立即数操作数 =====
struct ImmediateOperand : public Operand {
    uint64_t value;
    
    ImmediateOperand(uint64_t v, OperandSize s = OperandSize::QWORD)
        : Operand(OperandType::IMMEDIATE, s), value(v) {}
    
    std::string to_string() const override { 
        return "0x" + std::to_string(value); 
    }
};

// ===== 内存操作数 =====
struct MemoryOperand : public Operand {
    int base_reg;       // 基址寄存器 ID (-1 表示无)
    int index_reg;      // 索引寄存器 ID (-1 表示无)
    int scale;          // 缩放因子 (1, 2, 4, 8)
    int64_t displacement; // 位移量
    std::string base_reg_name;   // 基址寄存器名称（可选）
    std::string index_reg_name;  // 索引寄存器名称（可选）
    
    MemoryOperand(int base = -1, int index = -1, int sc = 1, int64_t disp = 0,
                  OperandSize s = OperandSize::QWORD,
                  const std::string& base_name = "", const std::string& index_name = "")
        : Operand(OperandType::MEMORY, s), 
          base_reg(base), index_reg(index), scale(sc), displacement(disp),
          base_reg_name(base_name), index_reg_name(index_name) {}
    
    std::string to_string() const override;
};

// ===== 指令类别 =====
enum class InstructionCategory {
    UNKNOWN,
    DATA_TRANSFER,      // MOV, PUSH, POP 等
    ARITHMETIC,         // ADD, SUB, MUL, DIV 等
    LOGICAL,            // AND, OR, XOR, NOT 等
    CONTROL_FLOW,       // JMP, CALL, RET, Jcc 等
    STACK_OPERATION,    // PUSH, POP, ENTER, LEAVE
    FLAG_OPERATION,     // STC, CLC, STD, CLD 等
    INTERRUPT,          // INT, IRET
    STRING_OPERATION,   // MOVS, CMPS, SCAS, LODS, STOS
    SYSTEM,             // SYSCALL, SYSRET, CPUID
    HALT                // HLT
};

// ===== 通用指令 IR（组合模式的核心）=====
class Instruction {
public:
    uint64_t address;           // 指令地址
    uint16_t size;              // 指令长度（字节）
    std::string mnemonic;       // 助记符（如 "MOV", "ADD"）
    std::string operands_str;   // 操作数字符串（如 "rax, rbx"）
    InstructionCategory category; // 指令类别
    
    // 操作数列表（通用表示）
    std::vector<std::shared_ptr<Operand>> operands;
    
    // 架构特定的详细信息（通过组合方式存储，使用 shared_ptr 自动管理内存）
    std::shared_ptr<void> arch_specific_data;   // 指向 x86 特有数据的智能指针
    
    Instruction()
        : address(0), size(0), category(InstructionCategory::UNKNOWN) {}
    
    virtual ~Instruction() = default;
    
    // 获取指令的完整字符串表示
    std::string to_string() const {
        return mnemonic + " " + operands_str;
    }
    
    // 判断是否属于某个类别
    bool is_category(InstructionCategory cat) const {
        return category == cat;
    }
};

// ===== 指令流（反汇编结果）=====
class InstructionStream {
public:
    std::vector<std::shared_ptr<Instruction>> instructions;
    uint64_t start_address;
    size_t total_bytes;
    
    InstructionStream(uint64_t start_addr = 0)
        : start_address(start_addr), total_bytes(0) {}
    
    void add_instruction(std::shared_ptr<Instruction> insn) {
        instructions.push_back(insn);
        total_bytes += insn->size;
    }
    
    size_t size() const { return instructions.size(); }
    
    std::shared_ptr<Instruction> operator[](size_t index) const {
        return instructions[index];
    }
};

} // namespace disassembly

#endif // LIVS_VM_DISASSEMBLY_IR_H
