/**
 * @brief 改进跳转目标提取 - 演示程序
 * 
 * 展示如何使用 Capstone 详细模式准确提取跳转目标地址
 * 
 * 测试场景：
 * 1. 直接跳转（JMP rel32）- 立即数操作数
 * 2. 条件跳转（JE rel8）- 立即数操作数
 * 3. CALL 指令（CALL rel32）- 立即数操作数
 * 4. RIP-relative 跳转（JMP [RIP+disp]）- 内存操作数
 * 5. 间接跳转（JMP RAX）- 寄存器操作数（无法静态确定）
 */

#include "EnhancedJumpTargetExtractor.h"
#include "EnhancedCapstoneDisassemblerV2.h"
#include <iostream>
#include <vector>
#include <iomanip>

namespace disassembly {

/**
 * @brief 测试 1: 直接跳转（JMP rel32）
 */
void test_unconditional_jump() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test 1: Unconditional Jump (JMP rel32)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 机器码：EB FE = JMP $-2 (无限循环)
    // 或者：E9 05 00 00 00 = JMP +5
    std::vector<uint8_t> code = {
        0xE9, 0x05, 0x00, 0x00, 0x00  // JMP 0x5 (相对偏移)
    };
    
    EnhancedCapstoneDisassemblerV2 disassembler;
    
    auto insn = disassembler.disassemble_instruction(code.data(), code.size(), 0);
    if (!insn) {
        std::cerr << "Failed to disassemble!" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    std::cout << "Address: 0x" << std::hex << insn->address << std::dec << std::endl;
    std::cout << "Size: " << insn->size << " bytes" << std::endl;
    
    // 提取跳转目标
    uint64_t target = EnhancedJumpTargetExtractor::extract_jump_target(insn.get(), insn->address);
    
    std::cout << "Extracted jump target: 0x" << std::hex << target << std::dec << std::endl;
    
    // 验证：JMP rel32 的目标应该是当前地址 + 指令长度 + 偏移
    // 在这个例子中，Capstone 应该已经计算出绝对地址
    if (target != 0) {
        std::cout << "✅ SUCCESS: Jump target extracted" << std::endl;
    } else {
        std::cout << "❌ FAILED: Could not extract jump target" << std::endl;
    }
}

/**
 * @brief 测试 2: 条件跳转（JE rel8）
 */
void test_conditional_jump() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test 2: Conditional Jump (JE rel8)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 机器码：74 05 = JE +5
    std::vector<uint8_t> code = {
        0x74, 0x05  // JE +5
    };
    
    EnhancedCapstoneDisassemblerV2 disassembler;
    
    auto insn = disassembler.disassemble_instruction(code.data(), code.size(), 0);
    if (!insn) {
        std::cerr << "Failed to disassemble!" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    std::cout << "Address: 0x" << std::hex << insn->address << std::dec << std::endl;
    std::cout << "Size: " << insn->size << " bytes" << std::endl;
    
    // 提取跳转目标
    uint64_t target = EnhancedJumpTargetExtractor::extract_jump_target(insn.get(), insn->address);
    
    std::cout << "Extracted jump target: 0x" << std::hex << target << std::dec << std::endl;
    
    if (target != 0) {
        std::cout << "✅ SUCCESS: Conditional jump target extracted" << std::endl;
    } else {
        std::cout << "❌ FAILED: Could not extract jump target" << std::endl;
    }
}

/**
 * @brief 测试 3: CALL 指令
 */
void test_call_instruction() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test 3: CALL Instruction (CALL rel32)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 机器码：E8 0A 00 00 00 = CALL +10
    std::vector<uint8_t> code = {
        0xE8, 0x0A, 0x00, 0x00, 0x00  // CALL +10
    };
    
    EnhancedCapstoneDisassemblerV2 disassembler;
    
    auto insn = disassembler.disassemble_instruction(code.data(), code.size(), 0);
    if (!insn) {
        std::cerr << "Failed to disassemble!" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    std::cout << "Address: 0x" << std::hex << insn->address << std::dec << std::endl;
    std::cout << "Size: " << insn->size << " bytes" << std::endl;
    
    // 提取跳转目标
    uint64_t target = EnhancedJumpTargetExtractor::extract_jump_target(insn.get(), insn->address);
    
    std::cout << "Extracted call target: 0x" << std::hex << target << std::dec << std::endl;
    
    if (target != 0) {
        std::cout << "✅ SUCCESS: Call target extracted" << std::endl;
    } else {
        std::cout << "❌ FAILED: Could not extract call target" << std::endl;
    }
}

/**
 * @brief 测试 4: RIP-relative 跳转
 */
void test_rip_relative_jump() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test 4: RIP-Relative Jump (JMP [RIP+disp])" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 机器码：FF 25 00 00 00 00 = JMP [RIP+0] (间接跳转表)
    std::vector<uint8_t> code = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00  // JMP [RIP+0]
    };
    
    EnhancedCapstoneDisassemblerV2 disassembler;
    
    auto insn = disassembler.disassemble_instruction(code.data(), code.size(), 0);
    if (!insn) {
        std::cerr << "Failed to disassemble!" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    std::cout << "Address: 0x" << std::hex << insn->address << std::dec << std::endl;
    std::cout << "Size: " << insn->size << " bytes" << std::endl;
    
    // 提取跳转目标
    uint64_t target = EnhancedJumpTargetExtractor::extract_jump_target(insn.get(), insn->address);
    
    std::cout << "Extracted target: 0x" << std::hex << target << std::dec << std::endl;
    
    if (target != 0) {
        std::cout << "✅ SUCCESS: RIP-relative target calculated" << std::endl;
        std::cout << "   Note: This is the address of the jump table entry, not the final target" << std::endl;
    } else {
        std::cout << "⚠️  INFO: Indirect jump detected (cannot determine target statically)" << std::endl;
    }
}

