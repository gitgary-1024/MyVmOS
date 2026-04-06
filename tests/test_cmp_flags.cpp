#include "vm/X86CPU.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "===== CMP 标志位测试 =====\n" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 1024 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0x10000;
    
    auto vm = std::make_unique<X86CPUVM>(1, config);
    
    // 手动设置寄存器
    vm->set_register(X86Reg::RAX, 5);
    vm->set_register(X86Reg::RBX, 3);
    
    std::cout << "初始状态:" << std::endl;
    std::cout << "RAX = " << vm->get_register(X86Reg::RAX) << std::endl;
    std::cout << "RBX = " << vm->get_register(X86Reg::RBX) << std::endl;
    std::cout << "RFLAGS = 0x" << std::hex << vm->get_rflags() << std::dec << std::endl;
    
    // 编写 CMP 指令
    std::vector<uint8_t> program = {
        // CMP EAX, EBX
        0x39, 0xD8
    };
    
    vm->load_binary(program, config.entry_point);
    vm->set_debug_logging(true);
    vm->start();
    
    // 执行 CMP 指令
    vm->execute_instruction();
    
    std::cout << "\n执行 CMP EAX, EBX 后:" << std::endl;
    std::cout << "RFLAGS = 0x" << std::hex << vm->get_rflags() << std::dec << std::endl;
    std::cout << "ZF = " << ((vm->get_rflags() & (1ULL << 6)) ? 1 : 0) << " (预期: 0, 因为 5 != 3)" << std::endl;
    
    bool zf_correct = ((vm->get_rflags() & (1ULL << 6)) == 0);
    std::cout << (zf_correct ? "[PASS]" : "[FAIL]") << " ZF flag test" << std::endl;
    
    vm->stop();
    return zf_correct ? 0 : 1;
}
