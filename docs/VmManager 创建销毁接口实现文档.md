# VmManager 创建/销毁接口实现文档

## 📖 概述

为 `VmManager` 添加了简化的 VM 创建和销毁接口，供 `RuntimeInterface` 模块使用，实现运行时的"热交互"能力。

---

## 🎯 设计目标

### 需求背景
- **RuntimeInterface** 需要一个简单的接口来动态创建/销毁 VM
- 现有的模板方法 `make_vm<T>()` 需要指定具体类型，不适合运行时动态调用
- 需要统一的返回值约定（成功返回 VM ID，失败返回 -1）

### 设计原则
```
┌─────────────────────────────────────────────┐
│  Simple Interface (简化接口)                │
├─────────────────────────────────────────────┤
│ ✅ createX86VM() - 创建 X86 虚拟机           │
│ ✅ destroyVM(int vmId) - 销毁指定虚拟机      │
│ ✅ 返回值明确 - 成功/失败清晰判断            │
│ ✅ 线程安全 - 支持并发调用                   │
└─────────────────────────────────────────────┘
```

---

## 📋 API 接口

### 1. `createX86VM()` - 创建 X86 虚拟机

```cpp
int VmManager::createX86VM();
```

**功能：**
- 创建一个新的 X86 CPU 虚拟机
- 自动注册到 VmManager 和调度器
- 返回 VM ID（从 1 开始递增）

**返回值：**
- `>= 0`: 成功，返回 VM ID
- `-1`: 失败

**示例：**
```cpp
auto& vmMgr = VmManager::instance();
int vmId = vmMgr.createX86VM();

if (vmId >= 0) {
    std::cout << "Created VM with ID: " << vmId << "\n";
} else {
    std::cerr << "Failed to create VM\n";
}
```

**实现细节：**
```cpp
int VmManager::createX86VM() {
    // 1. 生成唯一的 VM ID
    static std::atomic<uint64_t> next_vm_id{1};
    uint64_t new_vm_id = next_vm_id.fetch_add(1);
    
    // 2. 创建 X86CPUVM 实例（现在继承自 baseVM）
    auto x86_vm = std::make_shared<X86CPUVM>(new_vm_id);
    
    // 3. 注册到 VmManager
    register_vm(x86_vm);
    
    return static_cast<int>(new_vm_id);
}
```

---

### 2. `destroyVM(int vmId)` - 销毁虚拟机

```cpp
bool VmManager::destroyVM(int vmId);
```

**功能：**
- 异步销毁指定的 VM
- 从 VmManager 和调度器中注销
- 释放相关资源

**返回值：**
- `true`: 成功
- `false`: 失败（VM 不存在或参数无效）

**示例：**
```cpp
auto& vmMgr = VmManager::instance();

// 创建 VM
int vmId = vmMgr.createX86VM();

// ... 使用 VM ...

// 销毁 VM
if (vmMgr.destroyVM(vmId)) {
    std::cout << "VM destroyed successfully\n";
} else {
    std::cerr << "Failed to destroy VM\n";
}
```

**实现细节：**
```cpp
bool VmManager::destroyVM(int vmId) {
    if (vmId < 0) return false;
    
    // 检查 VM 是否存在
    auto vm = get_vm(static_cast<uint64_t>(vmId));
    if (!vm) {
        return false;
    }
    
    // 调用现有的异步销毁方法
    destroy_vm(static_cast<uint64_t>(vmId));
    
    return true;
}
```

---

## 🏗️ 架构变更

### 1. X86CPUVM 继承 baseVM

为了让 X86VM 能够被 VmManager 统一管理，修改了 `X86CPUVM` 类使其继承 `baseVM`：

```cpp
// 修改前
class X86CPUVM {
public:
    void start();
    void stop();
    int execute_instruction();
    void handle_interrupt(const InterruptResult& result);
};

// 修改后
class X86CPUVM : public baseVM {
public:
    void start() override;
    void stop() override;
    VMState get_state() const override;
    bool execute_instruction() override;  // 适配 baseVM 接口
    int execute_instruction_x86();        // x86 特定版本
    void run_loop() override;
    void load_program(const std::vector<uint32_t>& code) override;
    void handle_interrupt(const InterruptResult& result) override;
};
```

### 2. 接口适配层

在 `X86CPUVM` 中实现了 `baseVM` 的纯虚函数：

```cpp
// execute_instruction 适配
bool X86CPUVM::execute_instruction() {
    int bytes_executed = execute_instruction_x86();
    return bytes_executed > 0;  // 返回是否成功执行
}

// get_state 适配
VMState X86CPUVM::get_state() const {
    return state_ == X86VMState::RUNNING ? VMState::RUNNING : VMState::CREATED;
}
```

---

## 📁 文件变更清单

### 头文件修改

| 文件 | 修改内容 |
|------|---------|
| `include/vm/VmManager.h` | 新增 `createX86VM()` 和 `destroyVM(int)` 声明 |
| `include/vm/X86CPU.h` | 让 `X86CPUVM` 继承 `baseVM`，添加 `override` 方法 |

### 源文件修改

| 文件 | 修改内容 |
|------|---------|
| `src/vm/VmManager.cpp` | 实现 `createX86VM()` 和 `destroyVM(int)` |

### 新增测试

| 文件 | 说明 |
|------|------|
| `tests/test_vm_create_destroy.cpp` | 测试创建/销毁功能 |

