#include "../include/vm/X86CPU.h"
#include <iostream>
#include <vector>

void test_instruction_length() {
    std::cout << "\n=== Test: Instruction Length Calculation ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 64 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0xFFFF;
    
    X86CPUVM vm(99, config);
    
    // 测试代码：MOV EAX, 10 (5 字节)
    std::vector<uint8_t> test_code = {
        0xB8, 0x0A, 0x00, 0x00, 0x00,  // mov eax, 10
    };
    
    vm.load_binary(test_code, config.entry_point);
    
    // 手动调用 decode_and_execute 并检查 RIP 变化
    uint64_t initial_rip = vm.get_rip();
    std::cout << "Initial RIP: 0x" << std::hex << initial_rip << std::dec << std::endl;
    
    int length = vm.decode_and_execute_test();
    
    uint64_t new_rip = vm.get_rip();
    std::cout << "After execute: RIP = 0x" << std::hex << new_rip << std::dec << std::endl;
    std::cout << "Returned length: " << length << std::endl;
    std::cout << "Expected length: 5" << std::endl;
    
    if (length == 5 && new_rip == initial_rip + 5) {
        std::cout << "[PASS] Instruction length is correct!" << std::endl;
    } else {
        std::cout << "[FAIL] Instruction length mismatch!" << std::endl;
    }
    
    // 检查 EAX 值
    uint64_t eax = vm.get_register(X86Reg::RAX);
    std::cout << "EAX = " << eax << " (expected 10)" << std::endl;
}

int main() {
    test_instruction_length();
    return 0;
}
