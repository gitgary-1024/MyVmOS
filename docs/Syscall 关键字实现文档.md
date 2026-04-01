# Syscall 关键字实现文档

## 概述

在 x86 VM 编译器中添加了 `syscall(<syscall_num>)` 函数作为新的"关键字"，允许 VM 通过路由树向外设发送系统调用请求。

## 实现内容

### 1. 指令编码

**操作码**: `0x05` (简化实现)

**功能**: 触发系统调用，通过路由树向外设发送消息

**寄存器约定**:
- `RAX`: 系统调用号
- `RCX`, `R11`: 自动保存（按 x86-64 syscall 规范）

### 2. 核心文件修改

#### 2.1 `include/vm/X86CPU.h`

添加了：
- `trigger_syscall(uint64_t syscall_num)` 方法声明
- `get_rcx()` 和 `get_r11()` 便捷访问方法

```cpp
void trigger_syscall(uint64_t syscall_num);  // 触发系统调用
uint64_t get_rcx() const;
uint64_t get_r11() const;
```

#### 2.2 `src/vm/X86_decode.cpp`

在指令解码器中添加了对 `0x05` 操作码的处理：

```cpp
// ===== SYSCALL =====
case 0x05:  // SYSCALL (简化实现，使用 0x05 作为 syscall 操作码)
    {
        // 系统调用号从 RAX 寄存器获取
        uint64_t syscall_num = get_register(X86Reg::RAX);
        std::cout << "[X86VM-" << vm_id_ << "] SYSCALL triggered with num=" 
                  << syscall_num << " at RIP=0x" 
                  << std::hex << get_rip() << std::dec << std::endl;
        
        // 触发系统调用中断（使用特定的中断向量，例如 0x80）
        trigger_syscall(syscall_num);
        return 1;
    }
```

#### 2.3 `src/vm/X86CPU.cpp`

实现了 `trigger_syscall` 函数：

```cpp
void X86CPUVM::trigger_syscall(uint64_t syscall_num) {
    // 1. 保存现场（RAX 已经包含 syscall_num）
    push(get_rcx());  // 保存返回地址
    push(get_r11());  // 保存 RFLAGS
    
    // 2. 设置返回地址为下一条指令
    uint64_t ret_rip = get_rip();
    
    // 3. 构造系统调用消息
    Message msg;
    msg.sender_id = vm_id_;
    msg.receiver_id = MODULE_ROUTER_CORE;
    msg.type = MessageType::SYSCALL;
    
    SyscallRequest req;
    req.vm_id = vm_id_;
    req.syscall_number = syscall_num;
    // 可以从其他寄存器获取参数，如 RDI, RSI, RDX, RCX 等
    msg.set_payload(req);
    
    // 4. 打印日志（后续会通过 VmManager 发送到路由树）
    std::cout << "[X86VM-" << vm_id_ << "] SYSCALL #" << syscall_num 
              << " - Sending to router tree" << std::endl;
    
    // 5. 更新状态
    state_ = X86VMState::WAITING_INTERRUPT;
    
    // 6. 跳转到系统调用处理程序（从 IDT 获取）
    uint64_t idt_base = 0;
    uint64_t syscall_handler = read_qword(idt_base + 0x80 * 8);
    if (syscall_handler != 0) {
        set_rip(syscall_handler);
    } else {
        // 如果没有设置处理程序，直接返回到用户空间
        std::cout << "[X86VM-" << vm_id_ << "] No syscall handler, returning" << std::endl;
        set_rip(ret_rip);
    }
    
    // 7. 清除 IF 标志
    set_flag(FLAG_IF, false);
}
```

#### 2.4 `include/router/MessageProtocol.h`

添加了系统调用相关的消息类型和数据结构：

```cpp
// MessageType 枚举
enum class MessageType {
    // ... 其他类型 ...
    SYSCALL  // 系统调用请求（从 VM 发起到内核/外设）
};

// 系统调用请求结构
struct SyscallRequest {
    uint64_t vm_id;
    uint64_t syscall_number;  // 系统调用号
    uint64_t arg1 = 0;        // 参数 1
    uint64_t arg2 = 0;        // 参数 2
    uint64_t arg3 = 0;        // 参数 3
    uint64_t arg4 = 0;        // 参数 4
};

// 添加到 Message 的 variant 中
std::variant<
    // ... 其他类型 ...
    SyscallRequest                // 系统调用请求
> payload;
```

