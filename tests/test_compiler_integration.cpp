// test_compiler_integration.cpp - 编译器集成测试
#include <iostream>
#include "../Compiler/include/File.h"

int main() {
    std::cout << "=== Compiler Integration Test ===" << std::endl;
    
    try {
        // 测试简单程序
        File compiler("../Compiler/test.txt");
        
        std::cout << "\n✓ Compilation successful!" << std::endl;
        std::cout << "Generated " << compiler.ir().size() << " IR instructions." << std::endl;
        
#ifdef _DEBUG
        std::cout << "\n--- Debug Information ---" << std::endl;
        std::cout << "AST and IR printed above." << std::endl;
#endif
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Compilation error: " << e.what() << std::endl;
        return 1;
    }
}
