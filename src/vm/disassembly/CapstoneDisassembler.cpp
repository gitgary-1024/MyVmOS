#include "CapstoneDisassembler.h"
#include <capstone/capstone.h>
#include <iostream>
#include <cstring>

namespace disassembly {

CapstoneDisassembler::CapstoneDisassembler() : handle_(0) {
    // 初始化 Capstone (x86-64 模式)
    cs_err err = cs_open(CS_ARCH_X86, CS_MODE_64, &handle_);
    if (err != CS_ERR_OK) {
        std::cerr << "[CapstoneDisassembler] Failed to initialize Capstone: " 
                  << cs_strerror(err) << std::endl;
        handle_ = 0;
        return;
    }
    
    // 启用详细模式（获取操作数信息）
    cs_option(handle_, CS_OPT_DETAIL, CS_OPT_ON);
}

CapstoneDisassembler::~CapstoneDisassembler() {
    if (handle_ != 0) {
        cs_close(reinterpret_cast<csh*>(&handle_));
    }
}

std::shared_ptr<SimpleInstruction> CapstoneDisassembler::disassemble_instruction(
    const uint8_t* code,
    size_t code_size,
    uint64_t address
) const {
    if (handle_ == 0 || !code || code_size == 0) {
        return nullptr;
    }
    
    // 计算剩余可反汇编的字节数
    size_t remaining = code_size - static_cast<size_t>(address);
    if (remaining == 0) {
        return nullptr;
    }
    
    // 使用 Capstone 反汇编单条指令
    cs_insn* insn = nullptr;
    size_t count = cs_disasm(
        handle_,
        code + address,
        remaining,
        address,
        1,  // 只反汇编一条指令
        &insn
    );
    
    if (count == 0 || !insn) {
        return nullptr;
    }
    
    // 提取指令信息
    std::string mnemonic = insn->mnemonic;
    std::string op_str = insn->op_str;
    uint16_t size = insn->size;
    
    // 清理 Capstone 分配的内存
    cs_free(insn, count);
    
    // 创建 SimpleInstruction
    auto simple_insn = std::make_shared<SimpleInstruction>();
    simple_insn->address = address;
    simple_insn->size = size;
    simple_insn->mnemonic = mnemonic;
    simple_insn->operands = op_str;
    
    return simple_insn;
}

bool CapstoneDisassembler::is_unconditional_jump(const SimpleInstruction* insn) {
    if (!insn) return false;
    
    const std::string& mnem = insn->mnemonic;
    return (mnem == "jmp");
}

bool CapstoneDisassembler::is_conditional_jump(const SimpleInstruction* insn) {
    if (!insn) return false;
    
    const std::string& mnem = insn->mnemonic;
    
    // x86-64 条件跳转指令
    return (
        mnem == "je"  || mnem == "jz"   ||
        mnem == "jne" || mnem == "jnz"  ||
        mnem == "ja"  || mnem == "jnbe" ||
        mnem == "jae" || mnem == "jnb"  ||
        mnem == "jb"  || mnem == "jnae" ||
        mnem == "jbe" || mnem == "jna"  ||
        mnem == "jc"                   ||
        mnem == "jnc"                  ||
        mnem == "jo"                   ||
        mnem == "jno"                  ||
        mnem == "jp"  || mnem == "jpe"  ||
        mnem == "jnp" || mnem == "jpo"  ||
        mnem == "js"                   ||
        mnem == "jns"                  ||
        mnem == "jl"  || mnem == "jnge" ||
        mnem == "jle" || mnem == "jng"  ||
        mnem == "jg"  || mnem == "jnle" ||
        mnem == "jge" || mnem == "jnl"
    );
}

bool CapstoneDisassembler::is_terminator(const SimpleInstruction* insn) {
    if (!insn) return false;
    
    const std::string& mnem = insn->mnemonic;
    
    // 终止指令：RET, HLT, INT3, SYSCALL, SYSRET, IRET
    return (
        mnem == "ret"   || mnem == "retn"  ||
        mnem == "hlt"   ||
        mnem == "int3"  ||
        mnem == "syscall" ||
        mnem == "sysret"  ||
        mnem == "iret"  || mnem == "iretd" || mnem == "iretq"
    );
}

uint64_t CapstoneDisassembler::extract_jump_target(
    const SimpleInstruction* insn,
    uint64_t current_addr
) {
    if (!insn) return 0;
    
    // 从操作数字符串中提取目标地址
    // Capstone 通常会将相对偏移转换为绝对地址显示
    const std::string& op_str = insn->operands;
    
    // 尝试解析十六进制地址
    if (op_str.empty()) return 0;
    
    // 查找 "0x" 前缀
    size_t hex_pos = op_str.find("0x");
    if (hex_pos != std::string::npos) {
        try {
            uint64_t target = std::stoull(op_str.substr(hex_pos), nullptr, 16);
            return target;
        } catch (...) {
            // 解析失败，返回 0
        }
    }
    
    // 如果是相对跳转，手动计算
    // 这里简化处理：假设 Capstone 已经给出了绝对地址
    // 如果需要更精确的处理，需要访问 Capstone 的详细数据
    return 0;
}

} // namespace disassembly
