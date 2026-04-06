// test_syscall_assembler.cpp - 测试 syscall 指令编译为机器码
#include "../include/File.h"
#include <iostream>
#include <fstream>
#include <windows.h>
#include <iomanip>

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== Syscall 汇编器测试：IR → 机器码 ===" << std::endl;
    
    // 创建测试源代码文件（包含 syscall）
    std::ofstream testFile("test_syscall_source.myos");
    testFile << R"(
int main() {
    syscall(1);
    syscall(2);
    return 0;
}
)";
    testFile.close();
    
    std::cout << "✓ 测试文件已创建 (test_syscall_source.myos)" << std::endl;
    
    // 加载并编译
    File compiler;
    compiler.load("test_syscall_source.myos");
    compiler.compile();
    
    std::cout << "✓ 编译完成（生成 IR）" << std::endl;
    
    // 打印 IR
    std::cout << "\n=== IR 代码 ===" << std::endl;
    for (const auto& node : compiler.ir()) {
        std::string opName = (IRopTOStr.find(node.op) != IRopTOStr.end()) ? IRopTOStr[node.op] : "UNKNOWN";
        std::cout << node.line << "\t" << std::setw(10) << opName << "\t";
        for (const auto& operand : node.operands) {
            std::cout << operand << " ";
        }
        if (!node.comment.empty()) {
            std::cout << "; " << node.comment;
        }
        std::cout << std::endl;
    }
    
    // 生成机器码对象文件
    std::cout << "\n=== 生成机器码对象文件 ===" << std::endl;
    bool success = compiler.compileToObjectFile("test_syscall_output.bin");
    
    if (success) {
        std::cout << "✓ 机器码生成成功" << std::endl;
        
        // 读取并显示文件大小
        std::ifstream inFile("test_syscall_output.bin", std::ios::binary | std::ios::ate);
        if (inFile.is_open()) {
            std::streamsize size = inFile.tellg();
            inFile.seekg(0, std::ios::beg);
            
            std::cout << "✓ 文件大小：" << size << " 字节" << std::endl;
            
            // 读取并显示机器码
            std::vector<uint8_t> buffer(size);
            if (inFile.read(reinterpret_cast<char*>(buffer.data()), size)) {
                std::cout << "\n=== 机器码 (十六进制) ===" << std::endl;
                for (size_t i = 0; i < size; ++i) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                              << static_cast<int>(buffer[i]) << " ";
                    if ((i + 1) % 16 == 0) {
                        std::cout << std::endl;
                    }
                }
                std::cout << std::dec << std::endl;
                
                // 查找 syscall 指令 (0F 05)
                std::cout << "\n=== 查找 syscall 指令 (0F 05) ===" << std::endl;
                bool found = false;
                for (size_t i = 0; i + 1 < size; ++i) {
                    if (buffer[i] == 0x0F && buffer[i+1] == 0x05) {
                        std::cout << "✓ 在偏移 " << i << " (0x" << std::hex << i << std::dec 
                                  << ") 处找到 syscall 指令！" << std::endl;
                        found = true;
                        
                        // 显示周围的字节
                        std::cout << "上下文：";
                        size_t start = (i > 4) ? i - 4 : 0;
                        size_t end = (i + 6 < size) ? i + 6 : size;
                        for (size_t j = start; j < end; ++j) {
                            if (j == i) {
                                std::cout << "[" << std::hex << std::setw(2) 
                                          << static_cast<int>(buffer[j]) << "] ";
                            } else {
                                std::cout << std::hex << std::setw(2) 
                                          << static_cast<int>(buffer[j]) << " ";
                            }
                        }
                        std::cout << std::dec << std::endl;
                        break;
                    }
                }
                
                if (!found) {
                    std::cout << "✗ 未找到 syscall 指令 (0F 05)" << std::endl;
                }
            }
        }
        inFile.close();
        
        std::cout << "\n✓ 测试完成！syscall 指令已成功编译为 x86-64 机器码。" << std::endl;
    } else {
        std::cout << "✗ 机器码生成失败" << std::endl;
        return 1;
    }
    
    return 0;
}
