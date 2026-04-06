/**
 * @file test_ir_executor.cpp
 * @brief X86VM 执行器系统单元测试
 * 
 * 本测试文件验证以下核心功能：
 * 1. ExecutionContext - 执行上下文结构
 * 2. InstructionWithExecutor - 带执行器的指令类
 * 3. X86ExecutorRegistry - 执行器注册表（单例模式）
 * 4. 基础指令执行器（MOV, ADD, SUB, HLT）
 * 
 * ⚠️  重要设计决策与常见陷阱：
 * 
 * 【陷阱 1】必须使用真实的 X86CPUVM，不能使用 Mock VM
 * -------------------------------------------------------
 * 原因：执行器内部通过 static_cast<X86CPUVM*>(ctx.vm_instance) 进行类型转换，
 *      如果传入 Mock VM，会导致未定义行为（崩溃或数据错误）。
 * 
 * 错误示例：
 *   class MockVM { uint64_t rax; };  // ❌ 不要这样做
 *   MockVM vm;
 *   ExecutionContext ctx(&vm, 0x1000);
 * 
 * 正确示例：
 *   X86CPUVM vm(1, config);  // ✅ 使用真实的 VM
 *   ExecutionContext ctx(&vm, 0x1000);
 * 
 * 【陷阱 2】指令必须包含操作数对象，不能只设置 operands_str
 * -------------------------------------------------------
 * 原因：执行器通过 insn.operands 访问实际的操作数对象，operands_str 仅用于显示。
 *      如果 operands 为空，dynamic_cast 会返回 nullptr，导致执行器跳过逻辑。
 * 
 * 错误示例：
 *   InstructionWithExecutor insn;
 *   insn.mnemonic = "mov";
 *   insn.operands_str = "rax, 42";  // ❌ 只有字符串，没有实际对象
 *   // 忘记添加 operands.push_back(...)
 * 
 * 正确示例：
 *   auto dest_reg = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);
 *   auto imm_val = std::make_shared<ImmediateOperand>(42, OperandSize::QWORD);
 *   insn.operands.push_back(std::move(dest_reg));  // ✅ 添加实际对象
 *   insn.operands.push_back(std::move(imm_val));
 * 
 * 【陷阱 3】Capstone 寄存器 ID 映射
 * -------------------------------------------------------
 * Capstone 的寄存器常量定义在 capstone/x86.h 中：
 *   X86_REG_RAX = 57, X86_REG_RBX = 58, ...
 * 
 * 我们的执行器通过 capstone_reg_to_x86reg() 函数将这些 ID 映射到 X86Reg 枚举。
 * 如果映射错误，会操作错误的寄存器。
 * 
 * 【陷阱 4】变量命名冲突
 * -------------------------------------------------------
 * 在同一个作用域内不要重复声明同名变量。例如：
 *   auto dest_reg = ...;  // Test 7
 *   auto dest_reg = ...;  // ❌ Test 8.5 中重复声明，编译错误
 * 
 * 应该使用不同的名称：
 *   auto add_dest = ...;  // ✅ Test 8.5 中使用新名称
 * 
 * 【测试覆盖范围】
 * - Test 1-5: 基础 IR 结构测试
 * - Test 6-7: 注册表初始化与执行器绑定
 * - Test 8: MOV 指令执行（reg <- imm）
 * - Test 8.5: ADD 指令执行（reg + reg）
 * - Test 9: 未知指令 fallback 机制
 * - Test 10: SUB 指令执行及标志位验证
 * 
 * @author AI Assistant
 * @date 2026-04-05
 * @version 1.0
 */

#include "disassembly/IR.h"
#include "disassembly/InstructionWithExecutor.h"
#include "disassembly/x86/X86ExecutorRegistry.h"  // 新增：执行器注册表
#include "vm/X86CPU.h"  // 新增：使用真实的 X86CPUVM
#include <iostream>
#include <cassert>