---

## 🔧 使用场景

### 场景 1：RuntimeInterface 集成

```cpp
// RuntimeInterface::createVM()
int RuntimeInterface::createVM(const std::string& type, ...) {
    if (type == "X86") {
        return m_vmManager->createX86VM();
    }
    return -1;
}

// RuntimeInterface::destroyVM()
bool RuntimeInterface::destroyVM(int vmId) {
    return m_vmManager->destroyVM(vmId);
}
```

### 场景 2：交互式控制台

```bash
# 用户输入命令
runtime> vm create X86
VM created successfully. ID: 1

runtime> vm list
ID       Type        State       Instructions    Memory
-------------------------------------------------------------------
1        X86         RUNNING     0               67108864

runtime> vm destroy 1
VM 1 destroyed successfully
```

### 场景 3：自动化测试

```cpp
TEST(VmManagerTest, CreateAndDestroy) {
    auto& vmMgr = VmManager::instance();
    
    // 创建多个 VM
    std::vector<int> vmIds;
    for (int i = 0; i < 5; ++i) {
        int id = vmMgr.createX86VM();
        ASSERT_GE(id, 0);
        vmIds.push_back(id);
    }
    
    // 全部销毁
    for (int id : vmIds) {
        ASSERT_TRUE(vmMgr.destroyVM(id));
    }
}
```

---

## ⚠️ 注意事项

### 1. 异步销毁

`destroyVM()` 是**异步**执行的，不会立即释放内存：

```cpp
vmMgr.destroyVM(vmId);  // 立即返回
// VM 还在运行，稍后才会被销毁

// 如果需要等待销毁完成：
std::this_thread::sleep_for(std::chrono::milliseconds(100));
auto vm = vmMgr.get_vm(vmId);  // 此时应该为 nullptr
```

### 2. 线程安全

所有操作都是线程安全的：

```cpp
// 多线程并发创建 VM 是安全的
std::thread t1([]() {
    vmMgr.createX86VM();
});

std::thread t2([]() {
    vmMgr.createX86VM();
});

t1.join();
t2.join();
```

### 3. VM ID 唯一性

VM ID 从 1 开始递增，保证全局唯一：

```cpp
int id1 = vmMgr.createX86VM();  // 返回 1
int id2 = vmMgr.createX86VM();  // 返回 2
int id3 = vmMgr.createX86VM();  // 返回 3
```

---

## 🧪 测试验证

### 编译测试

```bash
# PowerShell
g++ -std=c++17 -I./include tests/test_vm_create_destroy.cpp \
    ./build/VmManager.o ./build/baseVM.o ./build/SchedulerManager.o \
    -o test_vm_create.exe
```

### 运行测试

```bash
./test_vm_create.exe
```

预期输出：
```
========================================
   VmManager::createX86VM() Test      
========================================

=== Test: Create and Destroy X86 VM ===
Creating X86 VM...
[VmManager] Created X86VM with ID: 1
Created X86 VM with ID: 1
VM found, type: class X86CPUVM
Destroying VM 1...
[VmManager] Destroying VM 1 asynchronously
VM 1 destroyed successfully
[VmManager] Unregistered VM 1
[VmManager] VM 1 destroyed and memory released
Verified: VM no longer exists

✓ Test passed!

========================================
   All tests completed!                
========================================
```

---

## 🎯 与 RuntimeInterface 的集成

### RuntimeInterface 中的使用

```cpp
#include "../vm/VmManager.h"

int RuntimeInterface::createVM(const std::string& type, ...) {
    if (!m_initialized || !m_vmManager) {
        return -1;
    }
    
    if (type == "X86") {
        int vmId = m_vmManager->createX86VM();
        
        if (vmId >= 0) {
            notifyStatusChange("VM_CREATED", vmId, "Type: " + type);
        }
        
        return vmId;
    }
    
    return -1;
}

bool RuntimeInterface::destroyVM(int vmId) {
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    bool success = m_vmManager->destroyVM(vmId);
    
    if (success) {
        notifyStatusChange("VM_DESTROYED", vmId, "VM destroyed");
    }
    
    return success;
}
```

---

## 📚 相关文件

- **头文件**: `include/vm/VmManager.h`
- **实现**: `src/vm/VmManager.cpp`
- **X86 实现**: `include/vm/X86CPU.h`
- **测试**: `tests/test_vm_create_destroy.cpp`
- **接口文档**: `docs/RuntimeInterface 使用指南.md`

---

## 🔮 未来扩展

### 计划功能
- [ ] 支持更多 VM 类型（RISC-V、ARM 等）
- [ ] VM 模板/克隆功能
- [ ] VM 快照/恢复
- [ ] 批量创建/销毁优化
- [ ] VM 资源限制配置

### 优化方向
- [ ] VM ID 回收机制（避免无限增长）
- [ ] 创建/销毁优先级队列（限流）
- [ ] VM 池化（复用已销毁的 VM）

---

## 📝 总结

通过添加 `createX86VM()` 和 `destroyVM(int)` 两个简化接口：

✅ **RuntimeInterface** 可以动态创建/销毁 VM  
✅ **线程安全** - 支持并发调用  
✅ **返回值明确** - 易于错误处理  
✅ **异步销毁** - 不阻塞主线程  
✅ **统一管理** - 所有 VM 都经过 VmManager  

这为实现运行时"热交互"提供了基础能力！🎉
