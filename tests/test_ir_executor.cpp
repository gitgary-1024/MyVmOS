#include "disassembly/IR.h"
#include "disassembly/InstructionWithExecutor.h"
#include "disassembly/x86/X86ExecutorRegistry.h"  // 新增：执行器注册表
#include <iostream>
#include <cassert>

using namespace disassembly;

// 简单的 Mock VM 用于测试
class MockVM {
public:
    uint64_t rax = 0;
    uint64_t rbx = 0;
    
    void set_rax(uint64_t val) { rax = val; }
    uint64_t get_rax() const { return rax; }
};

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== Testing ExecutionContext and InstructionWithExecutor ===" << std::endl;
    
    // 测试 1: ExecutionContext 创建
    std::cout << "\n--- Test 1: ExecutionContext Creation ---" << std::endl;
    MockVM vm;
    ExecutionContext ctx(&vm, 0x1000);
    
    assert(ctx.vm_instance == &vm);
    assert(ctx.rip == 0x1000);
    std::cout << "✓ ExecutionContext created successfully" << std::endl;
    std::cout << "  VM instance: " << ctx.vm_instance << std::endl;
    std::cout << "  RIP: 0x" << std::hex << ctx.rip << std::dec << std::endl;
    
    // 测试 2: InstructionWithExecutor 创建
    std::cout << "\n--- Test 2: InstructionWithExecutor Creation ---" << std::endl;
    InstructionWithExecutor insn;
    insn.address = 0x1000;
    insn.size = 3;
    insn.mnemonic = "mov";
    insn.operands_str = "rax, 42";
    insn.category = InstructionCategory::DATA_TRANSFER;
    
    std::cout << "✓ InstructionWithExecutor created successfully" << std::endl;
    std::cout << "  Address: 0x" << std::hex << insn.address << std::dec << std::endl;
    std::cout << "  Size: " << insn.size << " bytes" << std::endl;
    std::cout << "  Mnemonic: " << insn.mnemonic << std::endl;
    std::cout << "  Operands: " << insn.operands_str << std::endl;
    std::cout << "  Category: " << static_cast<int>(insn.category) << std::endl;
    
    // 测试 3: 继承关系验证
    std::cout << "\n--- Test 3: Inheritance Verification ---" << std::endl;
    Instruction* base_ptr = &insn;
    assert(base_ptr->address == 0x1000);
    assert(base_ptr->mnemonic == "mov");
    std::cout << "✓ InstructionWithExecutor correctly inherits from Instruction" << std::endl;
    
    // 测试 4: to_string() 方法
    std::cout << "\n--- Test 4: to_string() Method ---" << std::endl;
    std::string insn_str = insn.to_string();
    assert(insn_str == "mov rax, 42");
    std::cout << "✓ to_string() works correctly: \"" << insn_str << "\"" << std::endl;
    
    // 测试 5: is_category() 方法
    std::cout << "\n--- Test 5: is_category() Method ---" << std::endl;
    assert(insn.is_category(InstructionCategory::DATA_TRANSFER));
    assert(!insn.is_category(InstructionCategory::ARITHMETIC));
    std::cout << "✓ is_category() works correctly" << std::endl;
    
    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    
    // 测试 6: 执行器注册表
    std::cout << "\n--- Test 6: Executor Registry ---" << std::endl;
    auto& registry = disassembly::x86::X86ExecutorRegistry::instance();
    registry.initialize_all_executors();
    std::cout << "✓ ExecutorRegistry initialized successfully" << std::endl;
    
    // 测试 7: 创建执行器并绑定到指令
    std::cout << "\n--- Test 7: Create and Bind Executor ---" << std::endl;
    InstructionWithExecutor insn_mov;
    insn_mov.address = 0x1000;
    insn_mov.size = 10;
    insn_mov.mnemonic = "mov";
    insn_mov.operands_str = "rax, 42";
    insn_mov.category = InstructionCategory::DATA_TRANSFER;
    
    // 从注册表创建执行器
    auto executor = registry.create_executor("mov");
    insn_mov.bind_executor(executor);
    std::cout << "✓ Executor created and bound to instruction" << std::endl;
    
    // 测试 8: 执行指令（目前只是占位实现）
    std::cout << "\n--- Test 8: Execute Instruction ---" << std::endl;
    int length = insn_mov.execute(ctx);
    assert(length == 10);  // 应该返回指令长度
    std::cout << "✓ Instruction executed successfully, length = " << length << std::endl;
    
    // 测试 9: 测试未知指令的 fallback
    std::cout << "\n--- Test 9: Unknown Instruction Fallback ---" << std::endl;
    InstructionWithExecutor insn_unknown;
    insn_unknown.address = 0x2000;
    insn_unknown.size = 1;
    insn_unknown.mnemonic = "unknown_instr";
    auto fallback_executor = registry.create_executor("unknown_instr");
    insn_unknown.bind_executor(fallback_executor);
    int fallback_length = insn_unknown.execute(ctx);
    assert(fallback_length == 1);  // 应该返回 HLT 的长度
    std::cout << "✓ Unknown instruction handled with fallback (HLT)" << std::endl;
    
    std::cout << "\n=== All Extended Tests Passed! ===" << std::endl;
    
    return 0;
}
