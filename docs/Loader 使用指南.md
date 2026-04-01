# Loader 使用指南

## 概述

Loader 模块提供了一个简单而强大的二进制文件加载器，用于将 `.bin` 文件正确加载到 VM 内存中。它支持：

- ✅ 从文件加载原始二进制数据
- ✅ 自定义加载地址
- ✅ 入口点设置
- ✅ 数据验证
- ✅ 详细的调试信息
- ✅ 安全的错误处理

---

## 快速开始

### 1. 基本使用

```cpp
#include "../utils/Loader/Loader.h"

int main() {
    try {
        // 加载二进制文件（默认加载到地址 0）
        auto binary = Loader::load_from_file("program.bin");
        
        // 打印加载信息
        Loader::print_info(binary);
        
        // 准备 VM 内存
        std::vector<uint8_t> vm_memory(0x10000);  // 64KB
        
        // 写入 VM 内存
        if (Loader::write_to_memory(binary, vm_memory, vm_memory.size())) {
            std::cout << "Binary loaded successfully!" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Load failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

---

### 2. 自定义加载地址

```cpp
Loader::LoadOptions options;
options.base_address = 0x1000;  // 加载到 4KB 处
options.verbose = true;          // 显示详细信息
options.verify = true;           // 启用验证

auto binary = Loader::load_from_file("kernel.bin", options);

// binary.load_address 现在是 0x1000
// binary.entry_point 也是 0x1000（默认等于加载地址）
```

---

### 3. 设置入口点

```cpp
// 加载到 0x1000，但从 0x1000 开始执行
Loader::LoadOptions options;
options.base_address = 0x1000;

auto binary = Loader::load_from_file("bootloader.bin", options);

// 如果需要不同的入口点，可以手动设置
binary.entry_point = 0x1000;  // 或者任何你想要的入口地址
```

---

### 4. 集成到 VM

#### 示例：mVM 加载程序

```cpp
#include "../vm/mVM.h"
#include "../utils/Loader/Loader.h"

std::shared_ptr<mVM> create_vm_with_program(const std::string& bin_file) {
    // 创建 VM
    auto vm = std::make_shared<mVM>(1);
    
    // 加载二进制文件
    Loader::LoadOptions options;
    options.base_address = 0;  // 从头开始加载
    auto binary = Loader::load_from_file(bin_file, options);
    
    // 准备内存
    std::vector<uint32_t> vm_memory((binary.load_address + binary.data.size() + 3) / 4);
    
    // 转换并写入内存（假设是字节对齐的）
    for (size_t i = 0; i < binary.data.size(); ++i) {
        size_t word_index = (binary.load_address + i) / 4;
        size_t byte_offset = (binary.load_address + i) % 4;
        vm_memory[word_index] |= (binary.data[i] << (byte_offset * 8));
    }
    
    // 加载到 VM
    vm->load_program(vm_memory);
    
    return vm;
}
```

#### 示例：X86CPU VM 加载程序

```cpp
#include "../vm/X86CPU.h"
#include "../utils/Loader/Loader.h"

void load_program_to_x86vm(std::shared_ptr<X86CPUVM> vm, const std::string& bin_file) {
    // 加载二进制
    Loader::LoadOptions options;
    options.base_address = 0x7C00;  // 典型的 bootloader 加载地址
    options.verbose = true;
    
    auto binary = Loader::load_from_file(bin_file, options);
    
    // 直接写入 VM 的物理内存
    vm->load_binary(binary.data, binary.load_address);
    
    // 设置 RIP（指令指针）到入口点
    vm->set_rip(binary.entry_point);
}
```

---

## API 参考

### 数据结构

#### BinaryFormat

```cpp
struct BinaryFormat {
    std::vector<uint8_t> data;      // 二进制数据
    uint64_t load_address = 0;      // 加载地址
    uint64_t entry_point = 0;       // 入口点
    size_t file_size = 0;           // 文件大小
    std::string source_file;        // 源文件名
};
```

#### LoadOptions

```cpp
struct LoadOptions {
    uint64_t base_address = 0;   // 基础地址（覆盖默认加载地址）
    bool verbose = false;         // 详细输出
    bool verify = true;           // 验证加载
    size_t max_size = 0;          // 最大加载大小（0=不限制）
};
```

---

### 公开方法

#### load_from_file()

```cpp
// 完整版本
static BinaryFormat load_from_file(const std::string& filename, 
                                   const LoadOptions& options);

