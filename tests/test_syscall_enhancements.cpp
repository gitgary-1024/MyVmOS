#include <iostream>
#include <vector>
#include <cassert>
#include "vm/X86CPU.h"
#include "periph/TerminalManager.h"

using namespace std;

// ✅ 综合测试：syscall 参数传递、返回值处理、终端管理器

void test_syscall_with_params() {
    cout << "\n===== 测试 1: Syscall 参数传递 =====" << endl;
    
    X86VMConfig config;
    config.memory_size = 64 * 1024 * 1024;
    config.entry_point = 0x100000;
    config.stack_pointer = 0x800000;
    
    X86CPUVM vm(1, config);
    
    // 编写测试程序：syscall(1, 100, 200, 300)
    // 汇编代码：
    //   mov rax, 1      ; syscall 号 1
    //   mov rdi, 100    ; 参数 1
    //   mov rsi, 200    ; 参数 2
    //   mov rdx, 300    ; 参数 3
    //   syscall         ; 触发系统调用
    
    std::vector<uint8_t> test_program = {
        // mov rax, 1
        0xB8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // mov rdi, 100
        0xBF, 0x64, 0x00, 0x00, 0x00,
        
        // mov rsi, 200
        0xBE, 0xC8, 0x00, 0x00, 0x00,
        
        // mov rdx, 300
        0xBA, 0x2C, 0x01, 0x00, 0x00,
        
        // syscall (简化为 0x05)
        0x05,
        
        // HLT
        0xF4
    };
    
    vm.load_binary(test_program, config.entry_point);
    vm.set_rip(config.entry_point);
    
    cout << "执行 syscall(1, 100, 200, 300)..." << endl;
    
    vm.start();
    vm.run_loop();
    
    cout << "✓ Syscall 参数传递测试完成" << endl;
}

void test_syscall_return_value() {
    cout << "\n===== 测试 2: Syscall 返回值处理 =====" << endl;
    
    X86VMConfig config;
    config.memory_size = 64 * 1024 * 1024;
    config.entry_point = 0x100000;
    config.stack_pointer = 0x800000;
    
    X86CPUVM vm(2, config);
    
    // 编写测试程序
    std::vector<uint8_t> test_program = {
        // mov rax, 2      ; syscall 号 2
        0xB8, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // syscall
        0x05,
        
        // 此时 RAX 应该包含返回值
        // HLT
        0xF4
    };
    
    vm.load_binary(test_program, config.entry_point);
    vm.set_rip(config.entry_point);
    
    cout << "执行 syscall(2) 并检查返回值..." << endl;
    
    vm.start();
    vm.run_loop();
    
    uint64_t rax = vm.get_register(X86Reg::RAX);
    cout << "RAX (返回值) = " << rax << endl;
    
    // 注意：当前没有真实的 syscall handler，返回值可能为 0
    cout << "✓ Syscall 返回值处理测试完成" << endl;
}

void test_terminal_manager() {
    cout << "\n===== 测试 3: 终端管理器 =====" << endl;
    
    TerminalManager term_mgr;
    
    // 初始化终端
    int term_id = term_mgr.initialize_terminal(100);
    cout << "创建终端 ID: " << term_id << endl;
    assert(term_id > 0);
    
    // 写入输入数据
    term_mgr.write_input(term_id, "Hello from VM!");
    cout << "写入输入数据：\"Hello from VM!\"" << endl;
    
    // 检查是否有输入
    assert(term_mgr.has_input(term_id));
    cout << "✓ 检测到输入数据" << endl;
    
    // 获取输入长度
    size_t available = term_mgr.input_available(term_id);
    cout << "可用输入长度：" << available << endl;
    assert(available == 1);
    
    // 读取输入
    std::string data = term_mgr.read_input(term_id, 100, 1000);
    cout << "读取输入：\"" << data << "\"" << endl;
    assert(data == "Hello from VM!");
    
    // 写入输出数据
    term_mgr.write_output(term_id, "Output to terminal");
    cout << "写入输出数据：\"Output to terminal\"" << endl;
    
    // 读取输出
    std::string output = term_mgr.read_output(term_id);
    cout << "读取输出：\"" << output << "\"" << endl;
    assert(output == "Output to terminal");
    
    // 获取状态
    auto status = term_mgr.get_status(term_id);
    cout << "终端状态:" << endl;
    cout << "  ID: " << status.terminal_id << endl;
    cout << "  Owner VM: " << status.owner_vm_id << endl;
    cout << "  Input Queue Size: " << status.input_queue_size << endl;
    cout << "  Output Queue Size: " << status.output_queue_size << endl;
    cout << "  Active: " << (status.active ? "Yes" : "No") << endl;
    
    // 释放终端
    term_mgr.release_terminal(term_id);
    cout << "释放终端" << endl;
    
    // 验证已释放
    auto status_after = term_mgr.get_status(term_id);
    assert(!status_after.active);
    cout << "✓ 终端已成功释放" << endl;
    
    cout << "✓ 终端管理器测试完成" << endl;
}

void test_terminal_syscall_handling() {
    cout << "\n===== 测试 4: 终端 Syscall 处理 =====" << endl;
    
    TerminalManager term_mgr;
    int term_id = term_mgr.initialize_terminal(200);
    
    // 模拟 SYS_WRITE
    std::string write_data = "Test write via syscall";
    int bytes_written = term_mgr.handle_sys_write(term_id, write_data);
    cout << "SYS_WRITE: 写入 " << bytes_written << " 字节" << endl;
    assert(bytes_written == static_cast<int>(write_data.size()));
    
    // 模拟 SYS_READ
    term_mgr.write_input(term_id, "Input data");
    char buffer[100];
    int bytes_read = term_mgr.handle_sys_read(term_id, buffer, sizeof(buffer));
    cout << "SYS_READ: 读取 " << bytes_read << " 字节: \"" << buffer << "\"" << endl;
    assert(bytes_read > 0);
    
    // 模拟 SYS_POLL
    std::vector<int> terminals = {term_id};
    term_mgr.write_input(term_id, "Poll test");
    int poll_result = term_mgr.handle_sys_poll(terminals, 1000);
    cout << "SYS_POLL: 结果 = " << poll_result << endl;
    assert(poll_result == 1);  // 有数据可读
    
    term_mgr.release_terminal(term_id);
    cout << "✓ 终端 Syscall 处理测试完成" << endl;
}

int main() {
    system("chcp 65001 > nul");
    
    cout << "========================================" << endl;
    cout << "  Syscall 增强功能与终端管理器测试" << endl;
    cout << "========================================" << endl;
    
    test_syscall_with_params();
    test_syscall_return_value();
    test_terminal_manager();
    test_terminal_syscall_handling();
    
    cout << "\n========================================" << endl;
    cout << "  ✓ 所有测试完成！" << endl;
    cout << "========================================" << endl;
    
    return 0;
}
