#ifndef LIVS_VM_DISASSEMBLY_CAPSTONE_DISASSEMBLER_H
#define LIVS_VM_DISASSEMBLY_CAPSTONE_DISASSEMBLER_H

#include "IR.h"
#include <cstdint>
#include <memory>
#include <string>

// 前向声明 Capstone 句柄类型
typedef uintptr_t csh;

namespace disassembly {

// ===== 支持的架构 =====
enum class Architecture {
    X86,
    // 未来可以添加 ARM, MIPS 等
};

// ===== 运行模式 =====
enum class Mode {
    MODE_16,
    MODE_32,
    MODE_64,
};

// ===== Capstone 反汇编器封装 =====
class CapstoneDisassembler {
public:
    CapstoneDisassembler(Architecture arch, Mode mode);
    ~CapstoneDisassembler();
    
    // 禁止拷贝
    CapstoneDisassembler(const CapstoneDisassembler&) = delete;
    CapstoneDisassembler& operator=(const CapstoneDisassembler&) = delete;
    
    // 反汇编代码
    std::shared_ptr<InstructionStream> disassemble(
        const uint8_t* code,
        size_t code_size,
        uint64_t start_address = 0,
        size_t max_instructions = 0  // 0 表示全部
    );
    
    // 设置详细模式
    void set_detail_mode(bool enable);
    
    // 获取版本信息
    std::string get_version() const;
    
private:
    Architecture arch_;
    Mode mode_;
    csh handle_;  // Capstone 句柄
};

} // namespace disassembly

#endif // LIVS_VM_DISASSEMBLY_CAPSTONE_DISASSEMBLER_H
