# Runtime Interface - 运行时交互接口模块

## 📖 概述

`RuntimeInterface` 模块提供了与系统（VM 管理、外设等）进行**热交互**的能力，允许在程序运行时动态查询和控制系统状态。

### 核心功能

```
┌─────────────────────────────────────────────┐
│         Runtime Interface                   │
├─────────────────────────────────────────────┤
│ ✅ VM 管理     - 创建/销毁/控制/查询        │
│ ✅ 外设管理   - 配置/监控/数据传输          │
│ ✅ 消息调试   - 发送测试消息                │
│ ✅ 系统监控   - 性能统计/状态查询           │
│ ✅ 回调机制   - 事件通知                    │
│ ✅ 命令行接口 - 交互式控制台                │
└─────────────────────────────────────────────┘
```

---

## 🚀 快速开始

### 1. 初始化接口

```cpp
#include "../utils/RuntimeInterface.h"

// 获取单例实例
auto& runtime = RuntimeInterface::getInstance();

// 初始化（传入 VmManager 和 PeriphManager）
VmManager* vmMgr = ...;
PeriphManager* periphMgr = ...;
runtime.initialize(vmMgr, periphMgr);
```

### 2. 基本使用

```cpp
// 创建 VM
int vmId = runtime.createVM("X86");

// 查询状态
auto status = runtime.getVMStatus(vmId);
std::cout << "VM State: " << status.state << "\n";

// 修改寄存器
runtime.writeRegister(vmId, "EAX", 42);

// 控制运行
runtime.startVM(vmId);
runtime.pauseVM(vmId);
runtime.resumeVM(vmId);
runtime.stopVM(vmId);

// 销毁 VM
runtime.destroyVM(vmId);
```

---

## 📋 API 参考

### VM 管理接口

#### 创建/销毁
```cpp
// 创建 VM
int createVM(const std::string& type, 
             const std::map<std::string, std::string>& config = {});

// 销毁 VM
bool destroyVM(int vmId);
```

#### 状态查询
```cpp
struct VMStatus {
    int id;
    std::string type;
    std::string state;      // "RUNNING", "BLOCKED", "READY"
    uint64_t instructions;  // 执行指令数
    uint64_t cycles;        // CPU 周期数
    size_t memory_used;     // 内存使用（字节）
    int peripheral_count;   // 绑定外设数量
};

VMStatus getVMStatus(int vmId);
std::vector<VMStatus> getAllVMsStatus();
```

#### 运行控制
```cpp
bool startVM(int vmId);
bool stopVM(int vmId);
bool pauseVM(int vmId);
bool resumeVM(int vmId);
```

#### 寄存器访问
```cpp
uint32_t readRegister(int vmId, const std::string& regName);
void writeRegister(int vmId, const std::string& regName, uint32_t value);
```

#### 内存访问
```cpp
void readMemory(int vmId, void* dest, size_t offset, size_t size);
void writeMemory(int vmId, const void* src, size_t offset, size_t size);
```

---

### 外设管理接口

#### 状态查询
```cpp
struct PeriphStatus {
    std::string name;
    std::string type;       // "Terminal", "Disk", "NIC"
    std::string state;      // "ACTIVE", "IDLE", "ERROR"
    size_t data_size;       // 数据大小（字节）
    bool has_pending_op;    // 是否有未完成操作
};

PeriphStatus getPeriphStatus(const std::string& name);
std::vector<PeriphStatus> getAllPeriphsStatus();
```

#### 设备控制
```cpp
bool resetPeriph(const std::string& name);
bool configurePeriph(const std::string& name, 
                     const std::map<std::string, std::string>& config);
```

#### 数据传输
```cpp
bool readPeriphData(const std::string& name, void* buffer, size_t size);
bool writePeriphData(const std::string& name, const void* buffer, size_t size);
```

---

### 消息调试接口

```cpp
// 发送测试消息
bool sendTestMessage(int fromVmId, int toVmId, 
                     MessageType type, const void* payload, size_t size);

// 获取消息队列状态
struct MessageQueueStatus {
    size_t total_capacity;
    size_t current_usage;
    size_t pending_messages;
    size_t dropped_messages;
};

MessageQueueStatus getMessageQueueStatus();
```

