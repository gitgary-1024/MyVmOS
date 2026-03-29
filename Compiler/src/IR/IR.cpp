// IR.cpp - 中间表示生成器实现（带线性扫描寄存器分配）
#include "IR.h"
#include <stdexcept>
#include <algorithm>
#include <set>

// 初始化物理寄存器列表
const std::vector<std::string> ALL_PHYSICAL_REGS = {"eax", "ebx", "ecx", "edx"};

std::string IR::newVReg() {
    return "vreg" + std::to_string(vregCounter++);
}

std::string IR::newLab() {
    return ".L" + std::to_string(labCounter++);
}

IROp IR::opToIROp(const std::string& op) {
    if (op == "+")
        return IROp::ADD;
        
    if (op == "-")
        return IROp::SUB;
        
    if (op == "*")
        return IROp::IMUL;
        
    if (op == "/")
        return IROp::IDIV;
        
    if (op == "&&")
        return IROp::AND;
        
    if (op == "||")
        return IROp::OR;
        
    if (op == "!")
        return IROp::NOT;
        
    if (op == ">")
        return IROp::JG;  // 用于条件跳转
        
    if (op == "<")
        return IROp::JL;
        
    if (op == ">=")
        return IROp::JGE;
        
    if (op == "<=")
        return IROp::JLE;
        
    if (op == "==")
        return IROp::JE;
        
    if (op == "!=")
        return IROp::JNE;
        
    throw std::runtime_error("Unsupported operator: " + op);
}

std::string IR::generateExpression(ASTBaseNode* node) {
    if (!node)
        throw std::runtime_error("Null expression node");
        
    Expression* expr = dynamic_cast<Expression*>(node);
    
    if (!expr)
        throw std::runtime_error("Not an expression node");
        
    switch (expr->exprType) {
        case Expression::LITERAL: {
            // 加载常量到新的虚拟寄存器
            std::string vreg = newVReg();
            irNodes.push_back({IROp::MOV,
                              {vreg, expr->value},
                              std::string("Load literal: ") + expr->value});
            return vreg;
        }
        
        case Expression::IDENTIFIER: {
            // 加载变量到新的虚拟寄存器
            std::string vreg = newVReg();
            irNodes.push_back({IROp::MOV,
                              {vreg, "[" + expr->value + "]"},
                              "Load variable: " + expr->value});
            return vreg;
        }
        
        case Expression::BINARY_OPERATOR: {
            // 处理二元运算符：先处理左右操作数，再生成运算指令
            std::string leftVReg = generateExpression(expr->left);
            std::string rightVReg = generateExpression(expr->right);
            
            std::string resultVReg = newVReg();
            IROp op = opToIROp(expr->value);
            
            // 执行运算：result = left op right
            irNodes.push_back({op,
                              {resultVReg, leftVReg, rightVReg},
                              "Binary op: " + expr->value});
            return resultVReg;
        }
        
        case Expression::UNARY_OPERATOR: {
            // 处理一元运算符：先处理操作数，再生成运算指令
            std::string operandVReg = generateExpression(expr->operand);
            
            if (expr->value == "!") {
                std::string resultVReg = newVReg();
                irNodes.push_back({IROp::NOT,
                                  {resultVReg, operandVReg},
                                  "Unary op: !"});
                return resultVReg;
            }
            else if (expr->value == "++") {
                // 后置自增：直接对内存操作
                irNodes.push_back({IROp::INC,
                                  {"[" + expr->operand->value + "]"},
                                  "Increment variable: " + expr->operand->value});
                // 返回自增后的值（加载到新虚拟寄存器）
                std::string resultVReg = newVReg();
                irNodes.push_back({IROp::MOV, {resultVReg, "[" + expr->operand->value + "]"}, "Load incremented value"});
                return resultVReg;
            }
            
            return operandVReg;
        }
        
        case Expression::FUNC_CALL: {
            FunctionCall* call = dynamic_cast<FunctionCall*>(expr);
            std::vector<std::string> paramVRegs;
            
            // 步骤 1：计算所有实参表达式，得到结果虚拟寄存器
            for (auto* param : call->parameters) {
                paramVRegs.push_back(generateExpression(param));
            }
            
            // 步骤 2：按调用约定压栈（C 语言通常从右到左）
            for (auto it = paramVRegs.rbegin(); it != paramVRegs.rend(); ++it) {
                irNodes.push_back({IROp::PUSH,
                                  {*it},
                                  "Push parameter: " + *it});
            }
            
            // 步骤 3：生成函数调用指令
            irNodes.push_back({IROp::CALL,
                              {call->funcName},
                              "Call function: " + call->funcName});
            
            // 步骤 4：清理栈上的参数
            int stackSize = paramVRegs.size();
            if (stackSize > 0) {
                irNodes.push_back({IROp::ADD,
                                  {ESP, std::to_string(stackSize * 4)},
                                  "Clean stack"});
            }
            
            // 返回值在 EAX 中，移动到新的虚拟寄存器
            std::string resultVReg = newVReg();
            irNodes.push_back({IROp::MOV, {resultVReg, EAX}, "Move return value to vreg"});
            return resultVReg;
        }
        
        case Expression::ASSIGN_OPERATOR: {
            // 左操作数必须是标识符（变量）
            if (expr->left->exprType != Expression::IDENTIFIER) {
                throw std::runtime_error("Left side of assignment must be a variable");
            }
            
            std::string varName = expr->left->value;
            // 处理右表达式，得到结果虚拟寄存器
            std::string rightVReg = generateExpression(expr->right);
            
            // 生成赋值指令：将右表达式结果赋给变量
            irNodes.push_back({IROp::MOV,
                              {"[" + varName + "]", rightVReg},
                              "Assign: " + varName + " = " + rightVReg});
            
            return rightVReg;
        }
        
        default:
            throw std::runtime_error("Unsupported expression type");
    }
}