// 简化版本（使用默认选项）
static BinaryFormat load_from_file(const std::string& filename);
```

**参数**:
- `filename`: .bin 文件路径
- `options`: 加载选项

**返回**: BinaryFormat 结构体

**异常**:
- `std::runtime_error`: 文件不存在、文件为空、文件太大
- `std::invalid_argument`: 无效的参数

---

#### load_from_memory()

```cpp
static BinaryFormat load_from_memory(const uint8_t* data, size_t size, 
                                     uint64_t load_addr = 0, 
                                     uint64_t entry = 0);
```

**参数**:
- `data`: 二进制数据指针
- `size`: 数据大小
- `load_addr`: 加载地址（默认 0）
- `entry`: 入口点（默认等于 load_addr）

**返回**: BinaryFormat 结构体

---

#### write_to_memory()

```cpp
template<typename MemoryType>
static bool write_to_memory(const BinaryFormat& binary, 
                           MemoryType& dest_memory, 
                           size_t memory_size);
```

**参数**:
- `binary`: 已加载的二进制
- `dest_memory`: 目标内存（可以是 `std::vector<uint8_t>` 或其他容器）
- `memory_size`: 目标内存大小

**返回**: 
- `true`: 成功写入
- `false`: 失败（二进制太大）

---

#### print_info()

```cpp
static void print_info(const BinaryFormat& binary);
```

打印详细的加载信息，包括：
- 源文件名
- 文件大小
- 加载地址
- 入口点
- 数据范围
- 前 16 字节内容

---

#### verify()

```cpp
static bool verify(const BinaryFormat& binary);
```

验证二进制数据的有效性：
- 检查是否为空
- 检查是否全为 0
- 计算校验和

**返回**: 
- `true`: 验证通过
- `false`: 验证失败

---

## 高级用法

### 1. 批量加载多个程序

```cpp
std::vector<std::pair<std::string, uint64_t>> programs = {
    {"bootloader.bin", 0x7C00},
    {"kernel.bin", 0x1000},
    {"init.bin", 0x5000}
};

std::vector<Loader::BinaryFormat> binaries;

for (const auto& [file, address] : programs) {
    Loader::LoadOptions options;
    options.base_address = address;
    options.verbose = true;
    
    binaries.push_back(Loader::load_from_file(file, options));
}
```

---

### 2. 内存对齐加载

```cpp
constexpr size_t ALIGN_SIZE = 4096;  // 4KB 对齐

uint64_t align_address(uint64_t addr) {
    return (addr + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1);
}

Loader::LoadOptions options;
options.base_address = align_address(0x1234);  // 对齐到 0x1000

auto binary = Loader::load_from_file("aligned_program.bin", options);
```

---

### 3. 带偏移量的加载

```cpp
// 先加载到临时位置
auto binary = Loader::load_from_file("program.bin");

// 然后复制到最终位置
std::vector<uint8_t> vm_memory(0x10000);
uint64_t final_address = 0x8000;

// 手动复制（绕过 write_to_memory）
for (size_t i = 0; i < binary.data.size(); ++i) {
    vm_memory[final_address + i] = binary.data[i];
}
```

---

### 4. 自定义验证逻辑

```cpp
// 扩展 Loader 类添加自定义验证
class CustomLoader : public Loader {
public:
    static bool verify_with_header(const BinaryFormat& binary) {
        // 检查基本的验证
        if (!verify(binary)) {
            return false;
        }
        
        // 自定义头部验证
        if (binary.data.size() < 16) {
            return false;
        }
        
        // 检查魔数（假设前 4 字节是魔数）
        uint32_t magic = *reinterpret_cast<const uint32_t*>(binary.data.data());
        return (magic == 0xDEADBEEF);
    }
};
```

---

## 错误处理

### 常见错误及解决方案

#### 1. 文件不存在

```cpp
try {
    auto binary = Loader::load_from_file("missing.bin");
} catch (const std::runtime_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    // 输出：[Loader] Cannot open file: missing.bin
}
```

**解决**: 确保文件路径正确，使用绝对路径或检查相对路径的工作目录。

---

#### 2. 二进制太大

```cpp
std::vector<uint8_t> small_memory(100);  // 只有 100 字节

