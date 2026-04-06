/**
 * @brief 改进跳转目标提取 - 主代码集成测试
 * 
 * 测试增强版跳转目标提取功能是否正确集成到主代码中
 */

#include "src/vm/disassembly/CapstoneDisassembler.h"  // CFG 使用的简化版
#include <iostream>
#include <vector>
#include <iomanip>
#include <cassert>

using namespace disassembly;

// 辅助函数：打印测试结果
void print_test_result(const std::string& test_name, bool passed) {
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << test_name << std::endl;
}

// 测试 1: 直接跳转（JMP rel32）
void test_direct_jump() {
    std::cout << "\n=== Test 1: Direct Jump (JMP rel32) ===" << std::endl;
    
    // JMP +5 (0xE9 0x05 0x00 0x00 0x00)
    std::vector<uint8_t> code = {
        0x90, 0x90, 0x90, 0x90, 0x90,  // NOP x5 (address 0x0-0x4)
        0xE9, 0x05, 0x00, 0x00, 0x00   // JMP +5 (address 0x5)
    };
    
    CapstoneDisassembler disasm;
    auto insn = disasm.disassemble_instruction(code.data(), code.size(), 5);
    
    if (!insn) {
        std::cout << "[ERROR] Failed to disassemble instruction" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    std::cout << "Address: 0x" << std::hex << insn->address << std::endl;
    
    uint64_t target = CapstoneDisassembler::extract_jump_target(insn.get(), insn->address);
    std::cout << "Extracted jump target: 0x" << std::hex << target << std::endl;
    
    // 预期：0x5 + 5 (指令长度) + 5 (偏移) = 0xF
    bool passed = (target == 0xF);
    print_test_result("Direct jump target extraction", passed);
}

// 测试 2: 条件跳转（JE rel8）
void test_conditional_jump() {
    std::cout << "\n=== Test 2: Conditional Jump (JE rel8) ===" << std::endl;
    
    // JE +5 (0x74 0x05)
    std::vector<uint8_t> code = {
        0x90, 0x90, 0x90, 0x90, 0x90,  // NOP x5
        0x74, 0x05                      // JE +5
    };
    
    CapstoneDisassembler disasm;
    auto insn = disasm.disassemble_instruction(code.data(), code.size(), 5);
    
    if (!insn) {
        std::cout << "[ERROR] Failed to disassemble instruction" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    
    uint64_t target = CapstoneDisassembler::extract_jump_target(insn.get(), insn->address);
    std::cout << "Extracted jump target: 0x" << std::hex << target << std::endl;
    
    // 预期：0x5 + 2 (指令长度) + 5 (偏移) = 0xC
    bool passed = (target == 0xC);
    print_test_result("Conditional jump target extraction", passed);
}

// 测试 3: CALL 指令
void test_call_instruction() {
    std::cout << "\n=== Test 3: CALL Instruction ===" << std::endl;
    
    // CALL +10 (0xE8 0x0A 0x00 0x00 0x00)
    std::vector<uint8_t> code = {
        0x90, 0x90, 0x90, 0x90, 0x90,  // NOP x5
        0xE8, 0x0A, 0x00, 0x00, 0x00   // CALL +10
    };
    
    CapstoneDisassembler disasm;
    auto insn = disasm.disassemble_instruction(code.data(), code.size(), 5);
    
    if (!insn) {
        std::cout << "[ERROR] Failed to disassemble instruction" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    
    uint64_t target = CapstoneDisassembler::extract_jump_target(insn.get(), insn->address);
    std::cout << "Extracted call target: 0x" << std::hex << target << std::endl;
    
    // 预期：0x5 + 5 (指令长度) + 10 (偏移) = 0x14
    bool passed = (target == 0x14);
    print_test_result("CALL target extraction", passed);
}

// 测试 4: RIP-relative 跳转
void test_rip_relative_jump() {
    std::cout << "\n=== Test 4: RIP-relative Jump ===" << std::endl;
    
    // JMP [RIP+0] (0xFF 0x25 0x00 0x00 0x00 0x00)
    std::vector<uint8_t> code = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90,  // NOP x6
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00   // JMP [RIP+0]
    };
    
    CapstoneDisassembler disasm;
    auto insn = disasm.disassemble_instruction(code.data(), code.size(), 6);
    
    if (!insn) {
        std::cout << "[ERROR] Failed to disassemble instruction" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    
    uint64_t target = CapstoneDisassembler::extract_jump_target(insn.get(), insn->address);
    std::cout << "Extracted target: 0x" << std::hex << target << std::endl;
    
    // 预期：0x6 + 6 (指令长度) + 0 (disp) = 0xC
    bool passed = (target == 0xC);
    print_test_result("RIP-relative jump target extraction", passed);
}

// 测试 5: 间接跳转（寄存器）
void test_indirect_jump() {
    std::cout << "\n=== Test 5: Indirect Jump (Register) ===" << std::endl;
    
    // JMP RAX (0xFF 0xE0)
    std::vector<uint8_t> code = {
        0x90, 0x90,  // NOP x2
        0xFF, 0xE0   // JMP RAX
    };
    
    CapstoneDisassembler disasm;
    auto insn = disasm.disassemble_instruction(code.data(), code.size(), 2);
    
    if (!insn) {
        std::cout << "[ERROR] Failed to disassemble instruction" << std::endl;
        return;
    }
    
    std::cout << "Instruction: " << insn->mnemonic << " " << insn->operands << std::endl;
    
    uint64_t target = CapstoneDisassembler::extract_jump_target(insn.get(), insn->address);
    std::cout << "Extracted target: 0x" << std::hex << target << std::dec << std::endl;
    
    // 预期：0（无法静态确定）
    bool passed = (target == 0);
    print_test_result("Indirect jump returns 0", passed);
}

// 测试 6: 验证 Capstone 数据被正确保存
void test_capstone_data_preserved() {
    std::cout << "\n=== Test 6: Capstone Data Preservation ===" << std::endl;
    
    std::vector<uint8_t> code = {
        0xE9, 0x05, 0x00, 0x00, 0x00  // JMP +5
    };
    
    CapstoneDisassembler disasm;
    auto insn = disasm.disassemble_instruction(code.data(), code.size(), 0);
    
    if (!insn) {
        std::cout << "[ERROR] Failed to disassemble instruction" << std::endl;
        return;
    }
    
    std::cout << "has_capstone_data: " << (insn->has_capstone_data ? "true" : "false") << std::endl;
    std::cout << "capstone_insn pointer: " << insn->capstone_insn << std::endl;
    
    bool passed = (insn->has_capstone_data && insn->capstone_insn != nullptr);
    print_test_result("Capstone data is preserved", passed);
}

// 运行所有测试
void run_all_tests() {
    system("chcp 65001 > nul");
    
    std::cout << "========================================" << std::endl;
    std::cout << "改进跳转目标提取 - 主代码集成测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_direct_jump();
    test_conditional_jump();
    test_call_instruction();
    test_rip_relative_jump();
    test_indirect_jump();
    test_capstone_data_preserved();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "所有测试完成！" << std::endl;
    std::cout << "========================================" << std::endl;
}

int main() {
    run_all_tests();
    return 0;
}
