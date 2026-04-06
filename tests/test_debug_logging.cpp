#include "vm/X86CPU.h"
#include <iostream>
#include <vector>
#include <iomanip>

int main() {
    std::cout << "===== X86VM 调试日志功能测试 =====\n" << std::endl;
    
    // 创建 VM
    X86VMConfig config;
    config.memory_size = 1024 * 1024;  // 1MB
    config.entry_point = 0x1000;
    config.stack_pointer = 0x10000;
    
    auto vm = std::make_unique<X86CPUVM>(1, config);
    
    int count = 0;  // 在函数开头声明
    
    // 编写一个简单的循环程序（递减并跳转）
    std::vector<uint8_t> program = {
        // 0x1000: MOV RAX, 5
        0x48, 0xB8, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // 0x100A: DEC RAX
        0x48, 0xFF, 0xC8,
        
        // 0x100D: JNE 0x100A (跳转到 DEC)
        0x75, 0xF7,
        
        // 0x100F: HLT
        0xF4
    };
    
    vm->load_binary(program, config.entry_point);
    
    std::cout << "Initial RIP: 0x" << std::hex << vm->get_rip() << std::endl;
    std::cout << "Program loaded at: 0x" << std::hex << config.entry_point << std::endl;
    std::cout << "First 10 bytes: ";
    for (int i = 0; i < 10 && i < program.size(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)program[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    std::cout << "\n--- 测试 1: 关闭调试日志 ---" << std::endl;
    vm->set_debug_logging(false);
    vm->start();
    
    // 手动执行，不使用 CFG
    while (vm->get_x86_state() == X86VMState::RUNNING && count++ < 20) {
        vm->execute_instruction();
    }
    
    std::cout << "EAX = " << vm->get_register(X86Reg::RAX) << std::endl;
    vm->stop();
    vm->reset();
    
    std::cout << "\n--- 测试 2: 开启调试日志 ---" << std::endl;
    vm->reset();  // 重置 VM
    vm->set_debug_logging(true);
    vm->start();
    
    count = 0;  // 重置计数器
    while (vm->get_x86_state() == X86VMState::RUNNING && count++ < 20) {
        vm->execute_instruction();
    }
    
    std::cout << "\n最终 EAX = " << vm->get_register(X86Reg::RAX) << std::endl;
    vm->stop();
    
    std::cout << "\n===== 测试完成 =====" << std::endl;
    return 0;
}