---

### 系统监控接口

#### 系统状态
```cpp
struct SystemStatus {
    size_t total_vms;           // VM 总数
    size_t running_vms;         // 运行中 VM 数
    size_t total_peripherals;   // 外设总数
    size_t active_peripherals;  // 活跃外设数
    uint64_t total_instructions;// 总执行指令数
    uint64_t total_cycles;      // 总 CPU 周期
    size_t memory_usage;        // 总内存使用
    double uptime_seconds;      // 运行时间
};

SystemStatus getSystemStatus();
```

#### 性能统计
```cpp
struct PerformanceStats {
    double avg_vm_cpu_usage;    // VM 平均 CPU 使用率
    double messages_per_second; // 消息吞吐率
    double io_operations_per_second; // IO 操作频率
};

PerformanceStats getPerformanceStats();
```

---

### 回调函数注册

```cpp
using StatusCallback = std::function<void(
    const std::string& event, 
    int vmId, 
    const std::string& details
)>;

// 注册回调
runtime.registerStatusCallback([](const std::string& event, 
                                   int vmId, 
                                   const std::string& details) {
    std::cout << "[EVENT] " << event << ": " << details << "\n";
});
```

---

### 命令行接口

```cpp
// 执行命令
std::string executeCommand(const std::string& command);

// 示例
runtime.executeCommand("vm list");
runtime.executeCommand("vm create X86");
runtime.executeCommand("system status");
runtime.executeCommand("help");
```

#### 支持的命令

```bash
# VM 管理
vm list                     # 列出所有 VM
vm create <type>            # 创建 VM
vm destroy <id>             # 销毁 VM
vm status <id>              # 显示 VM 状态
vm start|stop|pause|resume  # 控制 VM

# 外设管理
periph list                 # 列出所有外设
periph show <name>          # 显示外设详情
periph reset <name>         # 重置外设
periph config <name>        # 配置外设

# 系统监控
system status               # 系统整体状态
system stats                # 性能统计

# 其他
help                        # 显示帮助信息
```

---

## 💡 使用场景

### 场景 1：调试控制台

```cpp
auto& runtime = RuntimeInterface::getInstance();

// 启动交互式控制台
std::string line;
while (true) {
    std::cout << "runtime> ";
    std::getline(std::cin, line);
    
    if (line == "quit") break;
    
    std::string output = runtime.executeCommand(line);
    std::cout << output;
}
```

### 场景 2：自动化测试

```cpp
// 测试 VM 创建
int vmId = runtime.createVM("X86");
assert(vmId >= 0);

// 测试寄存器读写
runtime.writeRegister(vmId, "EAX", 0x12345678);
assert(runtime.readRegister(vmId, "EAX") == 0x12345678);

// 测试内存读写
uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
runtime.writeMemory(vmId, data, 0x1000, 4);
uint8_t buffer[4];
runtime.readMemory(vmId, buffer, 0x1000, 4);
assert(memcmp(data, buffer, 4) == 0);

// 清理
runtime.destroyVM(vmId);
```

### 场景 3：监控系统

```cpp
// 注册全局回调
runtime.registerStatusCallback([](const std::string& event, 
                                   int vmId, 
                                   const std::string& details) {
    // 记录日志
    log_event(event, vmId, details);
    
    // 发送告警
    if (event == "VM_ERROR") {
        send_alert("VM " + std::to_string(vmId) + " error: " + details);
    }
});

// 定期采集性能数据
while (running) {
    auto stats = runtime.getPerformanceStats();
    metrics.record(stats);
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

### 场景 4：远程管理

```cpp
// HTTP API 封装
app.get("/api/vms", [](HttpRequest& req, HttpResponse& res) {
    auto vms = runtime.getAllVMsStatus();
    res.json(vms);
});

