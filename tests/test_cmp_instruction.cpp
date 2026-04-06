#include "vm/X86CPU.h"
#include <iostream>
#include <vector>
#include <iomanip>

int main() {
    std::cout << "===== X86VM CMP 指令测试 =====\n" << std::endl;
    
    // 创建 VM
    X86VMConfig config;
    config.memory_size = 1024 * 1024;  // 1MB
    config.entry_point = 0x1000;
    config.stack_pointer = 0x10000;
    
    auto vm = std::make_unique<X86CPUVM>(1, config);
    
    // 编写测试程序：比较两个值并跳转
    std::vector<uint8_t> program = {
        // 0x1000: MOV RAX, 5
        0x48, 0xB8, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // 0x100A: MOV RBX, 3
        0x48, 0xBB, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // 0x1014: CMP RAX, RBX  (5 - 3 = 2, ZF=0)
        0x48, 0x39, 0xD8,  // CMP RAX, RBX
        
        // 0x1017: JNE not_equal_label (应该跳转，因为 5 != 3)
        0x75, 0x09,  // 跳转到 0x1017 + 2 + 9 = 0x1022
        
        // 0x1019: MOV RCX, 0 (如果相等，会执行到这里)
        0x48, 0xC7, 0xC1, 0x00, 0x00, 0x00, 0x00,
        
        // 0x1020: JMP end
        0xEB, 0x05,  // 跳转到 0x1020 + 2 + 5 = 0x1027
        
        // 0x1022 (not_equal_label): MOV RCX, 1 (不相等时执行)
        0x48, 0xC7, 0xC1, 0x01, 0x00, 0x00, 0x00,
        
        // 0x1029 (end): HLT
        0xF4
    };
    
    vm->load_binary(program, config.entry_point);
    
    std::cout << "--- 测试 1: CMP RAX(5), RBX(3) ---" << std::endl;
    vm->set_debug_logging(true);
    vm->start();
    
    int count = 0;
    while (vm->get_x86_state() == X86VMState::RUNNING && count++ < 50) {
        vm->execute_instruction();
    }
    
    std::cout << "\n最终状态:" << std::endl;
    std::cout << "RAX = " << vm->get_register(X86Reg::RAX) << " (预期: 5)" << std::endl;
    std::cout << "RBX = " << vm->get_register(X86Reg::RBX) << " (预期: 3)" << std::endl;
    std::cout << "RCX = " << vm->get_register(X86Reg::RCX) << " (预期: 1, 因为 5 != 3)" << std::endl;
    
    bool test1_pass = (vm->get_register(X86Reg::RCX) == 1);
    std::cout << (test1_pass ? "[PASS]" : "[FAIL]") << " Test 1" << std::endl;
    
    vm->stop();
    
    // 测试 2: 相等的情况
    std::cout << "\n--- 测试 2: CMP RAX(5), RBX(5) ---" << std::endl;
    vm->reset();
    
    std::vector<uint8_t> program2 = {
        // 0x1000: MOV RAX, 5
        0x48, 0xB8, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // 0x100A: MOV RBX, 5
        0x48, 0xBB, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // 0x1014: CMP RAX, RBX  (5 - 5 = 0, ZF=1)
        0x48, 0x39, 0xD8,
        
        // 0x1017: JE equal_label (应该跳转，因为 5 == 5)
        0x74, 0x09,  // 跳转到 0x1017 + 2 + 9 = 0x1022
        
        // 0x1019: MOV RCX, 0 (不相等时执行)
        0x48, 0xC7, 0xC1, 0x00, 0x00, 0x00, 0x00,
        
        // 0x1020: JMP end
        0xEB, 0x05,  // 跳转到 0x1020 + 2 + 5 = 0x1027
        
        // 0x1022 (equal_label): MOV RCX, 1 (相等时执行)
        0x48, 0xC7, 0xC1, 0x01, 0x00, 0x00, 0x00,
        
        // 0x1029 (end): HLT
        0xF4
    };
    
    vm->load_binary(program2, config.entry_point);
    vm->set_debug_logging(false);  // 关闭日志以便清晰查看结果
    vm->start();
    
    count = 0;
    while (vm->get_x86_state() == X86VMState::RUNNING && count++ < 50) {
        vm->execute_instruction();
    }
    
    std::cout << "RAX = " << vm->get_register(X86Reg::RAX) << " (预期: 5)" << std::endl;
    std::cout << "RBX = " << vm->get_register(X86Reg::RBX) << " (预期: 5)" << std::endl;
    std::cout << "RCX = " << vm->get_register(X86Reg::RCX) << " (预期: 1, 因为 5 == 5)" << std::endl;
    
    bool test2_pass = (vm->get_register(X86Reg::RCX) == 1);
    std::cout << (test2_pass ? "[PASS]" : "[FAIL]") << " Test 2" << std::endl;
    
    vm->stop();
    
    std::cout << "\n===== 测试完成 =====" << std::endl;
    std::cout << "总体结果: " << (test1_pass && test2_pass ? "[ALL PASS]" : "[SOME FAIL]") << std::endl;
    
    return (test1_pass && test2_pass) ? 0 : 1;
}
