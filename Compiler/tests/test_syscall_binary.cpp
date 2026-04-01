#define _DEBUG

#include <iostream>
#include <iomanip>
#include "include/File.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <source_file>" << std::endl;
        return 1;
    }
    
    std::string fileName = argv[1];
    std::cout << "Compiling: " << fileName << std::endl;
    
    try {
        File compiler(fileName);
        
        std::cout << "\n===== AST Structure =====" << std::endl;
        // 打印 AST（已经在 File 内部完成）
        
        std::cout << "\n===== IR Instructions =====" << std::endl;
        for (const auto& ir : compiler.ir()) {
            std::cout << ir.operands[0];
            for (size_t i = 1; i < ir.operands.size(); ++i) {
                std::cout << ", " << ir.operands[i];
            }
            std::cout << "  ; " << ir.comment << std::endl;
        }
        
        std::cout << "\nCompilation successful!" << std::endl;
        std::cout << "Generated " << compiler.ir().size() << " IR instructions." << std::endl;
        
#ifdef _DEBUG
        std::cout << "\nDebug mode enabled." << std::endl;
#endif
        
    } catch (const std::exception& e) {
        std::cerr << "Compilation error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
