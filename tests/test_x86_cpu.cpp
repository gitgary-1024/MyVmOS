#include <iostream>
#include <vector>
#include <iomanip>
#include "X86CPU.h"

using namespace std;

void test_basic_instructions() {
    cout << "\n========================================" << endl;
    cout << "  Test 1: Basic x86 Instructions" << endl;
    cout << "========================================\n" << endl;
    
    X86VMConfig config;
    config.memory_size = 1 * 1024 * 1024;  // 1MB
    config.entry_point = 0x1000;
    config.stack_pointer = 0x8000;
    
    X86CPUVM vm(1, config);
    
    // 编写简单的机器码程序
    // 计算 RAX = 10 + 20 - 5
    vector<uint8_t> program = {
        // MOV RAX, 10 (使用立即数)
        0x48, 0xB8, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 10
        
        // MOV RBX, 20
        0x48, 0xBB, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rbx, 20
        
        // ADD RAX, RBX  (RAX = RAX + RBX = 30)
        0x48, 0x01, 0xD8,  // add rax, rbx
        
        // MOV RCX, 5
        0x48, 0xB9, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rcx, 5
        
        // SUB RAX, RCX  (RAX = RAX - RCX = 25)
        0x48, 0x29, 0xC8,  // sub rax, rcx
        
        // HLT
        0xF4  // hlt
    };
    
    vm.load_binary(program, config.entry_point);
    
    cout << "Program loaded:" << endl;
    cout << "  RAX = 10" << endl;
    cout << "  RBX = 20" << endl;
    cout << "  RAX += RBX  (RAX = 30)" << endl;
    cout << "  RCX = 5" << endl;
    cout << "  RAX -= RCX  (RAX = 25)" << endl;
    cout << "  HLT" << endl << endl;
    
    vm.start();
    
    cout << "Initial state:" << endl;
    vm.dump_registers();
    
    vm.run_loop();
    
    cout << "Final state:" << endl;
    vm.dump_registers();
    
    // 验证结果
    if (vm.get_register(X86Reg::RAX) == 25) {
        cout << "✓ Test PASSED: RAX = 25 as expected!" << endl;
    } else {
        cout << "✗ Test FAILED: RAX = " << vm.get_register(X86Reg::RAX) 
             << ", expected 25" << endl;
    }
}

void test_stack_operations() {
    cout << "\n========================================" << endl;
    cout << "  Test 2: Stack Operations" << endl;
    cout << "========================================\n" << endl;
    
    X86VMConfig config;
    config.memory_size = 1 * 1024 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0x8000;
    
    X86CPUVM vm(2, config);
    
    // PUSH/POP 测试程序
    vector<uint8_t> program = {
        // MOV RAX, 0x1234567890ABCDEF
        0x48, 0xB8, 0xEF, 0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12,
        
        // MOV RBX, 0xFEDCBA0987654321
        0x48, 0xBB, 0x21, 0x43, 0x65, 0x87, 0x09, 0xBA, 0xDC, 0xFE,
        
        // PUSH RAX
        0x50,
        
        // PUSH RBX
        0x53,
        
        // POP RAX  (应该弹出 RBX 的值)
        0x58,
        
        // POP RBX  (应该弹出原来 RAX 的值)
        0x5B,
        
        // HLT
        0xF4
    };
    
    vm.load_binary(program, config.entry_point);
    
    cout << "Program: PUSH/POP swap test" << endl;
    cout << "  RAX = 0x1234567890ABCDEF" << endl;
    cout << "  RBX = 0xFEDCBA0987654321" << endl;
    cout << "  PUSH RAX" << endl;
    cout << "  PUSH RBX" << endl;
    cout << "  POP RAX   (swap!)" << endl;
    cout << "  POP RBX   (swap!)" << endl;
    cout << "  HLT" << endl << endl;
    
    vm.start();
    vm.run_loop();
    
    cout << "Final state:" << endl;
    vm.dump_registers();
    
    uint64_t expected_rax = 0xFEDCBA0987654321ULL;
    uint64_t expected_rbx = 0x1234567890ABCDEFULL;
    
    if (vm.get_register(X86Reg::RAX) == expected_rax && 
        vm.get_register(X86Reg::RBX) == expected_rbx) {
        cout << "✓ Test PASSED: Stack operations work correctly!" << endl;
    } else {
        cout << "✗ Test FAILED!" << endl;
        cout << "  Expected RAX = 0x" << hex << expected_rax << dec << endl;
        cout << "  Expected RBX = 0x" << hex << expected_rbx << dec << endl;
        cout << "  Got RAX = 0x" << hex << vm.get_register(X86Reg::RAX) << dec << endl;
        cout << "  Got RBX = 0x" << hex << vm.get_register(X86Reg::RBX) << dec << endl;
    }
}

void test_memory_access() {
    cout << "\n========================================" << endl;
    cout << "  Test 3: Memory Access" << endl;
    cout << "========================================\n" << endl;
    
    X86VMConfig config;
    config.memory_size = 1 * 1024 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0x8000;
    
    X86CPUVM vm(3, config);
    
    // 内存访问测试
    vector<uint8_t> program = {
        // MOV [RBP-8], RAX  (使用基址 + 偏移存储)
        0x48, 0x89, 0x45, 0xF8,
        
        // MOV RAX, 0
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // MOV RAX, [RBP-8]  (从内存加载回 RAX)
        0x48, 0x8B, 0x45, 0xF8,
        
        // HLT
        0xF4
    };
    
    // 设置 RBP 作为基址指针
    vm.set_register(X86Reg::RBP, 0x6000);
    
    // 初始值
    vm.set_register(X86Reg::RAX, 0xDEADBEEFCAFEBABEULL);
    
    cout << "Program: Memory store/load test" << endl;
    cout << "  Initial RAX = 0xDEADBEEFCAFEBABE" << endl;
    cout << "  MOV [0x5000], RAX" << endl;
    cout << "  MOV RAX, 0" << endl;
    cout << "  MOV RAX, [0x5000]" << endl;
    cout << "  HLT" << endl << endl;
    
    vm.start();
    vm.run_loop();
    
    cout << "Final state:" << endl;
    vm.dump_registers();
    
    if (vm.get_register(X86Reg::RAX) == 0xDEADBEEFCAFEBABEULL) {
        cout << "✓ Test PASSED: Memory access works correctly!" << endl;
    } else {
        cout << "✗ Test FAILED: RAX = 0x" << hex 
             << vm.get_register(X86Reg::RAX) << dec
             << ", expected 0xDEADBEEFCAFEBABE" << endl;
    }
}

int main() {
    cout << "\n============================================" << endl;
    cout << "  x86 CPU VM Tests" << endl;
    cout << "============================================\n" << endl;
    
    test_basic_instructions();
    test_stack_operations();
    test_memory_access();
    
    cout << "\n============================================" << endl;
    cout << "  All tests completed!" << endl;
    cout << "============================================\n" << endl;
    
    return 0;
}
