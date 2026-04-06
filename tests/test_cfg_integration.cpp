/**
 * @brief ControlFlowGraph 主代码集成测试
 * 
 * 测试增强版跳转目标提取在 CFG 中的正确性
 */

#include "src/vm/disassembly/ControlFlowGraph.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cassert>
#include <algorithm>

using namespace disassembly;

// 辅助函数：打印测试结果
void print_test_result(const std::string& test_name, bool passed) {
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << test_name << std::endl;
}

// 辅助函数：打印基本块信息
void print_block_info(const BasicBlock& block) {
    std::cout << "  Block at 0x" << std::hex << block.start_addr 
              << " - 0x" << block.end_addr << std::dec
              << " (" << block.instructions.size() << " instructions)" << std::endl;
    
    for (const auto& insn : block.instructions) {
        std::cout << "    0x" << std::hex << insn->address << ": " 
                  << insn->mnemonic << " " << insn->operands << std::dec << std::endl;
    }
    
    // 打印后继块
    if (!block.successors.empty()) {
        std::cout << "    Successors: ";
        for (size_t i = 0; i < block.successors.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "0x" << std::hex << block.successors[i];
        }
        std::cout << std::dec << std::endl;
    }
}

// 测试 1: 简单线性代码（无跳转）
void test_linear_code() {
    std::cout << "\n=== Test 1: Linear Code (No Jumps) ===" << std::endl;
    
    // MOV RAX, 1; MOV RBX, 2; RET
    std::vector<uint8_t> code = {
        0xB8, 0x01, 0x00, 0x00, 0x00,  // MOV EAX, 1
        0xBB, 0x02, 0x00, 0x00, 0x00,  // MOV EBX, 2
        0xC3                            // RET
    };
    
    ControlFlowGraph cfg(code.data(), code.size());
    cfg.build(0);
    
    std::cout << "Block count: " << cfg.block_count() << std::endl;
    
    // 预期：1 个基本块
    bool passed = (cfg.block_count() == 1);
    print_test_result("Linear code creates 1 block", passed);
    
    if (passed) {
        const BasicBlock* block = cfg.get_block(0);
        if (block) {
            print_block_info(*block);
        }
    }
}

// 测试 2: 条件跳转（JE）
void test_conditional_jump() {
    std::cout << "\n=== Test 2: Conditional Jump (JE) ===" << std::endl;
    
    std::vector<uint8_t> code = {
        0xB8, 0x01, 0x00, 0x00, 0x00,  // 0x00: MOV EAX, 1
        0x83, 0xF8, 0x01,              // 0x05: CMP EAX, 1
        0x74, 0x03,                    // 0x08: JE +3 (跳转到 0x0D)
        0xB8, 0x02, 0x00, 0x00, 0x00,  // 0x0A: MOV EAX, 2 (fall-through)
        0xC3,                          // 0x0F: RET
        0xB8, 0x03, 0x00, 0x00, 0x00,  // 0x0D: MOV EAX, 3 (jump target)
        0xC3                           // 0x12: RET
    };
    
    ControlFlowGraph cfg(code.data(), code.size());
    cfg.build(0);
    
    std::cout << "Block count: " << cfg.block_count() << std::endl;
    
    // 预期：3 个基本块
    // Block 1: 0x00-0x09 (MOV, CMP, JE)
    // Block 2: 0x0A-0x0F (MOV, RET) - fall-through
    // Block 3: 0x0D-0x12 (MOV, RET) - jump target
    bool passed = (cfg.block_count() == 3);
    print_test_result("Conditional jump creates 3 blocks", passed);
    
    if (passed) {
        std::cout << "\nExpected structure:" << std::endl;
        std::cout << "  Block 0x00: [MOV, CMP, JE]" << std::endl;
        std::cout << "    ├─> Block 0x0D (jump target)" << std::endl;
        std::cout << "    └─> Block 0x0A (fall-through)" << std::endl;
        std::cout << "  Block 0x0A: [MOV, RET]" << std::endl;
        std::cout << "  Block 0x0D: [MOV, RET]" << std::endl;
        
        // 验证第一个块的后继
        const BasicBlock* block0 = cfg.get_block(0);
        if (block0 && block0->successors.size() == 2) {
            // bool has_fallthrough = (block0->successors.count(0x0A) > 0);
            // bool has_jump_target = (block0->successors.count(0x0D) > 0);
            bool has_fallthrough = (std::count(block0->successors.begin(), block0->successors.end(), 0x0A) > 0);
            bool has_jump_target = (std::count(block0->successors.begin(), block0->successors.end(), 0x0D) > 0);
            bool successors_correct = has_fallthrough && has_jump_target;
            print_test_result("JE has correct successors (0x0A and 0x0D)", successors_correct);
        } else {
            print_test_result("JE has correct successors (0x0A and 0x0D)", false);
        }
    }
}

