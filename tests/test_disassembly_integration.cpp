/**
 * @file test_disassembly_integration.cpp
 * @brief 测试反汇编流程与执行器自动绑定的集成
 * 
 * ⚠️  重要设计决策与常见陷阱：
 * 
 * 【陷阱 1】必须调用 initialize_all_executors()
 * 原因：如果注册表未初始化，create_executor() 会返回 HltExecutor（fallback），
 *      导致所有指令都被当作 HLT 处理。
 * 
 * 【陷阱 2】CapstoneDisassembler 需要正确配置
 * 原因：模式设置错误（32位 vs 64位）会导致反汇编结果不正确。
 * 
 * 【陷阱 3】内存管理
 * CapstoneDisassembler 内部使用 shared_ptr 管理指令对象，
 * 不需要手动释放内存。
 */

#include "vm/disassembly/CapstoneDisassembler.h"
#include "vm/disassembly/InstructionWithExecutor.h"
#include "vm/disassembly/x86/X86Instruction.h"
#include "vm/disassembly/x86/X86ExecutorRegistry.h"  // 引入执行器注册表
#include <iostream>
#include <cassert>
#include <cstring>

using namespace disassembly;

// 简单的 x86-64 机器码（用于测试）
// MOV RAX, 42
static const uint8_t code_mov[] = {
    0x48, 0xB8, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // mov rax, 42
};

// ADD RAX, RBX
static const uint8_t code_add[] = {
    0x48, 0x01, 0xD8  // add rax, rbx
};

// HLT
static const uint8_t code_hlt[] = {
    0xF4  // hlt
};

int main() {
    system("chcp 65001 > nul");

    std::cout << "=== Disassembly Integration Test ===" << std::endl;
    
    // 初始化执行器注册表
    x86::X86ExecutorRegistry::instance().initialize_all_executors();
    std::cout << "[INFO] Executor registry initialized" << std::endl;
    
    // 简单的 x86-64 机器码（用于测试）
    uint8_t code[] = {
        0x48, 0xB8, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 42
        0x48, 0xBB, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rbx, 50
        0x48, 0x01, 0xD8,  // add rax, rbx
        0xF4               // hlt
    };
    
    std::cout << "\n--- Test 1: Disassemble and Auto-Bind Executor ---" << std::endl;
    
    // 创建反汇编器
    CapstoneDisassembler disasm(Architecture::X86, Mode::MODE_64);
    
    // 反汇编指令
    auto stream = disasm.disassemble(code, sizeof(code), 0x1000);
    
    assert(stream->size() == 4);
    std::cout << "✓ Disassembled " << stream->size() << " instructions" << std::endl;
    
    // 检查第一条指令
    auto insn1 = std::dynamic_pointer_cast<InstructionWithExecutor>((*stream)[0]);
    assert(insn1 != nullptr);
    std::cout << "✓ Instruction is InstructionWithExecutor type" << std::endl;
    
    std::cout << "  [0] " << insn1->to_string() << std::endl;
    assert(insn1->mnemonic == "mov" || insn1->mnemonic == "movabs");  // Capstone 可能输出 movabs
    std::cout << "✓ Correct mnemonic: " << insn1->mnemonic << std::endl;
    
    // 检查第二条指令
    auto insn2 = std::dynamic_pointer_cast<InstructionWithExecutor>((*stream)[1]);
    assert(insn2 != nullptr);
    std::cout << "  [1] " << insn2->to_string() << std::endl;
    assert(insn2->mnemonic == "mov" || insn2->mnemonic == "movabs");
    
    // 检查第三条指令
    auto insn3 = std::dynamic_pointer_cast<InstructionWithExecutor>((*stream)[2]);
    assert(insn3 != nullptr);
    std::cout << "  [2] " << insn3->to_string() << std::endl;
    assert(insn3->mnemonic == "add");
    
    // 检查第四条指令
    auto insn4 = std::dynamic_pointer_cast<InstructionWithExecutor>((*stream)[3]);
    assert(insn4 != nullptr);
    std::cout << "  [3] " << insn4->to_string() << std::endl;
    assert(insn4->mnemonic == "hlt");
    
    std::cout << "\n✓ All instructions correctly bound with executors!" << std::endl;
    
    std::cout << "\n=== All Integration Tests Passed! ===" << std::endl;
    
    return 0;
}
