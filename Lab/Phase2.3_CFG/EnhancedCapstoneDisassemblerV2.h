#pragma once

#include "../src/vm/disassembly/CapstoneDisassembler.h"
#include "EnhancedJumpTargetExtractor.h"
#include <capstone/capstone.h>
#include <memory>
#include <iostream>

namespace disassembly {

/**
 * @brief 增强的 Capstone 反汇编器（保存详细数据）
 * 
 * 在原有 CapstoneDisassembler 基础上：
 * - 启用详细模式（CS_OPT_DETAIL）
 * - 保存 cs_insn 指针到 EnhancedSimpleInstruction
 * - 提供更准确的跳转目标提取
 */
class EnhancedCapstoneDisassemblerV2 {
private:
    csh handle_;  // Capstone 句柄
    
public:
    EnhancedCapstoneDisassemblerV2() : handle_(0) {
        // 初始化 Capstone (x86-64 模式)
        cs_err err = cs_open(CS_ARCH_X86, CS_MODE_64, &handle_);
        if (err != CS_ERR_OK) {
            std::cerr << "[EnhancedCapstoneDisassemblerV2] Failed to initialize: " 
                      << cs_strerror(err) << std::endl;
            handle_ = 0;
            return;
        }
        
        // ✅ 启用详细模式（获取操作数信息）
        cs_option(handle_, CS_OPT_DETAIL, CS_OPT_ON);
        
        std::cout << "[EnhancedCapstoneDisassemblerV2] Initialized with DETAIL mode" << std::endl;
    }
    
    ~EnhancedCapstoneDisassemblerV2() {
        if (handle_ != 0) {
            cs_close(reinterpret_cast<csh*>(&handle_));
        }
    }
    
    /**
     * @brief 反汇编单条指令（增强版）
     * 
     * 返回 EnhancedSimpleInstruction，包含 Capstone 原始指令指针
     */
    std::shared_ptr<EnhancedSimpleInstruction> disassemble_instruction(
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
        
        // 创建 EnhancedSimpleInstruction
        auto enhanced_insn = std::make_shared<EnhancedSimpleInstruction>();
        enhanced_insn->address = insn->address;
        enhanced_insn->size = insn->size;
        enhanced_insn->mnemonic = insn->mnemonic;
        enhanced_insn->operands = insn->op_str;
        
        // ✅ 保存 Capstone 原始指令指针
        enhanced_insn->capstone_insn = insn;
        enhanced_insn->has_capstone_data = true;  // 标记有详细数据
        
        // 注意：不要在这里释放 insn
        // 它会在 EnhancedSimpleInstruction 销毁时由调用者管理
        // 或者我们可以立即复制需要的数据然后释放
        
        return enhanced_insn;
    }
    
    /**
     * @brief 反汇编单条指令并立即释放 Capstone 内存
     * 
     * 这个版本会复制需要的详细数据，然后释放 Capstone 分配的内存
     * 更适合长期存储
     */
    std::shared_ptr<EnhancedSimpleInstruction> disassemble_and_copy(
        const uint8_t* code,
        size_t code_size,
        uint64_t address
    ) const {
        if (handle_ == 0 || !code || code_size == 0) {
            return nullptr;
        }
        
        size_t remaining = code_size - static_cast<size_t>(address);
        if (remaining == 0) {
            return nullptr;
        }
        
        cs_insn* insn = nullptr;
        size_t count = cs_disasm(
            handle_,
            code + address,
            remaining,
            address,
            1,
            &insn
        );
        
        if (count == 0 || !insn) {
            return nullptr;
        }
        
        // 创建 EnhancedSimpleInstruction
        auto enhanced_insn = std::make_shared<EnhancedSimpleInstruction>();
        enhanced_insn->address = insn->address;
        enhanced_insn->size = insn->size;
        enhanced_insn->mnemonic = insn->mnemonic;
        enhanced_insn->operands = insn->op_str;
        
        // ⚠️ 复制 Capstone 详细数据（如果需要）
        // 这里我们选择不保存指针，而是立即释放
        // 如果需要访问详细数据，应该在提取跳转目标后立即释放
        
        // 释放 Capstone 分配的内存
        cs_free(insn, count);
        
        // capstone_insn 设为 nullptr，表示已释放
        enhanced_insn->capstone_insn = nullptr;
        enhanced_insn->has_capstone_data = false;  // 标记无详细数据
        
        return enhanced_insn;
    }
    
    /**
     * @brief 提取跳转目标（使用增强版提取器）
     */
    static uint64_t extract_jump_target(
        const cfg::SimpleInstruction* insn,
        uint64_t current_addr
    ) {
        return EnhancedJumpTargetExtractor::extract_jump_target(insn, current_addr);
    }
};

} // namespace disassembly
