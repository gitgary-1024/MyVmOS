#ifndef LIVS_ROUTER_MESSAGE_PROTOCOL_H
#define LIVS_ROUTER_MESSAGE_PROTOCOL_H

#include <cstdint>
#include <chrono>
#include <string>
#include <variant>
#include <memory>
#include <vector>
#include <functional>

// ===== 1. 消息类型枚举（严格按模块职责划分）=====
enum class MessageType {
    // --- VM层相关 ---
    VM_CREATE_REQUEST,
    VM_CREATE_RESPONSE,
    VM_DESTROY_REQUEST,
    VM_DESTROY_RESPONSE,
    VM_SUSPEND_NOTIFY,      // VM被挂起通知（由调度器发出）
    VM_WAKEUP_NOTIFY,       // VM被唤醒通知
    VM_STATE_QUERY,
    VM_STATE_RESPONSE,

    // --- 中断调度核心层 ---
    INTERRUPT_REQUEST,      // 通用中断请求入口
    INTERRUPT_SYNC_BEGIN,   // 同步中断开始（VM挂起前）
    INTERRUPT_SYNC_COMPLETE,// 同步中断完成（含返回值）
    INTERRUPT_ASYNC_ENQUEUE,// 异步中断入队（后台执行）
    INTERRUPT_RESULT_READY, // 中断结果就绪（同步/异步均可触发）

    // --- 外设抽象层 ---
    PERIPH_LOCK_REQUEST,    // 请求GIL锁（带timeout）
    PERIPH_LOCK_GRANTED,    // GIL锁获取成功
    PERIPH_LOCK_TIMEOUT,    // GIL锁超时
    PERIPH_UNLOCK_REQUEST,  // 释放GIL锁
    PERIPH_DATA_READY,      // 外设数据就绪（如传感器读数）
    PERIPH_WAIT_QUEUE_FULL, // 等待队列满（限流告警）

    // --- 调度器层 ---
    SCHEDULER_ADD_TO_RUNQ,  // 加入运行队列
    SCHEDULER_ADD_TO_SUSPENDQ, // 加入挂起队列
    SCHEDULER_SCHEDULE_NEXT, // 请求调度下一VM
    SCHEDULER_IDLE_NOTIFY,  // 调度器空闲（可用于唤醒异步任务）

    // --- 系统控制 ---
    ROUTER_PING,
    ROUTER_PONG,
    SYSTEM_SHUTDOWN_REQUEST,
    SYSTEM_SHUTDOWN_ACK
};

// ===== 2. 消息ID生成规则 =====
// 格式：[模块ID][子类型][序列号]
// 模块ID分配（建议）：
//   0x01: VMManager
//   0x02: VMStateManager
//   0x03: InterruptScheduler
//   0x04: PeriphManager
//   0x05: VMScheduler
//   0x06: ThreadPool
//   0x07: TimeoutManager
//   0xFF: RouterCore (广播/系统)
constexpr uint64_t MODULE_VM_MANAGER = 0x01;
constexpr uint64_t MODULE_VM_STATE = 0x02;
constexpr uint64_t MODULE_INTERRUPT_SCHEDULER = 0x03;
constexpr uint64_t MODULE_PERIPH_MANAGER = 0x04;
constexpr uint64_t MODULE_SCHEDULER = 0x05;
constexpr uint64_t MODULE_THREAD_POOL = 0x06;
constexpr uint64_t MODULE_TIMEOUT_MANAGER = 0x07;
constexpr uint64_t MODULE_ROUTER_CORE = 0xFF;

// ===== 3. Payload 数据类型（使用 std::variant 保证类型安全）=====
struct VMCreateRequest {
    uint64_t vm_id;
    int cpu_limit;
    size_t mem_limit_bytes;
    std::string config_name;
};

struct VMCreateResponse {
    uint64_t vm_id;
    int error_code; // ErrorCode value
};

struct InterruptRequest {
    uint64_t vm_id;
    int periph_id;
    MessageType interrupt_type; // 实际应为 InterruptType，但为兼容消息路由暂用MessageType
    int timeout_ms = 2000;
};

struct InterruptResult {
    uint64_t vm_id;
    int return_value;
    bool is_timeout = false;
};

struct PeriphLockRequest {
    uint64_t vm_id;
    int periph_id;
    int channel_id;
    int timeout_ms;
};

struct PeriphLockGranted {
    uint64_t vm_id;
    int periph_id;
    int channel_id;
};

struct SchedulerTaskEnqueue {
    uint64_t vm_id;
    uint64_t task_id;
    bool is_async; // true=async, false=sync
};

// ✅ 新增：VM 唤醒通知
struct VMWakeUpNotify {
    uint64_t vm_id;
};

// ===== 4. 通用消息结构体 =====
struct Message {
    uint64_t sender_id = 0;
    uint64_t receiver_id = 0;
    MessageType type = MessageType::ROUTER_PING;
    std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
    
    // 类型安全的负载数据
    std::variant<
        std::monostate,               // empty
        VMCreateRequest,
        VMCreateResponse,
        InterruptRequest,
        InterruptResult,
        PeriphLockRequest,
        PeriphLockGranted,
        SchedulerTaskEnqueue,
        VMWakeUpNotify                 // ✅ 新增到 variant 列表
    > payload;

    // 默认构造函数
    Message() = default;

    // 带参构造
    Message(uint64_t s, uint64_t r, MessageType t) 
        : sender_id(s), receiver_id(r), type(t), timestamp(std::chrono::steady_clock::now()) {}

    template<typename T>
    void set_payload(const T& data) {
        payload = data;
    }

    template<typename T>
    bool is_payload() const {
        return std::holds_alternative<T>(payload);
    }

    template<typename T>
    const T* get_payload() const {
        auto* p = std::get_if<T>(&payload);
        return p;
    }
};

// ===== 5. 错误码映射（复用文档中定义）=====
enum class ErrorCode {
    OK = 0,
    VM_NOT_FOUND,
    GIL_TIMEOUT,
    QUEUE_FULL,
    VM_DESTROYED,
    RESOURCE_LIMIT_EXCEEDED
};

// ===== 6. 辅助工具函数 =====
inline std::string message_type_to_string(MessageType type) {
    switch (type) {
        case MessageType::VM_CREATE_REQUEST: return "VM_CREATE_REQUEST";
        case MessageType::VM_CREATE_RESPONSE: return "VM_CREATE_RESPONSE";
        case MessageType::INTERRUPT_REQUEST: return "INTERRUPT_REQUEST";
        case MessageType::INTERRUPT_SYNC_COMPLETE: return "INTERRUPT_SYNC_COMPLETE";
        case MessageType::PERIPH_LOCK_GRANTED: return "PERIPH_LOCK_GRANTED";
        case MessageType::SCHEDULER_ADD_TO_RUNQ: return "SCHEDULER_ADD_TO_RUNQ";
        case MessageType::VM_WAKEUP_NOTIFY: return "VM_WAKEUP_NOTIFY"; // ✅ 新增
        default: return "UNKNOWN";
    }
}

#endif // LIVS_ROUTER_MESSAGE_PROTOCOL_H