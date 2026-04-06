/**
 * @brief CALL/RET 分析增强演示
 * 
 * 这个文件展示了如何在不修改主代码的情况下，
 * 在 Lab 目录中实现增强的 CALL/RET 分析功能。
 * 
 * 核心思路：
 * 1. 创建 CallGraph 数据结构（CallGraph.h）
 * 2. 添加 is_call() 辅助函数（EnhancedCapstoneDisassembler.h）
 * 3. 修改 ControlFlowGraph::create_basic_block 处理 CALL 指令
 * 4. 实现递归检测算法
 */

#include "CallGraph.h"
#include "EnhancedCapstoneDisassembler.h"
#include <iostream>
#include <vector>
#include <cstdint>

namespace disassembly {

/**
 * @brief 演示：修改后的 create_basic_block 逻辑
 * 
 * 这是伪代码，展示了应该如何修改 ControlFlowGraph.cpp 中的
 * create_basic_block 方法来支持 CALL 指令的双后继处理。
 */
void demonstrate_enhanced_basic_block_creation() {
    std::cout << "\n=== Enhanced Basic Block Creation Demo ===" << std::endl;
    
    // 模拟的代码数据
    std::vector<uint8_t> mock_code = {
        0x55,                   // 0x00: push rbp
        0x48, 0x89, 0xE5,      // 0x01: mov rbp, rsp
        0xB8, 0x0A, 0x00, 0x00, 0x00,  // 0x04: mov eax, 10
        0xE8, 0x05, 0x00, 0x00, 0x00,  // 0x09: call 0x13 (调用函数)
        0xC9,                   // 0x0E: leave
        0xC3,                   // 0x0F: ret
        
        // 被调用的函数
        0x55,                   // 0x10: push rbp
        0x48, 0x89, 0xE5,      // 0x11: mov rbp, rsp
        0x8B, 0x45, 0xFC,      // 0x14: mov eax, [rbp-4]
        0xC9,                   // 0x17: leave
        0xC3                    // 0x18: ret
    };
    
    const uint8_t* code = mock_code.data();
    size_t code_size = mock_code.size();
    
    std::cout << "Mock code size: " << code_size << " bytes" << std::endl;
    std::cout << "Entry point: 0x0" << std::endl;
    
    // ===== 步骤 1: 创建 CallGraph =====
    CallGraph call_graph;
    
    // ===== 步骤 2: 模拟反汇编过程 =====
    std::cout << "\n--- Simulating disassembly ---" << std::endl;
    
    // 模拟在地址 0x09 遇到 CALL 指令
    uint64_t call_addr = 0x09;
    uint64_t call_target = 0x13;  // 假设提取到的目标地址
    uint64_t fallthrough = call_addr + 5;  // CALL 指令长度 = 5
    
    std::cout << "Found CALL instruction at 0x" << std::hex << call_addr << std::dec << std::endl;
    std::cout << "  Call target: 0x" << std::hex << call_target << std::dec << std::endl;
    std::cout << "  Fall-through: 0x" << std::hex << fallthrough << std::dec << std::endl;
    
    // ===== 步骤 3: 记录调用关系 =====
    call_graph.add_call(call_addr, call_target);
    
    std::cout << "\n--- Call Graph after recording CALL ---" << std::endl;
    call_graph.print();
    
    // ===== 步骤 4: 验证 CALL 检测 =====
    SimpleInstruction mock_call_insn;
    mock_call_insn.address = call_addr;
    mock_call_insn.size = 5;
    mock_call_insn.mnemonic = "call";
    mock_call_insn.operands = "0x13";
    
    bool is_call = EnhancedCapstoneDisassembler::is_call(&mock_call_insn);
    std::cout << "\n--- CALL Detection Test ---" << std::endl;
    std::cout << "Instruction: " << mock_call_insn.mnemonic << " " << mock_call_insn.operands << std::endl;
    std::cout << "Is CALL? " << (is_call ? "YES ✓" : "NO ✗") << std::endl;
    
    // ===== 步骤 5: 演示 RET 检测 =====
    SimpleInstruction mock_ret_insn;
    mock_ret_insn.address = 0x0F;
    mock_ret_insn.size = 1;
    mock_ret_insn.mnemonic = "ret";
    mock_ret_insn.operands = "";
    
    bool is_terminator = CapstoneDisassembler::is_terminator(&mock_ret_insn);
    std::cout << "\n--- RET Detection Test ---" << std::endl;
    std::cout << "Instruction: " << mock_ret_insn.mnemonic << std::endl;
    std::cout << "Is terminator? " << (is_terminator ? "YES ✓" : "NO ✗") << std::endl;
    
    std::cout << "\n=== Demo Complete ===" << std::endl;
}

/**
 * @brief 演示：如何在 BFS 循环中处理 CALL 指令
 * 
 * 这是伪代码，展示了应该插入到 ControlFlowGraph::create_basic_block
 * 中的逻辑。
 */
void demonstrate_call_handling_in_bfs() {
    std::cout << "\n=== CALL Handling in BFS Loop Demo ===" << std::endl;
    
    // 模拟的待处理边集合
    std::unordered_map<uint64_t, std::vector<uint64_t>> pending_edges;
    std::set<uint64_t> pending_entries;
    
    uint64_t current_block_start = 0x00;
    uint64_t call_addr = 0x09;
    uint64_t call_target = 0x13;
    uint64_t fallthrough = 0x0E;
    
    std::cout << "Current block start: 0x" << std::hex << current_block_start << std::dec << std::endl;
    std::cout << "Processing CALL at 0x" << std::hex << call_addr << std::dec << std::endl;
    
    // ===== 关键修改：CALL 有两个后继 =====
    
    // 1. 添加调用目标
    pending_edges[current_block_start].push_back(call_target);
    pending_entries.insert(call_target);
    std::cout << "  Added call target: 0x" << std::hex << call_target << std::dec << std::endl;
    
    // 2. 添加 Fall-through（返回地址）
    pending_edges[current_block_start].push_back(fallthrough);
    pending_entries.insert(fallthrough);
    std::cout << "  Added fall-through: 0x" << std::hex << fallthrough << std::dec << std::endl;
    
    // 3. 基本块终止
    std::cout << "  Block terminated by CALL" << std::endl;
    
    std::cout << "\nPending entries:" << std::endl;
    for (uint64_t entry : pending_entries) {
        std::cout << "  - 0x" << std::hex << entry << std::dec << std::endl;
    }
    
    std::cout << "\n=== Demo Complete ===" << std::endl;
}

/**
 * @brief 演示：递归检测
 */
void demonstrate_recursion_detection() {
    std::cout << "\n=== Recursion Detection Demo ===" << std::endl;
    
    CallGraph call_graph;
    
    // 模拟递归调用：func(0x100) -> func(0x100)
    uint64_t func_addr = 0x100;
    call_graph.add_call(func_addr, func_addr);
    
    std::cout << "Function at 0x" << std::hex << func_addr << std::dec 
              << " calls itself" << std::endl;
    std::cout << "Is recursive? " 
              << (call_graph.is_recursive(func_addr) ? "YES ✓" : "NO ✗") 
              << std::endl;
    
    // 打印 Call Graph
    call_graph.print();
    
    std::cout << "\n=== Demo Complete ===" << std::endl;
}

} // namespace disassembly

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  CALL/RET Analysis Enhancement Demo" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 运行所有演示
    disassembly::demonstrate_enhanced_basic_block_creation();
    disassembly::demonstrate_call_handling_in_bfs();
    disassembly::demonstrate_recursion_detection();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  All Demos Completed Successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
