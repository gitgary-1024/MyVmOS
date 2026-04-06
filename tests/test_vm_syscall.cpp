#include <iostream>
#include <vector>
#include "vm/X86CPU.h"

using namespace std;

// 简单的测试程序，演示 syscall(<syscall_num>) 的使用
// 关键字：syscall(1) - 系统调用号 1（例如：退出系统）

int main() {
    system("chcp 65001 > nul");
    cout << "===== x86 VM SYSCALL 测试 =====" << endl;
    
    // 创建 VM
    X86VMConfig config;
    config.memory_size = 64 * 1024 * 1024;  // 64MB
    config.entry_point = 0x100000;
    config.stack_pointer = 0x800000;
    
    X86CPUVM vm(1, config);
    
    // 编写测试程序：执行 syscall(1)
    // 汇编代码：
    //   mov rax, 1      ; 系统调用号 1 (exit)
    //   syscall         ; 触发系统调用
    
    std::vector<uint8_t> test_program = {
        // mov rax, 1    ; B8 01 00 00 00 00 00 00 00
        0xB8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // syscall       ; 0F 05 (真实 x86 syscall 指令)
        // 我们使用简化的 0x05 作为 syscall 操作码
        0x05
    };
    
    // 加载程序到 VM
    vm.load_binary(test_program, config.entry_point);
    
    // 设置初始 RIP
    vm.set_rip(config.entry_point);
    
    cout << "测试程序已加载到地址 0x" << hex << config.entry_point << dec << endl;
    cout << "程序内容：" << endl;
    cout << "  mov rax, 1   ; 设置系统调用号 1" << endl;
    cout << "  syscall      ; 触发系统调用" << endl;
    cout << endl;
    
    // 开始执行
    cout << "开始执行..." << endl;
    vm.start();
    
    // 执行指令直到 syscall 或 HALT
    int max_instructions = 10;
    for (int i = 0; i < max_instructions && vm.get_state() == VMState::RUNNING; ++i) {
        vm.execute_instruction();
        
        cout << "[Step " << (i+1) << "] RIP=0x" << hex 
             << vm.get_rip() << dec 
             << " RAX=0x" << vm.get_register(X86Reg::RAX) 
             << " State=";
        
        switch (vm.get_x86_state()) {
            case X86VMState::CREATED:
                cout << "CREATED";
                break;
            case X86VMState::RUNNING:
                cout << "RUNNING";
                break;
            case X86VMState::HALTED:
                cout << "HALTED";
                break;
            case X86VMState::WAITING_INTERRUPT:
                cout << "WAITING_INTERRUPT";
                break;
            case X86VMState::TERMINATED:
                cout << "TERMINATED";
                break;
        }
        cout << dec << endl;
    }
    
    // 最终状态
    cout << endl;
    cout << "===== 执行完成 =====" << endl;
    cout << "最终状态:" << endl;
    cout << "  RIP: 0x" << hex << vm.get_rip() << dec << endl;
    cout << "  RAX: 0x" << hex << vm.get_register(X86Reg::RAX) << dec << endl;
    cout << "  VM 状态：" << (vm.get_state() == VMState::RUNNING ? "RUNNING" : 
                              vm.get_state() == VMState::CREATED ? "CREATED" : 
                              "OTHER") << endl;
    
    vm.dump_registers();
    
    return 0;
}