Loader::BinaryFormat binary;
binary.data.resize(200);  // 200 字节
binary.load_address = 0;

bool success = Loader::write_to_memory(binary, small_memory, small_memory.size());
if (!success) {
    std::cerr << "Binary too large!" << std::endl;
}
```

**解决**: 
- 增大数据内存
- 减小程序大小
- 分段加载

---

#### 3. 验证失败

```cpp
Loader::LoadOptions options;
options.verify = true;

auto binary = Loader::load_from_file("corrupted.bin", options);
// 输出：[Loader] Warning: Binary verification failed!
```

**解决**: 
- 检查文件是否损坏
- 确认编译过程正确
- 禁用验证（不推荐）：`options.verify = false`

---

## 性能优化

### 1. 减少文件 I/O

```cpp
// 缓存已加载的二进制
static std::unordered_map<std::string, Loader::BinaryFormat> binary_cache;

Loader::BinaryFormat load_cached(const std::string& filename) {
    if (binary_cache.find(filename) == binary_cache.end()) {
        binary_cache[filename] = Loader::load_from_file(filename);
    }
    return binary_cache[filename];
}
```

---

### 2. 零拷贝加载

对于小型二进制，可以直接使用 `load_from_memory()` 避免额外的内存分配：

```cpp
extern const uint8_t embedded_program[];
extern const size_t embedded_program_size;

auto binary = Loader::load_from_memory(embedded_program, embedded_program_size, 0x1000);
```

---

## 实际应用示例

### Bootloader 加载

```cpp
void boot_system() {
    auto vm = std::make_shared<X86CPUVM>(1);
    
    // 1. 加载 bootloader 到 0x7C00
    auto bootloader = Loader::load_from_file("bootloader.bin");
    vm->load_binary(bootloader.data, 0x7C00);
    
    // 2. 加载 kernel 到更高地址
    Loader::LoadOptions kernel_opts;
    kernel_opts.base_address = 0x1000;
    auto kernel = Loader::load_from_file("kernel.bin", kernel_opts);
    vm->load_binary(kernel.data, 0x1000);
    
    // 3. 设置入口点
    vm->set_rip(0x7C00);
    
    // 4. 启动 VM
    vm->start();
}
```

---

### 用户程序加载器

```cpp
class ProgramLoader {
public:
    static int load_and_run(std::shared_ptr<mVM> vm, const std::string& program) {
        try {
            // 加载程序
            auto binary = Loader::load_from_file(program);
            
            // 转换为 VM 内存格式
            std::vector<uint32_t> mem((binary.data.size() + 3) / 4);
            std::memcpy(mem.data(), binary.data.data(), binary.data.size());
            
            // 加载到 VM
            vm->load_program(mem);
            
            // 启动执行
            vm->start();
            
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load program: " << e.what() << std::endl;
            return -1;
        }
    }
};
```

---

## 调试技巧

### 1. 启用详细输出

```cpp
Loader::LoadOptions options;
options.verbose = true;

auto binary = Loader::load_from_file("program.bin", options);
// 输出完整的加载信息
```

---

### 2. 检查内存布局

```cpp
auto binary = Loader::load_from_file("program.bin");

std::cout << "Binary layout:" << std::endl;
std::cout << "  Start: 0x" << std::hex << binary.load_address << std::endl;
std::cout << "  End:   0x" << (binary.load_address + binary.file_size) << std::endl;
std::cout << "  Size:  " << std::dec << binary.file_size << " bytes" << std::endl;

// 打印内存映射
for (size_t i = 0; i < std::min(size_t(32), binary.file_size); ++i) {
    if (i % 16 == 0) {
        std::cout << "\n0x" << std::hex << (binary.load_address + i) << ": ";
    }
    printf("%02X ", binary.data[i]);
}
std::cout << std::endl;
```

---

## 总结

Loader 模块提供了：

✅ **简单易用的 API** - 一行代码即可加载二进制文件  
✅ **灵活的配置选项** - 自定义加载地址、验证策略  
✅ **安全的数据保护** - 边界检查、错误处理  
✅ **详细的调试信息** - 便于问题排查  
✅ **高效的内存操作** - 模板化的内存写入  

这使得将 `.bin` 文件加载到 VM 变得非常简单和安全！
