#include "../include/vm/X86CPU.h"
#include <iostream>
#include <cassert>
#include <vector>

void test_cfg_build() {
    std::cout << "\n=== Test 1: CFG Build ===" << std::endl;
    
    // 配置 VM
    X86VMConfig config;
    config.memory_size = 64 * 1024;
    config.entry_point = 0x1000;
    
    X86CPUVM vm(1, config);
    
    // 加载简单的测试代码（包含跳转）
    std::vector<uint8_t> test_code = {
        0xB8, 0x01, 0x00, 0x00, 0x00,  // mov eax, 1
        0x83, 0xF8, 0x00,              // cmp eax, 0
        0x74, 0x05,                    // je 0x100C
        0xB8, 0x02, 0x00, 0x00, 0x00,  // mov eax, 2
        0xEB, 0x03,                    // jmp 0x1011
        0xB8, 0x03, 0x00, 0x00, 0x00,  // mov eax, 3
        0xC3                           // ret
    };
    
    vm.load_binary(test_code, config.entry_point);
    
    // 构建 CFG
    vm.build_cfg(config.entry_point);
    
    // 验证 CFG
    assert(vm.has_cfg());
    std::cout << "[PASS] CFG built successfully" << std::endl;
    
    // 打印 CFG 摘要
    vm.print_cfg_summary();
    
    // 测试查询接口
    const void* block = vm.get_basic_block_at(config.entry_point);
    if (block) {
        std::cout << "[PASS] Found basic block at entry point" << std::endl;
        
        size_t insn_count = vm.get_block_instruction_count(config.entry_point);
        std::cout << "[INFO] Block instruction count: " << insn_count << std::endl;
        
        auto successors = vm.get_block_successors(config.entry_point);
        std::cout << "[INFO] Block successors: " << successors.size() << std::endl;
        for (size_t i = 0; i < successors.size(); i++) {
            std::cout << "  - Successor " << i << ": 0x" 
                      << std::hex << successors[i] << std::dec << std::endl;
        }
    } else {
        std::cout << "[WARN] No basic block found at entry point" << std::endl;
    }
    
    // 测试跳转目标检查
    bool is_target = vm.is_jump_target(config.entry_point + 8);
    std::cout << "[INFO] Address 0x" << std::hex << (config.entry_point + 8) 
              << " is jump target: " << std::boolalpha << is_target << std::dec << std::endl;
    
    std::cout << "\n[Test 1 Complete]" << std::endl;
}

int main() {
    try {
        test_cfg_build();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }
}
