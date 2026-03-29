// test_ir_to_assembly.cpp - 测试 IR 到 x86 汇编代码生成
#include "../../include/IR/IRbase.h"
#include "../../include/IR/IRToAssembly.h"
#include <iostream>
#include <vector>

using namespace std;

int main() {
    system("chcp 65001 > nul");
    cout << "=== 测试 IR 到 x86 汇编代码生成 ===" << endl << endl;
    
    // 创建一些测试 IR 指令
    std::vector<IRNode> irNodes;
    
    // 示例 1：简单的加法运算
    cout << "【示例 1】简单加法：eax = 5 + 3" << endl;
    irNodes.push_back(IRNode(IROp::MOV, {"eax", "5"}, "加载立即数 5"));
    irNodes.push_back(IRNode(IROp::MOV, {"ebx", "3"}, "加载立即数 3"));
    irNodes.push_back(IRNode(IROp::ADD, {"eax", "ebx"}, "eax = eax + ebx"));
    cout << "IR 代码：" << endl;
    for (const auto& node : irNodes) {
        cout << "  " << IRopTOStr[node.op] << " ";
        for (size_t i = 0; i < node.operands.size(); i++) {
            if (i > 0) cout << ", ";
            cout << node.operands[i];
        }
        cout << endl;
    }
    
    // 转换为汇编（Intel 语法）
    IRToAssembly assembler(irNodes, {}, true, true);
    string assembly = assembler.compile();
    
    cout << "\n生成的 x86-64 汇编代码（Intel 语法）：" << endl;
    cout << assembly << endl;
    
    // 清除，准备下一个测试
    irNodes.clear();
    
    // 示例 2：条件跳转
    cout << "\n=================================\n" << endl;
    cout << "【示例 2】条件跳转：if (x == 5) goto label1" << endl;
    irNodes.push_back(IRNode(IROp::MOV, {"eax", "[x]"}, "加载变量 x"));
    irNodes.push_back(IRNode(IROp::CMP, {"eax", "5"}, "比较 x 和 5"));
    irNodes.push_back(IRNode(IROp::JE, {".L1"}, "如果相等则跳转到.L1"));
    irNodes.push_back(IRNode(IROp::LABEL, {".L1"}, "标签.L1"));
    
    cout << "IR 代码：" << endl;
    for (const auto& node : irNodes) {
        cout << "  " << IRopTOStr[node.op] << " ";
        for (size_t i = 0; i < node.operands.size(); i++) {
            if (i > 0) cout << ", ";
            cout << node.operands[i];
        }
        cout << endl;
    }
    
    // 转换为汇编
    IRToAssembly assembler2(irNodes, {}, true, true);
    string assembly2 = assembler2.compile();
    
    cout << "\n生成的 x86-64 汇编代码：" << endl;
    cout << assembly2 << endl;
    
    // 清除，准备下一个测试
    irNodes.clear();
    
    // 示例 3：函数调用
    cout << "\n=================================\n" << endl;
    cout << "【示例 3】函数调用：call func_name; ret" << endl;
    irNodes.push_back(IRNode(IROp::PUSH, {"rbp"}, "保存基址指针"));
    irNodes.push_back(IRNode(IROp::MOV, {"rbp", "rsp"}, "设置新基址"));
    irNodes.push_back(IRNode(IROp::CALL, {"func_name"}, "调用函数"));
    irNodes.push_back(IRNode(IROp::POP, {"rbp"}, "恢复基址指针"));
    irNodes.push_back(IRNode(IROp::RET, {}, "返回"));
    
    cout << "IR 代码：" << endl;
    for (const auto& node : irNodes) {
        cout << "  " << IRopTOStr[node.op] << " ";
        for (size_t i = 0; i < node.operands.size(); i++) {
            if (i > 0) cout << ", ";
            cout << node.operands[i];
        }
        cout << endl;
    }
    
    // 转换为汇编
    IRToAssembly assembler3(irNodes, {}, true, true);
    string assembly3 = assembler3.compile();
    
    cout << "\n生成的 x86-64 汇编代码：" << endl;
    cout << assembly3 << endl;
    
    // 清除，准备下一个测试
    irNodes.clear();
    
    // 示例 4：循环结构（for 循环简化版）
    cout << "\n=================================\n" << endl;
    cout << "【示例 4】for 循环结构" << endl;
    irNodes.push_back(IRNode(IROp::MOV, {"ecx", "0"}, "初始化计数器 i=0"));
    irNodes.push_back(IRNode(IROp::LABEL, {".loop_start"}, "循环开始标签"));
    irNodes.push_back(IRNode(IROp::CMP, {"ecx", "10"}, "比较 i 和 10"));
    irNodes.push_back(IRNode(IROp::JGE, {".loop_end"}, "如果 i>=10 则退出循环"));
    irNodes.push_back(IRNode(IROp::INC, {"ecx"}, "i++"));
    irNodes.push_back(IRNode(IROp::JMP, {".loop_start"}, "跳回循环开始"));
    irNodes.push_back(IRNode(IROp::LABEL, {".loop_end"}, "循环结束标签"));
    
    cout << "IR 代码：" << endl;
    for (const auto& node : irNodes) {
        cout << "  " << IRopTOStr[node.op] << " ";
        for (size_t i = 0; i < node.operands.size(); i++) {
            if (i > 0) cout << ", ";
            cout << node.operands[i];
        }
        cout << endl;
    }
    
    // 转换为汇编
    IRToAssembly assembler4(irNodes, {}, true, true);
    string assembly4 = assembler4.compile();
    
    cout << "\n生成的 x86-64 汇编代码：" << endl;
    cout << assembly4 << endl;
    
    // 清除，准备下一个测试
    irNodes.clear();
    
    // 示例 5：AT&T 语法
    cout << "\n=================================\n" << endl;
    cout << "【示例 5】AT&T 语法测试" << endl;
    irNodes.push_back(IRNode(IROp::MOV, {"eax", "42"}, "加载 42"));
    irNodes.push_back(IRNode(IROp::ADD, {"eax", "ebx"}, "相加"));
    
    // 使用 AT&T 语法
    IRToAssembly assembler5(irNodes, {}, false, true);
    string assembly5 = assembler5.compile();
    
    cout << "\n生成的 x86-64 汇编代码（AT&T 语法）：" << endl;
    cout << assembly5 << endl;
    
    cout << "\n=== 所有测试完成 ===" << endl;
    
    return 0;
}
