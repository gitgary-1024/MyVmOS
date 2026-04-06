#include "CapstoneDisassembler.h"
#include "EnhancedJumpTargetExtractor.h"  // 包含增强版提取器
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
    
    // 创建 SimpleInstruction
    auto simple_insn = std::make_shared<SimpleInstruction>();
    simple_insn->address = address;
    simple_insn->size = size;
    simple_insn->mnemonic = mnemonic;
    simple_insn->operands = op_str;
    
    // ✅ 保存 Capstone 指针用于详细数据提取（不释放，由 SimpleInstruction 管理）
    simple_insn->capstone_insn = static_cast<void*>(insn);
    simple_insn->has_capstone_data = true;
    
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

bool CapstoneDisassembler::is_call(const SimpleInstruction* insn) {
    if (!insn) return false;
    
    const std::string& mnem = insn->mnemonic;
    
    // CALL 指令：call, callq
    return (mnem == "call" || mnem == "callq");
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
    // ✅ 委托给增强版提取器（自动回退到字符串解析）
    return EnhancedJumpTargetExtractor::extract_jump_target(insn, current_addr);
}

}// namespace disassembly