void IR::generateVarDecl(VariableDeclaration* varDecl) {
    if (varDecl->initExpr) {
        // 有初始化表达式：生成表达式并赋值给变量
        std::string initVReg = generateExpression(varDecl->initExpr);
        irNodes.push_back({IROp::MOV,
                          {"[" + varDecl->varName + "]", initVReg},
                          "Init variable: " + varDecl->varName});
    }
    else {
        // 无初始化：生成空操作标记
        irNodes.push_back({IROp::NOP,
                          {varDecl->varName},
                          "Declare variable: " + varDecl->varName});
    }
}

void IR::generateReturn(Statement* stmt) {
    if (!stmt->getAllChildren().empty()) {
        // return 带表达式：生成表达式
        std::string retVReg = generateExpression(stmt->getAllChildren()[0]);
        // 将返回值移动到 EAX
        irNodes.push_back({IROp::MOV, {EAX, retVReg}, "Move return value to EAX"});
    }
    // 生成返回指令
    irNodes.push_back({IROp::RET, {}, "Return from function"});
}

void IR::generate(ASTBaseNode* node) {
    if (!node)
        return;
        
    switch (node->getNodeType()) {
        case ASTBaseNode::STMT_BLOCK: {
            // 处理语句块中的所有子语句
            auto* block = dynamic_cast<StatementBlock*>(node);
            
            for (auto* child : block->getAllChildren()) {
                generate(child);
            }
            
            break;
        }
        
        case ASTBaseNode::STATEMENT: {
            auto* stmt = dynamic_cast<Statement*>(node);
            
            if (stmt->getStmtType() == Statement::RETURN) {
                generateReturn(stmt);
            }
            
            break;
        }
        
        case ASTBaseNode::EXPRESSION:
            // 单独的表达式语句（如函数调用语句）
            generateExpression(node);
            break;
            
        case ASTBaseNode::VAR_DECL:
            generateVarDecl(dynamic_cast<VariableDeclaration*>(node));
            break;
            
        case ASTBaseNode::IF_STATEMENT: {
            // 简化处理：仅生成条件表达式和标签标记
            auto* ifStmt = dynamic_cast<IfStatement*>(node);
            std::string condReg = generateExpression(ifStmt->condition);
            std::string thenLab = newLab();
            std::string elseLab = newLab();
            std::string endLab = newLab();
            
            // 比较条件与 0（在 x86 中，非零为真）
            irNodes.push_back({IROp::CMP, {condReg, "0"}, "Compare condition with 0"});
            // 如果等于 0（假），跳转到 else 分支
            irNodes.push_back({IROp::JE, {elseLab}, "Jump to else if false"});
            
            irNodes.push_back({IROp::LABEL, {thenLab}, "Then block start"});
            generate(ifStmt->thenBlock);
            irNodes.push_back({IROp::JMP, {endLab}, "End then block"});
            
            irNodes.push_back({IROp::LABEL, {elseLab}, "Else block start"});
            
            if (ifStmt->elseBlock) { // 存在 else
                generate(ifStmt->elseBlock);
                irNodes.push_back({IROp::LABEL, {endLab}, "If end"});
            }
            
            break;
        }
        
        case ASTBaseNode::FOR_STATEMENT: {
            // 简化处理 for 循环结构
            auto* forStmt = dynamic_cast<ForStatement*>(node);
            std::string loopLab = newLab();
            std::string endLab = newLab();
            generate(forStmt->initStmt); // 初始化
            irNodes.push_back({IROp::LABEL, {loopLab}, "Loop start"});
            
            std::string condReg = generateExpression(forStmt->condition); // 条件
            // 比较条件与 0
            irNodes.push_back({IROp::CMP, {condReg, "0"}, "Compare condition with 0"});
            // 如果条件为假（等于 0），退出循环
            irNodes.push_back({IROp::JE, {endLab}, "Exit loop if false"});
            
            generate(forStmt->body); // 循环体
            generate(forStmt->updateStmt); // 更新
            irNodes.push_back({IROp::JMP, {loopLab}, "Loop again"});
            
            irNodes.push_back({IROp::LABEL, {endLab}, "Loop end"});
            break;
        }
        
        case ASTBaseNode::FUNC_DECL: {
            auto* func = dynamic_cast<FunctionDeclaration*>(node);
            // 函数入口标签
            irNodes.push_back({IROp::LABEL,
                              {func->funcName},
                              "Function start: " + func->funcName});
            // 栈帧初始化（简化版）：保存基址寄存器，设置新栈帧
            irNodes.push_back({IROp::PUSH,
                              {EBP}, // 保存旧的基址指针
                              "Save ebp"});
            irNodes.push_back({IROp::MOV,
                              {EBP, ESP}, // 新基址指针 = 当前栈指针
                              "Let ebp = esp"});
            // 处理函数体（原有逻辑）
            generate(func->getAllChildren()[0]);
            // 恢复栈帧并返回
            irNodes.push_back({IROp::MOV, {ESP, EBP}, "Restore esp"});
            irNodes.push_back({IROp::POP, {EBP}, "Restore ebp"});
            break;
        }
        
        default:
            // 处理其他节点类型
            for (auto* child : node->getAllChildren()) {
                generate(child);
            }
    }
}

