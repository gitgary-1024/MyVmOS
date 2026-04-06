#include "../include/vm/X86CPU.h"
#include <iostream>
#include <cassert>
#include <vector>

void test_loop_debug() {
    std::cout << "\n=== Debug: Loop Execution ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 64 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0xFFFF;
    
    X86CPUVM vm(3, config);
    
    // 加载带循环的代码
    std::vector<uint8_t> test_code = {
        0xB9, 0x05, 0x00, 0x00, 0x00,  // mov ecx, 5  (循环计数器)
        0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0  (累加器)
        
        // 循环开始 (0x100A)
        0x83, 0xC0, 0x01,              // add eax, 1
        0xFF, 0xC9,                    // dec ecx
        0x75, 0xFA,                    // jne 0x100A  (向后跳转)
        
        0xC3                           // ret
    };
    
    vm.load_binary(test_code, config.entry_point);
    // vm.build_cfg(config.entry_point);  // 暂时禁用 CFG
    
    vm.start();
    
    int steps = 0;
    while (vm.get_x86_state() == X86VMState::RUNNING && steps < 50) {
        uint64_t rip_before = vm.get_rip();
        uint64_t eax_before = vm.get_register(X86Reg::RAX);
        uint64_t ecx_before = vm.get_register(X86Reg::RCX);
        
        std::cout << "[Step " << steps << "] RIP=0x" << std::hex << rip_before << std::dec 
                  << " EAX=" << eax_before << " ECX=" << ecx_before << " -> ";
        
        try {
            bool result = vm.execute_instruction();
            std::cout << "execute_instruction returned: " << result << std::endl;
            
            uint64_t rip_after = vm.get_rip();
            uint64_t eax_after = vm.get_register(X86Reg::RAX);
            uint64_t ecx_after = vm.get_register(X86Reg::RCX);
            
            std::cout << "RIP=0x" << std::hex << rip_after << std::dec 
                      << " EAX=" << eax_after << " ECX=" << ecx_after << std::endl;
        } catch (const std::exception& e) {
            std::cout << "EXCEPTION: " << e.what() << std::endl;
            break;
        }
        
        steps++;
    }
    
    // 验证：eax 应该等于 5
    uint64_t eax = vm.get_register(X86Reg::RAX);
    std::cout << "\n[RESULT] EAX = " << eax << " (expected 5)" << std::endl;
    std::cout << "[RESULT] Total steps: " << steps << std::endl;
    
    if (eax == 5) {
        std::cout << "[PASS]" << std::endl;
    } else {
        std::cout << "[FAIL]" << std::endl;
    }
    
    vm.stop();
}

int main() {
    try {
        test_loop_debug();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }
}
