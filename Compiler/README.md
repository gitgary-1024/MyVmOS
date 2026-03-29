# Custom Compiler - 自定义编译器

## 📋 概述

这是一个功能完整的自定义语言编译器，支持词法分析、语法分析、IR 生成和寄存器分配。

### 特性

✅ **词法分析** - Token 识别和分类  
✅ **语法分析** - 构建抽象语法树 (AST)  
✅ **中间表示** - 生成三地址码 IR  
✅ **寄存器分配** - 线性扫描算法（4 个物理寄存器）  
✅ **溢出处理** - 自动 spill 到栈帧  
✅ **调试支持** - 打印 AST 和 IR  

---

## 🏗️ 架构设计

```
源代码 (.txt)
    ↓
格式化 (format)
    ↓
词法分析 (Token)
    ↓
语法分析 (AST)
    ↓
IR 生成 + 寄存器分配
    ↓
优化后的 IR 代码
```

### 核心组件

1. **Token.cpp/h** - 词法分析记号
2. **AST.cpp/h** - 语法分析器
3. **ASTnode.h** - AST 节点定义
4. **IR.cpp/h** - IR 生成器
5. **IRbase.h** - IR 指令和节点定义
6. **File.cpp/h** - 文件处理和编译流程控制

---

## 🔧 编译方式

### 方式 1：作为子模块编译（推荐）

在主项目根目录执行：

```bash
cd d:\ClE\debugOS\MyOS
mkdir build && cd build
cmake .. -DBUILD_COMPILER=ON
cmake --build .
```

这会创建 `compiler_lib` 静态库，可以被其他模块使用。

### 方式 2：独立编译

进入 Compiler 目录单独编译：

```bash
cd Compiler
mkdir build && cd build
cmake ..
cmake --build .
```

这会创建可执行文件 `SimpleCompiler.exe`。

---

## 📖 使用方法

### 1. 使用 File 类进行编译

```cpp
#include "File.h"

// 创建编译器实例
File compiler("source.txt");

// 编译过程自动完成
// format() -> getAllToken() -> compileAST() -> compileIR()

// 获取生成的 IR
std::vector<IRNode>& ir = compiler.ir();

// 输出 IR 指令数量
std::cout << "Generated " << ir.size() << " IR instructions." << std::endl;
```

### 2. 运行测试程序

```bash
# 运行集成测试
./bin/test_compiler_integration

# 直接编译源文件
./bin/SimpleCompiler ../Compiler/test.txt
```

### 3. 示例代码

创建 `test.txt`:

```cpp
int main() {
    int i = 0;
    int sum = 0;
    
    for (i = 0; i < 10; i++) {
        sum = sum + i;
    }
    
    return sum;
}
```

编译并查看输出：

```bash
SimpleCompiler test.txt
```

---

## 🎯 支持的语法

### 数据类型
- `int` - 整数类型

### 表达式
- 算术运算：`+`, `-`, `*`, `/`
- 比较运算：`>`, `<`, `>=`, `<=`, `==`, `!=`
- 逻辑运算：`&&`, `||`, `!`
- 自增运算：`++`

### 语句
- 变量声明：`int x;` `int x = 5;`
- 赋值语句：`x = expr;`
- 返回语句：`return expr;`
- if 语句：`if (cond) { ... } else { ... }`
- for 循环：`for (init; cond; update) { ... }`
- 函数调用：`func(args);`

### 函数
```cpp
int func(int param1, int param2) {
    return param1 + param2;
}
```

---

## 📊 IR 指令集

### 基本指令

| 指令 | 说明 | 示例 |
|------|------|------|
| `LOAD_CONST` | 加载常量 | `eax = 5` |
| `LOAD_VAR` | 加载变量 | `eax = [x]` |
| `STORE` | 存储变量 | `[x] = eax` |
| `ADD` | 加法 | `eax = ebx + ecx` |
| `SUB` | 减法 | `eax = ebx - ecx` |
| `MUL` | 乘法 | `eax = ebx * ecx` |
| `DIV` | 除法 | `eax = ebx / ecx` |

### 控制流

| 指令 | 说明 | 示例 |
|------|------|------|
| `LABEL` | 标签 | `.L0:` |
| `GOTO` | 无条件跳转 | `goto .L1` |
| `IF_GOTO` | 条件跳转 | `if eax goto .L2` |

### 函数调用

| 指令 | 说明 | 示例 |
|------|------|------|
| `CALL` | 函数调用 | `call func` |
| `RET` | 返回 | `ret eax` |
| `PUSH` | 压栈参数 | `push eax` |

---

## 🔍 寄存器分配

### 物理寄存器配置

**可用寄存器** (4 个):
- `eax` - 累加器（返回值）
- `ebx` - 基址寄存器
- `ecx` - 计数寄存器
- `edx` - 数据寄存器

**保留寄存器**:
- `ebp` - 栈基址指针
- `esp` - 栈顶指针

### 线性扫描算法

```
1. 活跃性分析 → 构建 LiveInterval
2. 按起点排序所有区间
3. 遍历区间:
   - 有空闲寄存器 → 分配
   - 无空闲寄存器 → 溢出到栈 [ebp-offset]
4. 重写 IR，替换为物理寄存器或内存地址
```

### 性能优势

对于复杂表达式 `a + b * c - d / e`:
- **旧策略** (仅 EAX/EBX): ~35 条指令
- **新策略** (4 寄存器): ~24 条指令
- **减少约 30%** 的指令数量

---

## 🛠️ 开发指南

### 添加新的运算符

1. 在 `Token.h` 中添加运算符映射：
```cpp
{"%", Operators},  // 取模
```

2. 在 `IRbase.h` 中添加 IR 操作：
```cpp
enum class IROp {
    MOD,  // 取模
    ...
};
```

3. 在 `IR.cpp` 的 `opToIROp()` 中添加映射：
```cpp
if (op == "%") return IROp::MOD;
```

4. 在 `IR.cpp` 的 `generateExpression()` 中实现代码生成：
```cpp
case Expression::BINARY_OPERATOR:
    if (expr->value == "%") {
        // 生成 MOD 指令
    }
```

### 调试技巧

启用调试模式（已默认启用）：

```cpp
#define _DEBUG  // 在 main.cpp 或编译选项中
```

会输出：
- AST 结构树
- IR 指令列表
- 寄存器分配结果

---

## 📈 扩展方向

### 短期改进
1. ✅ 已完成线性扫描寄存器分配
2. ⏳ 添加更多数据类型（float, double）
3. ⏳ 支持数组和指针
4. ⏳ 实现更多标准库函数

### 长期规划
1. ⏳ 后端代码生成（x86/RISC-V）
2. ⏳ 优化 Pass（常量传播、死代码消除）
3. ⏳ 更智能的寄存器分配（图着色）
4. ⏳ 类型检查和推断

---

## 📚 参考资料

- **编译器设计**: 《Compilers: Principles, Techniques, and Tools》（龙书）
- **寄存器分配**: 《Linear Scan Register Allocation》(Poletto & Sarkar, 1999)
- **C++ 实现**: 现代 C++17 特性

---

## 🎓 学习价值

这个项目展示了：
1. ✅ 完整编译器前端实现
2. ✅ 活跃的性分析和寄存器分配
3. ✅ 模块化设计和代码组织
4. ✅ CMake 构建系统配置
5. ✅ 单元测试和集成测试

非常适合作为：
- 编译器原理课程实践
- C++ 高级编程示例
- 系统设计学习案例

---

**状态**: ✅ 可用并已集成到主项目

**下一步**: 可以连接到后端代码生成器（如 x86 虚拟机）来执行生成的 IR 代码。