// 测试 3: 无条件跳转（JMP）
void test_unconditional_jump() {
    std::cout << "\n=== Test 3: Unconditional Jump (JMP) ===" << std::endl;
    
    std::vector<uint8_t> code = {
        0xB8, 0x01, 0x00, 0x00, 0x00,  // 0x00: MOV EAX, 1
        0xEB, 0x05,                    // 0x05: JMP +5 (跳转到 0x0C)
        0x90, 0x90, 0x90,              // 0x07: NOP x3 (dead code)
        0xB8, 0x02, 0x00, 0x00, 0x00,  // 0x0A: MOV EAX, 2
        0xC3,                          // 0x0F: RET
        0xB8, 0x03, 0x00, 0x00, 0x00,  // 0x0C: MOV EAX, 3 (jump target)
        0xC3                           // 0x11: RET
    };
    
    ControlFlowGraph cfg(code.data(), code.size());
    cfg.build(0);
    
    std::cout << "Block count: " << cfg.block_count() << std::endl;
    
    // 预期：3 个基本块
    // Block 1: 0x00-0x06 (MOV, JMP)
    // Block 2: 0x07-0x0B (NOP x3, MOV) - 不可达（但会被反汇编）
    // Block 3: 0x0C-0x11 (MOV, RET) - jump target
    bool passed = (cfg.block_count() >= 2);  // 至少应该有 2 个可达块
    print_test_result("Unconditional jump creates multiple blocks", passed);
    
    if (passed) {
        // 验证第一个块只有一个后继（跳转目标）
        const BasicBlock* block0 = cfg.get_block(0);
        if (block0) {
            print_block_info(*block0);
            bool has_single_successor = (block0->successors.size() == 1);
            print_test_result("JMP has exactly 1 successor", has_single_successor);
            
            if (has_single_successor) {
                uint64_t successor = *block0->successors.begin();
                bool correct_target = (successor == 0x0C);
                print_test_result("JMP target is 0x0C", correct_target);
            }
        }
    }
}

// 测试 4: CALL 指令
void test_call_instruction() {
    std::cout << "\n=== Test 4: CALL Instruction ===" << std::endl;
    
    std::vector<uint8_t> code = {
        0xE8, 0x05, 0x00, 0x00, 0x00,  // 0x00: CALL +5 (调用 0x0A)
        0xC3,                          // 0x05: RET
        0x90, 0x90, 0x90,              // 0x06: NOP x3
        0xB8, 0x01, 0x00, 0x00, 0x00,  // 0x09: MOV EAX, 1
        0xC3,                          // 0x0E: RET
        0xB8, 0x02, 0x00, 0x00, 0x00,  // 0x0A: MOV EAX, 2 (call target)
        0xC3                           // 0x0F: RET
    };
    
    ControlFlowGraph cfg(code.data(), code.size());
    cfg.build(0);
    
    std::cout << "Block count: " << cfg.block_count() << std::endl;
    
    // 预期：CALL 应该创建新的入口点
    bool passed = (cfg.block_count() >= 2);
    print_test_result("CALL creates multiple blocks", passed);
    
    if (passed) {
        // 验证 CALL 目标被识别
        const BasicBlock* call_target = cfg.get_block(0x0A);
        bool target_exists = (call_target != nullptr);
        print_test_result("CALL target (0x0A) exists", target_exists);
        
        if (target_exists) {
            print_block_info(*call_target);
        }
    }
}

