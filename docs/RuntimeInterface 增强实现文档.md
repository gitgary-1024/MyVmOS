# RuntimeInterface 增强实现文档

## 概述

本次实现完善了 `RuntimeInterface` 的核心功能，包括真实的 VM 状态获取、增强的参数验证和错误处理机制。

---

## 实现内容

### 1. `getVMStatus()` 真实逻辑实现

#### 功能描述
从 `VmManager` 获取 VM 的真实运行状态，包括执行统计、调度信息等。

#### 实现细节

```cpp
RuntimeInterface::VMStatus RuntimeInterface::getVMStatus(int vmId) {
    // 1. 检查初始化状态
    if (!m_initialized || !m_vmManager) {
        status.state = "ERROR";
        status.error_message = "Runtime interface not initialized";
        return status;
    }
    
    // 2. 验证 VM ID
    if (vmId < 0) {
        status.state = "ERROR";
        status.error_message = "Invalid VM ID: must be >= 0";
        return status;
    }
    
    // 3. 检查 VM 是否存在
    auto vm = m_vmManager->get_vm(vmId);
    if (!vm) {
        status.state = "NOT_FOUND";
        status.error_message = "VM does not exist";
        return status;
    }
    
    // 4. 获取真实状态
    status.id = vmId;
    status.type = "X86";  // 当前仅支持 X86
    
    // 5. VM 状态映射
    VMState state = vm->get_state();
    switch (state) {
        case VMState::CREATED:
            status.state = "CREATED";
            break;
        case VMState::RUNNING:
            status.state = "RUNNING";
            break;
        case VMState::SUSPENDED_WAITING_INTERRUPT:
            status.state = "WAITING_INTERRUPT";
            break;
        case VMState::TERMINATED:
            status.state = "TERMINATED";
            break;
        default:
            status.state = "UNKNOWN";
            break;
    }
    
    // 6. 统计信息
    const VMScheduleCtx& ctx = vm->get_sched_ctx();
    status.instructions = ctx.instructions_executed;
    status.priority = ctx.priority;
    status.has_quota = ctx.has_quota;
    status.blocked_reason = ctx.blocked_reason;
    
    return status;
}
```

#### 支持的 VM 状态

| 状态 | 说明 |
|------|------|
| CREATED | VM 已创建但未启动 |
| RUNNING | VM 正在运行 |
| WAITING_INTERRUPT | VM 暂停等待中断响应 |
| TERMINATED | VM 已终止 |
| ERROR | 发生错误 |
| NOT_FOUND | VM 不存在 |

#### 返回的统计信息

- **指令计数** (`instructions`): VM 已执行的指令总数
- **CPU 周期** (`cycles`): CPU 周期数（预留）
- **内存使用** (`memory_used`): VM 使用的内存字节数（预留）
- **外设数量** (`peripheral_count`): 绑定的外设数量（预留）
- **优先级** (`priority`): VM 调度优先级
- **配额状态** (`has_quota`): 是否有运行配额
- **阻塞原因** (`blocked_reason`): 0=无阻塞，1=等待外设，2=等待中断

---

### 2. `destroyVM()` 增强参数验证

#### 实现细节

```cpp
bool RuntimeInterface::destroyVM(int vmId) {
    // 1. 基础检查
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    // 2. 验证 VM ID 有效性
    if (vmId < 0) {
        notifyStatusChange("VM_DESTROY_FAILED", vmId, "Invalid VM ID: must be >= 0");
        return false;
    }
    
    // 3. 检查 VM 是否存在
    auto vm = m_vmManager->get_vm(vmId);
    if (!vm) {
        notifyStatusChange("VM_DESTROY_FAILED", vmId, "VM does not exist");
        return false;
    }
    
    // 4. 检查 VM 状态（不能销毁运行中的 VM）
    VMState state = vm->get_state();
    if (state == VMState::RUNNING) {
        notifyStatusChange("VM_DESTROY_FAILED", vmId, "Cannot destroy running VM, stop it first");
        return false;
    }
    
    // 5. 执行销毁
    bool success = m_vmManager->destroyVM(vmId);
    
    if (success) {
        notifyStatusChange("VM_DESTROYED", vmId, "VM destroyed successfully");
    } else {
        notifyStatusChange("VM_DESTROY_FAILED", vmId, "Failed to destroy VM");
    }
    
    return success;
}
```

#### 验证规则