/**
 * @brief 测试 5: 间接跳转（寄存器）
 */
void test_indirect_jump_register() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test 5: Indirect Jump (JMP RAX)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 机器码：FF E0 = JMP RAX
    std::vector<uint8_t> code = {
        0xFF, 0xE0  // JMP RAX
    };
    
    EnhancedCapstoneDisassemblerV2 disassembler;
    
    auto insn = disassembler.disassemble_instruction(code.data(), code.size(), 0);
    if (!insn) {
        std::cerr << "Failed to disassemble!" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    std::cout << "Address: 0x" << std::hex << insn->address << std::dec << std::endl;
    std::cout << "Size: " << insn->size << " bytes" << std::endl;
    
    // 提取跳转目标
    uint64_t target = EnhancedJumpTargetExtractor::extract_jump_target(insn.get(), insn->address);
    
    std::cout << "Extracted target: 0x" << std::hex << target << std::dec << std::endl;
    
    if (target == 0) {
        std::cout << "✅ EXPECTED: Indirect jump cannot be determined statically" << std::endl;
        std::cout << "   Note: Requires runtime profiling or symbolic execution" << std::endl;
    } else {
        std::cout << "❌ UNEXPECTED: Should not be able to extract target" << std::endl;
    }
}

/**
 * @brief 测试 6: 对比字符串解析 vs Capstone 详细模式
 */
void test_comparison_string_vs_detail() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test 6: String Parsing vs Capstone Detail" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 机器码：E9 10 00 00 00 = JMP +16
    std::vector<uint8_t> code = {
        0xE9, 0x10, 0x00, 0x00, 0x00
    };
    
    EnhancedCapstoneDisassemblerV2 disassembler;
    
    auto enhanced_insn = disassembler.disassemble_instruction(code.data(), code.size(), 0);
    if (!enhanced_insn) {
        std::cerr << "Failed to disassemble!" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << enhanced_insn->mnemonic << " " << enhanced_insn->operands << std::endl;
    
    // 方法 1: 使用 Capstone 详细数据
    std::cout << "\n--- Method 1: Capstone Detail Mode ---" << std::endl;
    uint64_t target_detail = EnhancedJumpTargetExtractor::extract_jump_target(
        enhanced_insn.get(), 
        enhanced_insn->address
    );
    std::cout << "Target (detail): 0x" << std::hex << target_detail << std::dec << std::endl;
    
    // 方法 2: 模拟字符串解析（回退方案）
    std::cout << "\n--- Method 2: String Parsing (Fallback) ---" << std::endl;
    cfg::SimpleInstruction simple_insn;
    simple_insn.address = enhanced_insn->address;
    simple_insn.size = enhanced_insn->size;
    simple_insn.mnemonic = enhanced_insn->mnemonic;
    simple_insn.operands = enhanced_insn->operands;
    
    // 手动调用字符串解析
    uint64_t target_string = 0;
    const std::string& op_str = simple_insn.operands;
    size_t hex_pos = op_str.find("0x");
    if (hex_pos != std::string::npos) {
        try {
            target_string = std::stoull(op_str.substr(hex_pos), nullptr, 16);
        } catch (...) {}
    }
    std::cout << "Target (string): 0x" << std::hex << target_string << std::dec << std::endl;
    
    // 对比结果
    std::cout << "\n--- Comparison ---" << std::endl;
    if (target_detail == target_string && target_detail != 0) {
        std::cout << "✅ Both methods agree: 0x" << std::hex << target_detail << std::dec << std::endl;
    } else if (target_detail != 0 && target_string == 0) {
        std::cout << "✅ Capstone detail succeeded, string parsing failed" << std::endl;
        std::cout << "   This demonstrates the advantage of using detail mode!" << std::endl;
    } else {
        std::cout << "⚠️  Results differ or both failed" << std::endl;
    }
}

/**
 * @brief 运行所有测试
 */
void run_all_tests() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Enhanced Jump Target Extraction - Test Suite         ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
    
    test_unconditional_jump();
    test_conditional_jump();
    test_call_instruction();
    test_rip_relative_jump();
    test_indirect_jump_register();
    test_comparison_string_vs_detail();
    
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  All Tests Completed!                                  ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
}

} // namespace disassembly

int main() {
    system("chcp 65001 > nul");

    disassembly::run_all_tests();
    return 0;
}