// 测试 5: RIP-relative 跳转
void test_rip_relative_jump() {
    std::cout << "\n=== Test 5: RIP-relative Jump ===" << std::endl;
    
    std::vector<uint8_t> code = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  // 0x00: JMP [RIP+0] (跳转到 0x06)
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90,  // 0x06: NOP x6
        0xC3                                  // 0x0C: RET
    };
    
    ControlFlowGraph cfg(code.data(), code.size());
    cfg.build(0);
    
    std::cout << "Block count: " << cfg.block_count() << std::endl;
    
    // 预期：RIP-relative 跳转应该被正确解析
    // 注意：这是间接跳转，CFG 可能无法静态确定目标
    // 但至少不应该崩溃
    
    bool passed = (cfg.block_count() >= 1);
    print_test_result("RIP-relative jump doesn't crash CFG", passed);
    
    if (passed) {
        const BasicBlock* block0 = cfg.get_block(0);
        if (block0) {
            print_block_info(*block0);
        }
    }
}

// 测试 6: 复杂控制流（多个跳转）
void test_complex_control_flow() {
    std::cout << "\n=== Test 6: Complex Control Flow ===" << std::endl;
    
    std::vector<uint8_t> code = {
        // 0x00: if (x > 0) goto positive
        0xB8, 0x01, 0x00, 0x00, 0x00,  // MOV EAX, 1
        0x83, 0xF8, 0x00,              // CMP EAX, 0
        0x7F, 0x07,                    // JG +7 (跳转到 0x11)
        
        // 0x0A: negative path
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV EAX, -1
        0xEB, 0x05,                    // JMP +5 (跳转到 0x16)
        
        // 0x11: positive path
        0xB8, 0x01, 0x00, 0x00, 0x00,  // MOV EAX, 1
        0xEB, 0x03,                    // JMP +3 (跳转到 0x16)
        
        // 0x16: merge point
        0xC3                           // RET
    };
    
    ControlFlowGraph cfg(code.data(), code.size());
    cfg.build(0);
    
    std::cout << "Block count: " << cfg.block_count() << std::endl;
    
    // 预期：4 个基本块
    // Block 1: 0x00-0x09 (MOV, CMP, JG)
    // Block 2: 0x0A-0x10 (MOV, JMP) - negative path
    // Block 3: 0x11-0x15 (MOV, JMP) - positive path
    // Block 4: 0x16-0x16 (RET) - merge point
    bool passed = (cfg.block_count() == 4);
    print_test_result("Complex control flow creates 4 blocks", passed);
    
    if (passed) {
        std::cout << "\nAll blocks:" << std::endl;
        cfg.for_each_block([](const BasicBlock& block) {
            print_block_info(block);
            std::cout << std::endl;
        });
        
        // 验证 merge point 有两个前驱
        const BasicBlock* merge = cfg.get_block(0x16);
        if (merge) {
            std::cout << "Merge point (0x16) predecessors: " << merge->predecessors.size() << std::endl;
            bool has_two_predecessors = (merge->predecessors.size() == 2);
            print_test_result("Merge point has 2 predecessors", has_two_predecessors);
        }
    }
}

// 运行所有测试
void run_all_tests() {
    system("chcp 65001 > nul");
    
    std::cout << "========================================" << std::endl;
    std::cout << "ControlFlowGraph 主代码集成测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_linear_code();
    test_conditional_jump();
    test_unconditional_jump();
    test_call_instruction();
    test_rip_relative_jump();
    test_complex_control_flow();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "所有测试完成！" << std::endl;
    std::cout << "========================================" << std::endl;
}

int main() {
    run_all_tests();
    return 0;
}