IR::IR(ASTBaseNode* ASTroot) : ASTroot(ASTroot), vregCounter(0), labCounter(0), 
                                physicalRegs(ALL_PHYSICAL_REGS), stackOffset(0) {}

std::vector<IRNode> IR::compileToIR() {
    irNodes.clear();
    vregCounter = 0;
    labCounter = 0;
    intervals.clear();
    vregToPreg.clear();
    spillMap.clear();
    stackOffset = 0;
    
    // 步骤 1：生成使用虚拟寄存器的 IR
    generate(ASTroot);
    
    // 步骤 2：活跃性分析
    analyzeLiveness();
    
    // 步骤 3：线性扫描寄存器分配
    linearScanAllocate();
    
    // 步骤 4：重写 IR，将虚拟寄存器替换为物理寄存器或栈位置
    rewriteIR();
    
    return irNodes;
}

IR::~IR() {
    ASTroot = nullptr;
}

// 活跃性分析：收集所有虚拟寄存器的生命周期
void IR::analyzeLiveness() {
    std::unordered_map<std::string, int> firstUse; // 虚拟寄存器首次出现的位置
    
    // 遍历所有 IR 指令，记录每个虚拟寄存器的首次和末次使用位置
    for (int i = 0; i < irNodes.size(); i++) {
        auto& node = irNodes[i];
        
        // 检查所有操作数
        for (const auto& operand : node.operands) {
            // 如果操作数是虚拟寄存器（以 "vreg" 开头）
            if (operand.find("vreg") == 0) {
                // 记录首次使用
                if (firstUse.find(operand) == firstUse.end()) {
                    firstUse[operand] = i;
                    // 创建新的活跃区间
                    intervals.push_back({operand, i, i});
                } else {
                    // 更新活跃区间的结束位置
                    for (auto& interval : intervals) {
                        if (interval.vreg == operand) {
                            interval.end = i;
                            break;
                        }
                    }
                }
            }
        }
        
        // 对于定义新值的指令（如 MOV vreg1, ...），需要特殊处理
        if (node.op == IROp::MOV || node.op == IROp::ADD || node.op == IROp::SUB ||
            node.op == IROp::IMUL || node.op == IROp::IDIV || node.op == IROp::NOT) {
            std::string defReg = node.operands[0];
            if (defReg.find("vreg") == 0) {
                // 这是定义点，更新或创建区间
                bool found = false;
                for (auto& interval : intervals) {
                    if (interval.vreg == defReg) {
                        interval.start = i; // 定义点作为起点
                        interval.end = std::max(interval.end, i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    intervals.push_back({defReg, i, i});
                }
                firstUse[defReg] = i;
            }
        }
    }
    
    // 按起点排序
    std::sort(intervals.begin(), intervals.end());
}

// 线性扫描寄存器分配
void IR::linearScanAllocate() {
    std::set<std::string> active; // 当前活跃的虚拟寄存器
    std::unordered_map<std::string, std::string> pregAssignment; // 物理寄存器分配
    
    for (auto& interval : intervals) {
        // 1. 移除已经过期的区间
        std::vector<std::string> toRemove;
        for (const auto& vreg : active) {
            auto it = std::find_if(intervals.begin(), intervals.end(),
                                   [&vreg](const LiveInterval& i) { return i.vreg == vreg; });
            if (it != intervals.end() && it->end < interval.start) {
                toRemove.push_back(vreg);
                // 释放物理寄存器
                if (pregAssignment.find(vreg) != pregAssignment.end()) {
                    // 物理寄存器已释放，可以重用
                }
            }
        }
        
        for (const auto& vreg : toRemove) {
            active.erase(vreg);
        }
        
        // 2. 如果有可用的物理寄存器，分配一个
        if (active.size() < physicalRegs.size()) {
            // 找一个未使用的物理寄存器
            for (const auto& preg : physicalRegs) {
                bool used = false;
                for (const auto& vreg : active) {
                    if (pregAssignment[vreg] == preg) {
                        used = true;
                        break;
                    }
                }
                if (!used) {
                    vregToPreg[interval.vreg] = preg;
                    pregAssignment[interval.vreg] = preg;
                    active.insert(interval.vreg);
                    break;
                }
            }
        }
        else {
            // 3. 没有可用寄存器，溢出当前区间
            spillAtInterval(interval);
        }
    }
}

// 溢出处理：将虚拟寄存器溢出到栈
void IR::spillAtInterval(const LiveInterval& interval) {
    // 简单策略：溢出到栈帧中的固定位置
    spillMap[interval.vreg] = stackOffset;
    stackOffset += 4; // 每个变量占 4 字节
    // 注意：这里不分配物理寄存器，在 rewriteIR 中会处理为内存访问
}

// 重写 IR：将虚拟寄存器替换为物理寄存器或栈位置
void IR::rewriteIR() {
    for (auto& node : irNodes) {
        for (auto& operand : node.operands) {
            if (operand.find("vreg") == 0) {
                // 这是一个虚拟寄存器
                if (vregToPreg.find(operand) != vregToPreg.end()) {
                    // 已分配物理寄存器
                    operand = vregToPreg[operand];
                } else if (spillMap.find(operand) != spillMap.end()) {
                    // 已溢出到栈
                    operand = "[ebp-" + std::to_string(spillMap[operand]) + "]";
                }
            }
        }
    }
}
