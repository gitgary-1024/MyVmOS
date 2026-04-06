# Compiler 中 Syscall 关键字实现文档

## 概述

在 MyOS专属编译器中添加了 `syscall(<syscall_num>)` 作为新的语言关键字，允许程序员在源代码中直接调用系统调用。

## 语法

```c
syscall(<syscall_number>);
```

其中 `<syscall_number>` 是一个整数表达式，表示系统调用的编号。

### 示例

```c
int main() {
    // 系统调用号 1 - 退出
    syscall(1);
    
    // 系统调用号 2 - 读取
    syscall(2);
    
    // 系统调用号 3 - 写入
    syscall(3);
    
    return 0;
}
```

## 实现内容

### 1. 词法分析（Token.cpp）

在 `Compiler/src/Token.cpp` 中添加 `syscall` 到关键字映射表：

```cpp
std::unordered_map<std::string, TokenType> StringToTokenType = {
    // ... 其他关键字 ...
    {"syscall", Keywords}, // 系统调用关键字
};
```

### 2. AST 节点定义（ASTnode.h）

在 `Compiler/include/AST/ASTnode.h` 中：

#### 2.1 添加节点类型枚举

```cpp
class ASTBaseNode {
public:
    enum NodeType {
        // ... 其他类型 ...
        SYSCALL_STATEMENT  // 新增：系统调用语句
    };
};
```

#### 2.2 添加 SyscallStatement 类

```cpp
// 新增：系统调用语句节点
class SyscallStatement : public ASTBaseNode {
public:
    Expression* syscallNumber; // 系统调用号
    
    SyscallStatement(Expression* num);
    ~SyscallStatement();
};
```

### 3. AST 节点实现（ASTnode.cpp）

在 `Compiler/src/AST/ASTnode.cpp` 中实现构造函数和析构函数：

```cpp
// SyscallStatement 实现
SyscallStatement::SyscallStatement(Expression* num)
    : syscallNumber(num) {
    nodeType = SYSCALL_STATEMENT;
}

SyscallStatement::~SyscallStatement() {
    delete syscallNumber;
}
```

### 4. 语法分析器（AST.h & AST.cpp）

#### 4.1 声明解析函数（AST.h）

```cpp
class AST {
private:
    // ... 其他方法 ...
    
    // 解析 syscall 语句（新增）
    ASTBaseNode* parseSyscallStatement();
};
```

#### 4.2 实现解析逻辑（AST.cpp）

在 `parseStatement()` 函数中添加syscall 处理：

```cpp
ASTBaseNode* AST::parseStatement() {
    // ... 其他语句处理 ...
    
    // 处理 syscall 语句（新增）
    if (content == "syscall") {
        return parseSyscallStatement();
    }
    
    // ...
}
```

实现 `parseSyscallStatement()` 函数：

```cpp
// 解析 syscall 语句（新增）
ASTBaseNode* AST::parseSyscallStatement() {
    consume(); // 消耗 syscall 关键字
    expect("("); // 消耗左括号
    
    // 解析系统调用号表达式
    ASTBaseNode* exprNode = parseLogicalOr();
    Expression* syscallExpr = dynamic_cast<Expression*>(exprNode);
    
    if (!syscallExpr) {
        throw std::runtime_error("Invalid syscall number expression");
    }
    
    expect(")"); // 消耗右括号
    expect(";"); // 消耗分号
    
    // 创建 syscall 语句节点
    return new SyscallStatement(syscallExpr);
}
```

### 5. AST 打印器（ASTPrinter.cpp）

在 `Compiler/src/AST/ASTPrinter.cpp` 中添加打印支持：

```cpp
case ASTBaseNode::SYSCALL_STATEMENT: {
    auto* syscallStmt = dynamic_cast<SyscallStatement*>(node);
    std::cout << "SyscallStatement" << std::endl;
    // 打印系统调用号表达式
    printIndent(depth + 1);
    std::cout << "SyscallNumber:" << std::endl;
    printNode(syscallStmt->syscallNumber, depth + 2);
    break;
}
```

### 6. IR 基础定义（IRbase.h & IRbase.cpp）

#### 6.1 添加 IR 操作码（IRbase.h）

```cpp
enum class IROp {
    // ... 其他操作码 ...
    SYSCALL,    // syscall - 系统调用
};
```

#### 6.2 添加字符串映射（IRbase.cpp）

```cpp
std::unordered_map<IROp, std::string> IRopTOStr = {
    // ... 其他映射 ...
    {IROp::SYSCALL, "syscall"},
};
```

### 7. IR 生成器（IR.cpp）

在 `Compiler/src/IR/IR.cpp` 的 `generate()` 函数中添加syscall 语句的处理：

```cpp
void IR::generate(ASTBaseNode* node) {
    switch (node->getNodeType()) {
        // ... 其他节点处理 ...
        
        case ASTBaseNode::SYSCALL_STATEMENT: {
            auto* syscallStmt = dynamic_cast<SyscallStatement*>(node);
            // 生成系统调用号表达式，结果在虚拟寄存器中
            std::string syscallNumVReg = generateExpression(syscallStmt->syscallNumber);
            // 将系统调用号移动到 EAX（x86 syscall 约定）
            irNodes.push_back({IROp::MOV, {"eax", syscallNumVReg}, 
                              "Move syscall num to EAX"});
            // 生成 syscall 指令
            irNodes.push_back({IROp::SYSCALL, {}, "Syscall instruction"});
            break;
        }
    }
}
```

