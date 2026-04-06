// test_assembler.cpp - 测试 Assembler 类，直接从 IR 生成机器码
#include "../include/File.h"
#include <iostream>
#include <fstream>
#include <windows.h>

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== 汇编器测试：IR → 机器码 ===" << std::endl;
    
    // 创建测试源代码文件
    std::ofstream testFile("test_source.myos");
    testFile << R"(
int main() {
    int x = 5;
    int y = 3;
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
    
    // 打印 IR
    std::cout << "\n=== IR 代码 ===" << std::endl;
    for (const auto& node : compiler.ir()) {
        std::cout << node.line << " " << (IRopTOStr.find(node.op) != IRopTOStr.end() ? IRopTOStr[node.op] : "UNKNOWN") << " ";
        for (const auto& operand : node.operands) {
            std::cout << operand << " ";
        }
        std::cout << node.comment << std::endl;
    }
    
    // 生成机器码对象文件
    std::cout << "\n=== 生成机器码对象文件 ===" << std::endl;
    bool success = compiler.compileToObjectFile("output.bin");
    
    if (success) {
        std::cout << "✓ 机器码生成成功" << std::endl;
        
        // 读取并显示文件大小
        std::ifstream inFile("output.bin", std::ios::binary | std::ios::ate);
        if (inFile.is_open()) {
            std::streamsize size = inFile.tellg();
            std::cout << "✓ 对象文件大小：" << size << " 字节" << std::endl;
            inFile.close();
        }
    } else {
        std::cout << "✗ 机器码生成失败" << std::endl;
    }
    
    // 清理
    std::remove("test_source.myos");
    std::remove("temp.txt");
    
    std::cout << "\n=== 测试完成 ===" << std::endl;
    return 0;
}