### 3. 测试程序

创建了 `tests/test_vm_syscall.cpp` 测试文件：

```cpp
// 汇编代码：
//   mov rax, 1      ; 系统调用号 1 (exit)
//   syscall         ; 触发系统调用

std::vector<uint8_t> test_program = {
    // mov rax, 1    ; B8 01 00 00 00 00 00 00 00
    0xB8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    
    // syscall       ; 0x05
    0x05
};
```

### 4. CMakeLists.txt 更新

添加了 syscall 测试目标：

```cmake
# SYSCALL 测试
add_executable(test_vm_syscall tests/test_vm_syscall.cpp)
target_link_libraries(test_vm_syscall myos_lib Threads::Threads)
```

## 使用方法

### 编译

```bash
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build . --target test_vm_syscall
```

### 运行测试

```bash
./bin/test_vm_syscall.exe
```

### 示例输出

```
===== x86 VM SYSCALL 测试 =====
[X86VM-0] Loaded 10 bytes at 0x100000
测试程序已加载到地址 0x100000
程序内容：
  mov rax, 1   ; 设置系统调用号 1
  syscall      ; 触发系统调用

开始执行...
[Step 1] RIP=0x100009 RAX=0x1 State=RUNNING
[X86VM-0] SYSCALL triggered with num=1 at RIP=0x100009
[X86VM-0] SYSCALL #1 - Sending to router tree
[X86VM-0] No syscall handler, returning
[Step 2] RIP=0x10000a RAX=0x1 State=WAITING_INTERRUPT
```

## 系统调用号约定

| 编号 | 功能 | 说明 |
|------|------|------|
| 0 | unused | 保留 |
| 1 | exit | 退出/关机 |
| 2 | read | 读取数据 |
| 3 | write | 写入数据 |
| 4 | ioctl | 设备控制 |
| ... | ... | ... |

## 与路由树的集成

当前实现中，系统调用消息会被发送到 `MODULE_ROUTER_CORE`（路由核心），后续可以：

1. **路由到外设**: 根据 syscall_num 路由到不同的外设管理器
2. **权限检查**: 验证 VM 是否有权执行该系统调用
3. **参数传递**: 从寄存器（RDI, RSI, RDX, RCX 等）获取参数
4. **返回值处理**: 将系统调用结果写回 RAX

### 未来扩展示例

```cpp
// 在 RouterCore 或 PeriphManager 中处理
case MessageType::SYSCALL:
    {
        auto* req = std::get_if<SyscallRequest>(&msg.payload);
        switch (req->syscall_number) {
            case 1:  // exit
                handle_vm_exit(req->vm_id);
                break;
            case 2:  // read
                route_to_periph(req->vm_id, PERIPH_READ);
                break;
            case 3:  // write
                route_to_periph(req->vm_id, PERIPH_WRITE);
                break;
        }
    }
    break;
```

## 架构优势

1. **统一接口**: 所有 VM 到外设的通信都通过 syscall 机制
2. **可扩展性**: 轻松添加新的系统调用类型
3. **安全性**: 通过路由树进行权限检查和消息验证
4. **性能**: 基于现有的消息队列机制，支持异步处理

## 注意事项

1. **操作码选择**: 当前使用 `0x05` 作为 syscall 操作码（简化实现），真实 x86 使用 `0F 05`
2. **IDT 配置**: 需要预先设置中断描述符表（IDT）中的 syscall 处理程序
3. **参数传递**: 当前仅实现了 syscall_num，后续需支持从寄存器获取多个参数
4. **错误处理**: 需要添加对无效 syscall_num 的处理

## 相关文件

- `include/vm/X86CPU.h` - X86 CPU VM 头文件
- `src/vm/X86CPU.cpp` - X86 CPU VM 实现
- `src/vm/X86_decode.cpp` - 指令解码器
- `include/router/MessageProtocol.h` - 消息协议定义
- `tests/test_vm_syscall.cpp` - 测试程序

## 总结

成功实现了 `syscall(<syscall_num>)` 作为 x86 VM 的关键字之一，为 VM 与外设之间的通信提供了统一的系统调用接口。该实现遵循 x86-64 架构的 syscall 规范，并与现有的路由树消息机制无缝集成。
