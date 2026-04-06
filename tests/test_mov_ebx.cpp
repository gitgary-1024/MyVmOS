#include "vm/X86CPU.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "===== MOV EBX, imm32 测试 =====\n" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 1024 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0x10000;
    
    auto vm = std::make_unique<X86CPUVM>(1, config);
    
    // 只测试 MOV EBX, 3
    std::vector<uint8_t> program = {
        // MOV EBX, 3 (opcode 0xBB + 4字节立即数)
        0xBB, 0x03, 0x00, 0x00, 0x00
    };
    
    vm->load_binary(program, config.entry_point);
    vm->set_debug_logging(true);
    vm->start();
    
    std::cout << "\n执行前: EBX = " << vm->get_register(X86Reg::RBX) << std::endl;
    
    // 执行 MOV 指令
    vm->execute_instruction();
    
    std::cout << "执行后: EBX = " << vm->get_register(X86Reg::RBX) << " (预期: 3)" << std::endl;
    
    bool pass = (vm->get_register(X86Reg::RBX) == 3);
    std::cout << (pass ? "[PASS]" : "[FAIL]") << " MOV EBX, 3 test" << std::endl;
    
    vm->stop();
    return pass ? 0 : 1;
}
