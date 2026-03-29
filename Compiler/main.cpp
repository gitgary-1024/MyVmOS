#define _DEBUG

#include <iostream>
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
        
        std::cout << "Compilation successful!" << std::endl;
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
