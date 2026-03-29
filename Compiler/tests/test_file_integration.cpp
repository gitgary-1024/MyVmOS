// test_file_integration.cpp - 测试 File 类的汇编和二进制生成功能
#include "../include/File.h"
#include <iostream>
#include <fstream>
#include <windows.h>

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== File 类集成测试 ===" << std::endl;
    
    // 创建测试源代码文件
    std::ofstream testFile("test_source.myos");
    testFile << R"(
int main() {
    int x = 10;
    int y = 20;
    int result = x + y;
    return result;
}
)";
    testFile.close();
    
    std::cout << "✓ 测试文件已创建" << std::endl;
    
    // 加载并编译
    File compiler;
    compiler.load("test_source.myos");
    compiler.compile();
    
    std::cout << "✓ 编译完成（生成 IR）" << std::endl;
    
    // 生成汇编代码
    std::string assembly = compiler.compileToAssembly(true, true);
    std::cout << "\n=== 生成的汇编代码 (Intel 语法) ===" << std::endl;
    std::cout << assembly << std::endl;
    
    // 保存汇编代码到文件
    std::ofstream asmOut("output.asm");
    asmOut << assembly;
    asmOut.close();
    std::cout << "✓ 汇编代码已保存到 output.asm" << std::endl;
    
    // 生成二进制文件
    std::cout << "\n=== 尝试生成二进制文件 ===" << std::endl;
    bool success = compiler.compileToBinary("output.exe", true);
    
    if (success) {
        std::cout << "✓ 二进制文件生成成功：output.exe" << std::endl;
    } else {
        std::cout << "✗ 二进制文件生成失败" << std::endl;
        std::cout << "注意：需要安装 NASM 汇编器" << std::endl;
        std::cout << "下载地址：https://www.nasm.us/" << std::endl;
    }
    
    // 清理测试文件
    std::remove("test_source.myos");
    std::remove("temp.txt");
    
    std::cout << "\n=== 测试完成 ===" << std::endl;
    return 0;
}