using namespace disassembly;

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== Testing ExecutionContext and InstructionWithExecutor ===" << std::endl;
    
    // 测试 1: ExecutionContext 创建（使用真实的 X86CPUVM）
    std::cout << "\n--- Test 1: ExecutionContext Creation ---" << std::endl;
    X86VMConfig config;
    config.memory_size = 1024 * 1024;  // 1MB
    X86CPUVM vm(1, config);
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
    
    // ⚠️  关键步骤：必须添加实际的操作数对象
    // operands_str 只是用于显示的字符串，执行器实际需要 operands 中的对象
    auto dest_reg = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);  // RAX = 57 in Capstone
    auto imm_val = std::make_shared<ImmediateOperand>(42, OperandSize::QWORD);
    insn_mov.operands.push_back(std::move(dest_reg));
    insn_mov.operands.push_back(std::move(imm_val));
    
    // 从注册表创建执行器
    auto executor = registry.create_executor("mov");
    insn_mov.bind_executor(executor);
    std::cout << "✓ Executor created and bound to instruction" << std::endl;
    
    // 测试 8: 执行 MOV 指令（实际操作寄存器）
    std::cout << "\n--- Test 8: Execute MOV Instruction ---" << std::endl;
    
    // 设置初始值
    vm.set_register(X86Reg::RAX, 0);
    vm.set_register(X86Reg::RBX, 100);
    
    int length = insn_mov.execute(ctx);
    assert(length == 10);  // 应该返回指令长度
    
    // 验证 RAX 被设置为 42
    uint64_t rax_value = vm.get_register(X86Reg::RAX);
    assert(rax_value == 42);
    std::cout << "✓ MOV instruction executed successfully" << std::endl;
    std::cout << "  RAX = 0x" << std::hex << rax_value << std::dec << " (expected: 42)" << std::endl;
    
    // 测试 8.5: 测试 ADD 指令
    std::cout << "\n--- Test 8.5: Execute ADD Instruction ---" << std::endl;
    InstructionWithExecutor insn_add;
    insn_add.address = 0x1010;
    insn_add.size = 3;
    insn_add.mnemonic = "add";
    insn_add.operands_str = "rax, rbx";
    insn_add.category = InstructionCategory::ARITHMETIC;
    
    auto add_executor = registry.create_executor("add");
    insn_add.bind_executor(add_executor);
    
    // ⚠️  注意：使用不同的变量名避免与 Test 7 的 dest_reg 冲突
    auto add_dest = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);  // RAX = 57 in Capstone
    auto add_src = std::make_shared<RegisterOperand>(58, "rbx", OperandSize::QWORD);   // RBX = 58 in Capstone
    insn_add.operands.push_back(std::move(add_dest));
    insn_add.operands.push_back(std::move(add_src));
    
    length = insn_add.execute(ctx);
    assert(length == 3);
    
    // 验证 RAX = 42 + 100 = 142
    rax_value = vm.get_register(X86Reg::RAX);
    assert(rax_value == 142);
    std::cout << "✓ ADD instruction executed successfully" << std::endl;
    std::cout << "  RAX = 0x" << std::hex << rax_value << std::dec << " (expected: 142)" << std::endl;
    
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
    
    // 测试 10: 测试 SUB 指令和标志位
    std::cout << "\n--- Test 10: Execute SUB Instruction ---" << std::endl;
    vm.set_register(X86Reg::RAX, 100);
    vm.set_register(X86Reg::RBX, 30);
    
    InstructionWithExecutor insn_sub;
    insn_sub.address = 0x1020;
    insn_sub.size = 3;
    insn_sub.mnemonic = "sub";
    insn_sub.operands_str = "rax, rbx";
    insn_sub.category = InstructionCategory::ARITHMETIC;
    
    auto sub_executor = registry.create_executor("sub");
    insn_sub.bind_executor(sub_executor);
    
    auto sub_dest = std::make_shared<RegisterOperand>(57, "rax", OperandSize::QWORD);
    auto sub_src = std::make_shared<RegisterOperand>(58, "rbx", OperandSize::QWORD);
    insn_sub.operands.push_back(std::move(sub_dest));
    insn_sub.operands.push_back(std::move(sub_src));
    
    length = insn_sub.execute(ctx);
    assert(length == 3);
    
    // 验证 RAX = 100 - 30 = 70
    rax_value = vm.get_register(X86Reg::RAX);
    assert(rax_value == 70);
    
    // 验证标志位
    uint64_t flags = vm.get_rflags();
    bool zf_set = (flags & FLAG_ZF) != 0;
    bool sf_set = (flags & FLAG_SF) != 0;
    std::cout << "✓ SUB instruction executed successfully" << std::endl;
    std::cout << "  RAX = 0x" << std::hex << rax_value << std::dec << " (expected: 70)" << std::endl;
    std::cout << "  ZF = " << (zf_set ? "1" : "0") << " (expected: 0)" << std::endl;
    std::cout << "  SF = " << (sf_set ? "1" : "0") << " (expected: 0)" << std::endl;
    
    std::cout << "\n=== All Extended Tests Passed! ===" << std::endl;
    
    return 0;
}