1. **运行时初始化检查**: 确保 `RuntimeInterface` 和 `VmManager` 已初始化
2. **VM ID 范围验证**: ID 必须 >= 0
3. **VM 存在性检查**: VM 必须在系统中存在
4. **VM 状态检查**: 只能销毁非运行状态的 VM（必须先停止）

#### 错误处理

| 错误场景 | 返回值 | 通知事件 | 详细说明 |
|---------|--------|----------|----------|
| 未初始化 | false | - | Runtime interface not initialized |
| 无效 ID | false | VM_DESTROY_FAILED | Invalid VM ID: must be >= 0 |
| VM 不存在 | false | VM_DESTROY_FAILED | VM does not exist |
| VM 正在运行 | false | VM_DESTROY_FAILED | Cannot destroy running VM, stop it first |

---

### 3. `stopVM()` 增强参数验证

#### 实现细节

```cpp
bool RuntimeInterface::stopVM(int vmId) {
    // 1. 基础检查
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    // 2. 验证 VM ID 有效性
    if (vmId < 0) {
        notifyStatusChange("VM_STOP_FAILED", vmId, "Invalid VM ID: must be >= 0");
        return false;
    }
    
    // 3. 检查 VM 是否存在
    auto vm = m_vmManager->get_vm(vmId);
    if (!vm) {
        notifyStatusChange("VM_STOP_FAILED", vmId, "VM does not exist");
        return false;
    }
    
    // 4. 检查 VM 状态（只能停止运行中的 VM）
    VMState state = vm->get_state();
    if (state != VMState::RUNNING) {
        std::string errorMsg = "VM is not running (current state: ";
        switch (state) {
            case VMState::CREATED:
                errorMsg += "CREATED";
                break;
            case VMState::SUSPENDED_WAITING_INTERRUPT:
                errorMsg += "WAITING_INTERRUPT";
                break;
            case VMState::TERMINATED:
                errorMsg += "TERMINATED";
                break;
            default:
                errorMsg += "UNKNOWN";
                break;
        }
        errorMsg += ")";
        notifyStatusChange("VM_STOP_FAILED", vmId, errorMsg);
        return false;
    }
    
    // 5. 调用 VM 的 stop 方法
    vm->stop();
    
    notifyStatusChange("VM_STOPPED", vmId, "VM stopped successfully");
    return true;
}
```

#### 验证规则

1. **运行时初始化检查**: 确保 `RuntimeInterface` 和 `VmManager` 已初始化
2. **VM ID 范围验证**: ID 必须 >= 0
3. **VM 存在性检查**: VM 必须在系统中存在
4. **VM 状态检查**: 只能停止处于 RUNNING 状态的 VM

#### 错误处理

| 错误场景 | 返回值 | 通知事件 | 详细说明 |
|---------|--------|----------|----------|
| 未初始化 | false | - | Runtime interface not initialized |
| 无效 ID | false | VM_STOP_FAILED | Invalid VM ID: must be >= 0 |
| VM 不存在 | false | VM_STOP_FAILED | VM does not exist |
| VM 未运行 | false | VM_STOP_FAILED | VM is not running (current state: XXX) |

---

## 数据结构更新

### VMStatus 结构体增强

```cpp
struct VMStatus {
    int id;                              // VM ID
    std::string type;                    // VM 类型（X86、RISCV 等）
    std::string state;                   // VM 状态
    uint64_t instructions;               // 执行指令数
    uint64_t cycles;                     // CPU 周期数
    size_t memory_used;                  // 内存使用（字节）
    int peripheral_count;                // 绑定外设数量
    
    // 新增：调度信息
    int priority = 0;                    // 优先级（0=普通，1=高优，-1=低优）
    bool has_quota = false;              // 是否有运行名额
    int blocked_reason = 0;              // 阻塞原因（0=无，1=等待外设，2=等待中断）
    
    // 新增：错误信息
    std::string error_message;           // 当状态为 ERROR 或 NOT_FOUND 时的详细信息
};
```

---

## 头文件依赖管理

### 解决循环依赖问题

由于 `baseVM.h` 和 `VmManager.h` 之间存在循环依赖，在 `RuntimeInterface.cpp` 中需要特别注意包含顺序：

```cpp
// 正确顺序：先包含 baseVM.h，再包含 RuntimeInterface.h
#include "../vm/baseVM.h"      // 提供 VMState 和 VMScheduleCtx
#include "../utils/RuntimeInterface.h"  // 通过 VmManager.h 间接需要 baseVM
```

