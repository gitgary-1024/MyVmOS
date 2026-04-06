#include "vm/X86CPU.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "===== X86VM CMP 指令简单测试 =====\n" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 1024 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0x10000;
    
    auto vm = std::make_unique<X86CPUVM>(1, config);
    
    // 简单测试：使用寄存器直接赋值
    std::vector<uint8_t> program = {
        // MOV EAX, 5 (32位，自动零扩展)
        0xB8, 0x05, 0x00, 0x00, 0x00,
        
        // MOV EBX, 3
        0xBB, 0x03, 0x00, 0x00, 0x00,
        
        // CMP EAX, EBX
        0x39, 0xD8,  // CMP EAX, EBX
        
        // JNE not_equal (跳转到 0x1015)
        0x75, 0x07,  // 0x100c + 2 + 7 = 0x1015
        
        // MOV ECX, 0 (相等时执行)
        0xB9, 0x00, 0x00, 0x00, 0x00,
        
        // JMP end
        0xEB, 0x03,
        
        // not_equal: MOV ECX, 1
        0xB9, 0x01, 0x00, 0x00, 0x00,
        
        // end: HLT
        0xF4
    };
    
    vm->load_binary(program, config.entry_point);
    vm->set_debug_logging(true);
    vm->start();
    
    int count = 0;
    while (vm->get_x86_state() == X86VMState::RUNNING && count++ < 20) {
        vm->execute_instruction();
    }
    
    std::cout << "\n测试结果:" << std::endl;
    std::cout << "EAX = " << vm->get_register(X86Reg::RAX) << " (预期: 5)" << std::endl;
    std::cout << "EBX = " << vm->get_register(X86Reg::RBX) << " (预期: 3)" << std::endl;
    std::cout << "ECX = " << vm->get_register(X86Reg::RCX) << " (预期: 1, 因为 5 != 3)" << std::endl;
    
    bool pass = (vm->get_register(X86Reg::RCX) == 1);
    std::cout << (pass ? "[PASS]" : "[FAIL]") << " CMP + JNE test" << std::endl;
    
    vm->stop();
    return pass ? 0 : 1;
}
