# 快速开始 - 使用自定义编译器

## 🚀 5 分钟快速上手

### 1. 编译项目

```powershell
cd d:\ClE\debugOS\MyOS
mkdir build && cd build
cmake .. -DBUILD_COMPILER=ON
cmake --build .
```

### 2. 运行测试

```bash
# 运行集成测试
./bin/test_compiler_integration

# 查看生成的 IR
./bin/SimpleCompiler ../Compiler/test.txt
```

### 3. 编写自己的程序

创建 `myprog.txt`:

```cpp
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int result = factorial(5);
    return result;
}
```

编译它:

```bash
./bin/SimpleCompiler myprog.txt
```

---

## 💡 常用示例

### 示例 1: 循环求和

```cpp
int sum() {
    int total = 0;
    for (int i = 1; i <= 100; i++) {
        total = total + i;
    }
    return total;
}
```

### 示例 2: 条件判断

```cpp
int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}
```

### 示例 3: 复杂表达式

```cpp
int compute(int x, int y) {
    int result = (x + y) * (x - y);
    return result;
}
```

---

## 🔍 调试技巧

### 启用详细输出

在 `main.cpp` 中确保有：

```cpp
#define _DEBUG
```

编译后会输出：
- AST 结构树
- IR 指令列表
- 寄存器分配详情

### 查看中间结果

```cpp
File compiler("source.txt");

// 查看 Token 列表
auto tokens = compiler.returnAllToken();

// 查看 IR 指令
auto ir = compiler.ir();
for (auto& instr : ir) {
    std::cout << instr.toString() << std::endl;
}
```

---

## ❓ 常见问题

### Q: 编译失败 "file not found"
**A**: 检查文件路径是否正确，建议使用绝对路径。

### Q: 运行时出现段错误
**A**: 可能是程序逻辑错误，检查数组越界或无限递归。

### Q: 如何添加新特性？
**A**: 参考 `Compiler/README.md` 中的"开发指南"章节。

---

## 📚 下一步

- 阅读完整的 `Compiler/README.md`
- 查看 `编译器集成报告.md` 了解技术细节
- 尝试修改源代码添加新功能

**Happy Compiling!** 🎉