---

## 测试验证

### 测试覆盖

所有测试均通过，包括：

1. ✅ **初始化和关闭**
2. ✅ **VM 创建和销毁**（带状态验证）
3. ✅ **命令行接口**
4. ✅ **状态回调机制**
5. ✅ **并发访问**（5 个 VM 并发创建/销毁）
6. ✅ **错误处理**（负 ID、无效命令等）
7. ✅ **系统状态查询**

### 测试结果示例

```
=== Test 6: Error Handling ===
getVMStatus(-1): state = ERROR (expected: ERROR)
writeRegister(-1, ...): handled gracefully
stopVM(-1): false (expected: false)
Execute invalid command: returned output
✓ Error handling tests completed
```

---

## 使用示例

### 1. 查询 VM 状态

```cpp
auto& runtime = RuntimeInterface::getInstance();
auto status = runtime.getVMStatus(vmId);

std::cout << "VM " << status.id << " Status:\n";
std::cout << "  Type: " << status.type << "\n";
std::cout << "  State: " << status.state << "\n";
std::cout << "  Instructions: " << status.instructions << "\n";
std::cout << "  Priority: " << status.priority << "\n";
std::cout << "  Has Quota: " << (status.has_quota ? "Yes" : "No") << "\n";

if (status.state == "ERROR" || status.state == "NOT_FOUND") {
    std::cerr << "Error: " << status.error_message << "\n";
}
```

### 2. 安全销毁 VM

```cpp
// 先停止 VM
if (!runtime.stopVM(vmId)) {
    std::cerr << "Failed to stop VM\n";
    return;
}

// 再销毁 VM
if (!runtime.destroyVM(vmId)) {
    std::cerr << "Failed to destroy VM\n";
    return;
}
```

### 3. 批量查询所有 VM 状态

```cpp
auto vms = runtime.getAllVMsStatus();
for (const auto& vm : vms) {
    std::cout << "VM " << vm.id 
              << ": " << vm.state 
              << " (Instructions: " << vm.instructions << ")\n";
}
```

---

## 性能优化建议

### 1. 缓存策略

对于频繁查询的 VM 状态，可以考虑添加本地缓存：

```cpp
std::unordered_map<int, VMStatus> status_cache_;
std::chrono::steady_clock::time_point cache_time_;
constexpr auto CACHE_VALID_MS = 100ms;  // 缓存有效期
```

### 2. 批量查询优化

`getAllVMsStatus()` 可以一次性获取所有 VM 状态，避免多次锁竞争。

### 3. 异步状态更新

使用事件驱动方式，仅在 VM 状态变化时更新缓存。

---

## 未来扩展方向

### 1. 更多统计信息

- 内存使用详情（堆、栈大小）
- I/O 操作统计
- 中断处理延迟
- 调度等待时间

### 2. 控制命令增强

- `pauseVM()`: 暂停 VM（保留状态）
- `resumeVM()`: 恢复暂停的 VM
- `resetVM()`: 重置 VM 到初始状态

### 3. 监控告警

- CPU 使用率超阈值告警
- 内存泄漏检测
- 死锁检测

### 4. 持久化

- VM 状态快照
- 执行日志记录
- 性能历史数据

---

## 注意事项

### 1. 线程安全

- 所有 VM 操作通过 `VmManager` 的内部锁保护
- 避免在持有锁的情况下调用外部回调

### 2. 资源管理

- 销毁 VM 前必须先停止
- 确保释放所有相关资源（内存、外设绑定）

### 3. 错误处理

- 始终检查返回值
- 使用 `error_message` 字段获取详细错误信息
- 通过回调机制记录关键事件

---

## 相关文件

- **头文件**: `include/utils/RuntimeInterface.h`
- **实现**: `src/utils/RuntimeInterface.cpp`
- **依赖**: 
  - `include/vm/baseVM.h` (VMState, VMScheduleCtx)
  - `include/vm/VmManager.h` (VM 管理)
  - `include/periph/PeriphManager.h` (外设管理)

---

## 总结

本次实现完成了 `RuntimeInterface` 的核心功能，提供了：

✅ **真实的 VM 状态获取** - 从底层 VM 对象获取实时数据  
✅ **严格的参数验证** - 防止非法操作和误用  
✅ **完善的错误处理** - 清晰的错误信息和事件通知  
✅ **线程安全保障** - 安全的并发访问控制  

这为调试工具、监控系统和自动化测试提供了坚实的基础。
