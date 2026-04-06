// IRprinter.cpp - IR 打印器实现
#include "IRprinter.h"
#include <iostream>

IRprinter::IRprinter() {}

IRprinter::IRprinter(const std::vector<IRNode>& IRNodes) {
    this->IRNodes = IRNodes;
}

void IRprinter::print() {
    for (const IRNode& ir : IRNodes) {
        std::cout << ir.line << "\t" << IRopTOStr[ir.op] << "\t";
        
        for (const std::string& ops : ir.operands) {
            std::cout << ops << "  ";
        }
        
        std::cout << "\tcomment: " << ir.comment << std::endl;
    }
}

IRprinter::~IRprinter() {
}
