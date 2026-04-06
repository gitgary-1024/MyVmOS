# 快速开始指南

## 🎯 项目概述

本项目实现了 Linux 内核经典的 **list_head + 红黑树** 双重数据结构，用于高效管理虚拟地址与物理地址的映射。查找性能提升 **55 倍**！

## 📦 项目结构

```
mempool/
├── include/DataStructure/
│   ├── List.h           # Linux 风格双向循环链表
│   ├── RBtree.h         # 红黑树接口
│   ├── VMA.h            # 虚拟内存区域结构
│   └── MemoryManager.h  # 内存管理器（核心）
├── src/DataStructure/
│   └── RBtree.cpp       # 红黑树算法实现
├── main.cpp             # 使用案例演示（6 个示例）
├── test.cpp             # 单元测试（8 个测试）
└── CmakeLists.txt       # CMake 配置
```

## ⚡ 快速编译

### 方法 1：直接编译（推荐）

```bash
# 编译主程序
g++ -std=c++17 -I./include -o main.exe main.cpp src/DataStructure/RBtree.cpp

# 编译测试程序
g++ -std=c++17 -I./include -o test.exe test.cpp src/DataStructure/RBtree.cpp
```

### 方法 2：使用 CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 🚀 快速运行

```bash
# 运行演示程序（查看 6 个完整案例）
.\main.exe

# 运行单元测试（验证正确性）
.\test.exe
```

## 📖 核心 API（3 分钟上手）

### 1. 创建内存管理器

```cpp
#include "MemoryManager.h"

MemoryManager mm;  // 创建管理器
```

### 2. 添加虚拟内存区域

```cpp
VMA* vma = new VMA();
vma->vm_start = 0x1000;      // 起始地址
vma->vm_end = 0x2000;        // 结束地址
vma->vm_pfn = 0x100;         // 物理页帧号
vma->vm_flags = VMA_READ | VMA_WRITE;
vma->name = "my_region";

mm.add_vma(vma);  // 自动插入到红黑树和链表
```

### 3. 快速查找（O(log n)）

```cpp
VMA* result = mm.find_vma(0x1500);  // 查找包含地址 0x1500 的 VMA
if (result) {
    std::cout << "找到：0x" << result->vm_start 
              << "-0x" << result->vm_end << std::endl;
}
```

### 4. 地址转换

```cpp
uint64_t paddr = mm.virt_to_phys(0x1500);  // 虚拟→物理
std::cout << "物理地址：0x" << std::hex << paddr << std::endl;
```

### 5. 遍历所有 VMA

```cpp
mm.for_each_vma([](VMA* vma) {
    std::cout << "VMA: " << vma->name << std::endl;
});
```

## 💡 完整示例

```cpp
#include <iostream>
#include "MemoryManager.h"

int main() {
    MemoryManager mm;
    
    // 创建堆区域
    VMA* heap = new VMA();
    heap->vm_start = 0x1000000;
    heap->vm_end = 0x2000000;
    heap->vm_pfn = 0x100;
    heap->vm_flags = VMA_READ | VMA_WRITE | VMA_ANONYMOUS;
    heap->name = "[heap]";
    
    mm.add_vma(heap);
    
    // 查找
    VMA* found = mm.find_vma(0x1500000);
    if (found) {
        std::cout << "找到 VMA: " << found->name << std::endl;
        
        // 地址转换
        uint64_t phys = mm.virt_to_phys(0x1500000);
        std::cout << "物理地址：0x" << std::hex << phys << std::endl;
    }
    
    return 0;
}
```

## 🎯 性能对比

| 操作 | 红黑树 | 链表 | 加速比 |
|------|--------|------|--------|
| 查找特定地址 | O(log n) ≈ 9 次 | O(n) ≈ 500 次 | **55 倍** |
| 遍历所有 VMA | O(n) 需中序遍历 | O(n) 顺序访问 | 相当 |
| 插入/删除 | O(log n) | O(1)（已知位置） | - |

## ✅ 测试覆盖

8 个单元测试，涵盖：
- ✅ VMA 创建和销毁
- ✅ 地址查找（存在/不存在）
- ✅ 虚拟→物理地址转换
- ✅ 相邻 VMA 自动合并
- ✅ 不同权限不合并
- ✅ 链表顺序遍历
- ✅ 重叠地址检测
- ✅ 大规模 VMA 管理（1000 个）

## 📚 学习资源

1. **README.md** - 详细文档和 API 参考
2. **IMPLEMENTATION_SUMMARY.md** - 实现细节和技术亮点
3. **main.cpp** - 6 个完整的使用案例
4. **test.cpp** - 单元测试代码

## 🔧 常用 VMA 标志

```cpp
VMA_READ       // 可读
VMA_WRITE      // 可写
VMA_EXEC       // 可执行
VMA_PRIVATE    // 私有映射
VMA_SHARED     // 共享映射
VMA_ANONYMOUS  // 匿名映射（如堆、栈）
```

## 💻 输出示例

```
╔═══════════════════════════════════════════╗
║  Linux 内存管理：list_head + 红黑树      ║
╚═══════════════════════════════════════════╝

===== Virtual Memory Areas (4 VMA(s)) =====
Address Range           Flags      PFN       Name
-----------------------------------------------------------
0x400000-0x450000  r-xp        0x1000         [code]
0x450000-0x460000  rw-p        0x1050         [data]
0x1000000-0x2000000  rw-p        0x2000         [heap]
0x7fff0000-0x80000000  rw-p        0x5000         [stack]
===============================================

加速比：约 55 倍！
```

## ❓ 常见问题

**Q: 为什么需要两种数据结构？**  
A: 红黑树提供 O(log n) 快速查找，链表提供 O(n) 顺序遍历，互补优势。

**Q: 如何保证线程安全？**  
A: 当前版本未实现线程安全，可在 MemoryManager 外层加锁。

**Q: 支持大页吗？**  
A: 当前实现基于 4KB 页，可扩展支持 2MB/1GB 大页。

**Q: 可以序列化保存吗？**  
A: 可以遍历链表将所有 VMA 信息保存到文件。

## 🎓 适合人群

- 学习 Linux 内核内存管理
- 理解红黑树实际应用
- 掌握系统级编程技巧
- 准备操作系统课程设计

## 🚀 下一步

1. 阅读 `README.md` 了解详细 API
2. 运行 `main.exe` 查看演示效果
3. 修改 `main.cpp` 尝试自己的用例
4. 阅读源码深入理解实现细节

祝你学习愉快！🎉