app.post("/api/vm/create", [](HttpRequest& req, HttpResponse& res) {
    std::string type = req.body["type"];
    int vmId = runtime.createVM(type);
    res.json({{"vm_id", vmId}});
});

app.get("/api/system/stats", [](HttpRequest& req, HttpResponse& res) {
    auto stats = runtime.getPerformanceStats();
    res.json(stats);
});
```

---

## 🔧 实现细节

### 单例模式

```cpp
static RuntimeInterface& getInstance() {
    static RuntimeInterface instance;
    return instance;
}
```

确保全局唯一实例，方便跨模块访问。

### 延迟初始化

```cpp
void initialize(VmManager* vmMgr, PeriphManager* periphMgr) {
    if (m_initialized) return;
    
    m_vmManager = vmMgr;
    m_periphManager = periphMgr;
    m_initialized = true;
}
```

支持在系统启动后再初始化接口。

### 错误处理

```cpp
int createVM(const std::string& type, ...) {
    if (!m_initialized || !m_vmManager) {
        return -1;  // 返回错误码
    }
    
    try {
        return m_vmManager->createX86VM();
    } catch (const std::exception& e) {
        notifyStatusChange("VM_CREATE_FAILED", -1, e.what());
        return -1;
    }
}
```

所有接口都有完善的错误检查和异常处理。

---

## 📊 架构图

```
┌─────────────────────────────────────────────┐
│          Application / User                 │
└──────────────────┬──────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────┐
│         Runtime Interface (API Layer)       │
│  ┌──────────────┬──────────────┬──────────┐ │
│  │  VM Mgmt     │ Periph Mgmt  │  System  │ │
│  │  Control     │  Control     │ Monitor  │ │
│  └──────────────┴──────────────┴──────────┘ │
└──────────────────┬──────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────┐
│         VmManager | PeriphManager           │
└─────────────────────────────────────────────┘
```

---

## ⚠️ 注意事项

### 1. 线程安全
当前实现**未考虑线程安全**。如果在多线程环境中使用，需要添加适当的锁机制。

```cpp
// TODO: 添加互斥锁
std::mutex m_mutex;

void writeRegister(...) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // ... 实现 ...
}
```

### 2. 性能影响
频繁调用状态查询可能影响系统性能，建议：
- 使用回调代替轮询
- 批量查询多个状态
- 设置合理的采样间隔

### 3. 权限控制
生产环境应考虑添加权限验证：

```cpp
bool checkPermission(const std::string& action) {
    // 验证用户权限
    return user.hasPermission(action);
}

void destroyVM(int vmId) {
    if (!checkPermission("vm.destroy")) {
        throw PermissionDeniedException();
    }
    // ... 实现 ...
}
```

---

## 🧪 测试示例

参见 `tests/test_runtime_interface.cpp`：

```bash
# 编译测试
g++ -std=c++17 tests/test_runtime_interface.cpp -o test_runtime

# 运行测试
./test_runtime
```

---

## 📚 相关文件

- **头文件**: `include/utils/RuntimeInterface.h`
- **实现**: `src/utils/RuntimeInterface.cpp`
- **测试**: `tests/test_runtime_interface.cpp`
- **文档**: `docs/运行时交互接口设计.md`

---

## 🎯 未来扩展

### 计划功能
- [ ] 网络接口（REST API / WebSocket）
- [ ] 持久化配置存储
- [ ] 脚本支持（Lua / Python）
- [ ] GUI 界面
- [ ] 性能分析工具集成
- [ ] 断点/单步调试支持

### 优化方向
- [ ] 增量状态更新（减少全量查询）
- [ ] 异步命令执行
- [ ] 命令历史/自动完成
- [ ] 多会话支持

---

## 📝 总结

`RuntimeInterface` 模块为系统提供了强大的运行时交互能力，使得：

✅ **调试更简单** - 随时查看和修改系统状态  
✅ **监控更容易** - 实时采集性能和状态数据  
✅ **控制更灵活** - 动态创建/销毁资源  
✅ **扩展更方便** - 统一的接口设计，易于集成新功能  

这是开发和调试阶段的利器！🚀