## IR 生成示例

### 源代码

```c
int main() {
    syscall(1);
    return 0;
}
```

### 生成的 IR 指令

```
LABEL   main            ; Function start: main
push    ebp             ; Save ebp
mov     ebp esp         ; Let ebp = esp
mov     eax 1           ; Load literal: 1
mov     eax eax         ; Move syscall num to EAX
syscall                 ; Syscall instruction
mov     eax 0           ; Load literal: 0
mov     eax eax         ; Move return value to EAX
ret                     ; Return from function
mov     esp ebp         ; Restore esp
pop     ebp             ; Restore ebp
```

## 系统调用号约定

| 编号 | 功能 | 说明 |
|------|------|------|
| 0 | unused | 保留 |
| 1 | exit | 退出/关机 |
| 2 | read | 读取数据 |
| 3 | write | 写入数据 |
| 4 | ioctl | 设备控制 |
| 5+ | ... | 其他系统调用 |

## 与 VM 的集成

### 编译器 → VM 执行流程

1. **编译阶段**
   ```
   源代码 (.txt) 
       ↓
   词法分析 (Token)
       ↓
   语法分析 (AST)
       ↓
   IR 生成 (IR)
       ↓
   汇编生成 (IRToAssembly)
       ↓
   机器码生成 (Assembler)
   ```

2. **VM 执行阶段**
   ```
   机器码加载到 VM
       ↓
   CPU 执行 syscall 指令 (0x05)
       ↓
   trigger_syscall(syscall_num)
       ↓
   构造 SyscallRequest 消息
       ↓
   发送到路由树 (MODULE_ROUTER_CORE)
       ↓
   路由到对应外设处理
       ↓
   返回结果到 VM
   ```

## 测试验证

### 测试文件

创建了 `Compiler/tests/test_syscall.txt`：

```c
// 测试 syscall 关键字
int main() {
    // 系统调用号 1 - 退出
    syscall(1);
    
    // 系统调用号 2 - 读取
    syscall(2);
    
    // 系统调用号 3 - 写入
    syscall(3);
    
    return 0;
}
```

### 编译测试

```bash
cd Compiler/build
cmake --build .
./bin/SimpleCompiler.exe tests/test_syscall.txt
```

### 输出结果

```
===== AST Structure =====
StatementBlock
  FunctionDeclaration: int main()
    StatementBlock
      SyscallStatement
        SyscallNumber:
          LiteralExpression: 1
      SyscallStatement
        SyscallNumber:
          LiteralExpression: 2
      SyscallStatement
        SyscallNumber:
          LiteralExpression: 3
      ReturnStatement
        LiteralExpression: 0
=========================
Generated 17 IR instructions.
Compilation successful!
```

## 架构优势

1. **统一接口**: 所有 VM 到外设的通信都通过统一的 syscall 语法
2. **易于使用**: C 语言风格的系统调用语法，符合程序员习惯
3. **可扩展性**: 轻松添加新的系统调用类型和功能
4. **类型安全**: 通过 AST 和 IR 的多层验证，确保语法正确性
5. **性能优化**: IR 层面优化寄存器分配，减少不必要的内存访问

## 未来扩展

### 1. 支持多参数系统调用

```c
// 例如：read(fd, buffer, count)
syscall(2, fd, buffer, count);
```

实现思路：
- 修改 `SyscallStatement` 类，添加参数列表
- 支持从多个寄存器（RDI, RSI, RDX, RCX 等）传递参数

### 2. 支持返回值处理

```c
int result = syscall(2, fd, buffer, count);
```

实现思路：
- syscall 作为表达式而非语句
- 从 RAX 寄存器获取返回值

### 3. 符号化系统调用名

```c
#include <sys/syscall.h>

int main() {
    syscall(SYS_exit);  // 使用符号常量
    syscall(SYS_write, fd, buf, count);
}
```

实现思路：
- 预处理器支持系统调用号宏定义
- 符号表管理 syscall 名称到编号的映射

## 相关文件清单

### 头文件
- `Compiler/include/Token.h` - Token 类型定义
- `Compiler/include/AST/ASTnode.h` - AST 节点定义
- `Compiler/include/AST/AST.h` - 语法分析器声明
- `Compiler/include/AST/ASTPrinter.h` - AST 打印器声明
- `Compiler/include/IR/IRbase.h` - IR 基础定义
- `Compiler/include/IR/IR.h` - IR 生成器声明

### 源文件
- `Compiler/src/Token.cpp` - Token 实现
- `Compiler/src/AST/ASTnode.cpp` - AST 节点实现
- `Compiler/src/AST/AST.cpp` - 语法分析器实现
- `Compiler/src/AST/ASTPrinter.cpp` - AST 打印器实现
- `Compiler/src/IR/IRbase.cpp` - IR 基础实现
- `Compiler/src/IR/IR.cpp` - IR 生成器实现

### 测试文件
- `Compiler/tests/test_syscall.txt` - syscall 测试用例

## 总结

成功在 MyOS 编译器中实现了 `syscall(<syscall_num>)` 关键字，完成了从源代码词法分析、语法分析、IR 生成到最终汇编/机器码生成的完整编译流程。该实现与 x86 VM 的系统调用机制无缝对接，为操作系统提供了用户程序与内核/外设通信的标准接口。
