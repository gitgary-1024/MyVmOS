// 测试 syscall 编译为二进制代码
#include <iostream>
#include "include/File.h"

int main(int argc, char* argv[]) {
    system("chcp 65001 > nul");
    std::cout << "===== 测试 syscall 编译为二进制 =====" << std::endl;
    
    try {
        // 使用内置的测试代码
        File compiler("tests/test_syscall_bin.txt");
        
        std::cout << "\n===== 编译成功 =====" << std::endl;
        
        // 生成二进制文件
        bool result = compiler.compileToObjectFile("test_syscall_output.bin");
        
        if (result) {
            std::cout << "\n✓ syscall 指令已成功编译为 x86-64 机器码！" << std::endl;
            std::cout << "✓ 二进制文件：test_syscall_output.bin" << std::endl;
        } else {
            std::cout << "\n✗ 编译失败" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Compilation error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
