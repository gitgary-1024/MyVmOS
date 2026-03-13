# Logger 统一日志模块使用指南

## 概述

本项目已经集成了统一的 Logger 日志模块，所有模块都可以使用该模块进行日志输出，无需在每个 .cpp 文件中自定义日志宏。

## Logger 特性

- ✅ **跨平台支持**：Windows/Linux/Mac
- ✅ **彩色日志输出**：不同等级使用不同颜色
- ✅ **线程安全**：支持多线程并发日志
- ✅ **日志等级过滤**：可动态设置日志级别
- ✅ **文件输出**：支持输出到文件
- ✅ **模块化**：支持模块名标识

## 日志等级

```cpp
DEBUG    = 10   // 调试信息（青色）
INFO     = 20   // 一般信息（绿色）
WARNING  = 30   // 警告信息（黄色）
ERR      = 40   // 错误信息（红色）
CRITICAL = 50   // 严重错误（紫色）
```

## 使用方法

### 1. 包含头文件

```cpp
#include "log/Logging.h"
```

### 2. 使用日志宏

#### 基本用法（自动获取文件名）
```cpp
LOG_DEBUG("调试信息");
LOG_INFO("一般信息");
LOG_WARNING("警告信息");
LOG_ERR("错误信息");
LOG_CRITICAL("严重错误");
```

#### 带模块名的版本（推荐）
```cpp
LOG_DEBUG_MOD("Terminal", "调试信息");
LOG_INFO_MOD("Disk", "磁盘初始化完成");
LOG_WARNING_MOD("NIC", "网络连接不稳定");
LOG_ERR_MOD("Router", "消息队列已满");
LOG_CRITICAL_MOD("VM", "虚拟机崩溃");
```

### 3. 动态变量输出

由于 Logger 使用 `std::string`，需要使用 `std::to_string()` 拼接：

```cpp
// ❌ 错误：不支持 printf 风格的格式化
// LOG_INFO("VM", "VM ID: %d, PC: %d", vm_id, pc);

// ✅ 正确：使用 std::to_string() 拼接
LOG_INFO_MOD("VM", (std::string("VM ID: ") + std::to_string(vm_id) + 
                   ", PC: " + std::to_string(pc)).c_str());

// ✅ 或者先构建字符串
std::string msg = "VM " + std::to_string(vm_id) + " state changed";
LOG_INFO_MOD("VM", msg.c_str());
```

### 4. 配置 Logger

```cpp
auto& logger = Logger::instance();

// 设置日志等级（只显示 WARNING 及以上）
logger.set_level(LogLevel::WARNING);

// 设置输出到文件
logger.set_file_output("myapp.log");

// 恢复输出到控制台
logger.set_output(std::cout);
```

## 迁移指南

### 从自定义 LOG_INFO 宏迁移

#### 之前的方式（已废弃）
```cpp
// 在每个 .cpp 文件中定义
#define LOG_INFO(module, fmt, ...) do { \
    std::cout << "[" << module << "] " << fmt << std::endl; \
} while(0)

LOG_INFO("Terminal", "Keyboard interrupt: key='%c'", key);
```

#### 现在的方式（推荐）
```cpp
#include "log/Logging.h"

LOG_INFO_MOD("Terminal", (std::string("Keyboard interrupt: key='") + key + "'").c_str());
```

## 日志输出示例

```
[2026-03-13 18:40:15.753] [   DEBUG] [test_logging.cpp] This is a DEBUG message
[2026-03-13 18:40:15.755] [    INFO] [Router] Router initialized
[2026-03-13 18:40:15.756] [ WARNING] [Disk] Disk space low
[2026-03-13 18:40:15.756] [   ERROR] [NIC] Network connection failed
[2026-03-13 18:40:15.756] [CRITICAL] [System] Critical system failure
```

格式说明：
- `[时间戳]`：精确到毫秒
- `[等级]`：日志级别（固定 8 字符宽度）
- `[模块]`：模块名或文件名
- `消息`：日志内容

## 已迁移的模块

以下模块已完成迁移，使用统一的 Logger：

- ✅ Terminal.cpp - 终端外设模块
- ✅ Disk.cpp - 磁盘外设模块
- ✅ NIC.cpp - 网络外设模块
- ✅ RouterCore.cpp - 路由核心模块
- ✅ mVM.cpp - 虚拟机模块

## 最佳实践

1. **使用模块名**：始终使用 `LOG_*_MOD` 版本，便于日志筛选
2. **合理选择等级**：
   - DEBUG：详细的调试信息
   - INFO：正常的运行状态
   - WARNING：需要注意但不影响运行的问题
   - ERR：错误但可恢复的问题
   - CRITICAL：严重错误，可能导致程序终止
3. **避免过度日志**：在循环中谨慎使用日志
4. **生产环境配置**：设置合适的日志等级（如 WARNING）
5. **开发环境配置**：设置为 DEBUG 以获取详细信息

## 注意事项

- ⚠️ 不要使用 printf 风格的格式化，使用 `std::to_string()` 拼接
- ⚠️ 日志宏是线程安全的，但频繁调用会影响性能
- ⚠️ Windows 下 ERROR 宏已特殊处理，使用 ERR 代替

## 参考资料

- Logger 实现：`src/log/Logging.cpp`
- Logger 接口：`include/log/Logging.h`
- 测试代码：`tests/test_logging.cpp`
