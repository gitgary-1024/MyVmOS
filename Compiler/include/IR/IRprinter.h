// IRprinter.h - IR 打印器声明
#ifndef IRPRINTER_H
#define IRPRINTER_H

#include <vector>
#include "./IRbase.h"

class IRprinter {
    std::vector<IRNode> IRNodes;
    
public:
    IRprinter();
    IRprinter(const std::vector<IRNode>& IRNodes);
    void print();
    ~IRprinter();
};

#endif // IRPRINTER_H
