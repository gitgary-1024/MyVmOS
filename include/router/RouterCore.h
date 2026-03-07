#ifndef LIVS_ROUTER_CORE_H
#define LIVS_ROUTER_CORE_H

#include "MessageProtocol.h"
#include "LockFreeMessageQueue.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <optional>

// 前置声明
class RouterCore;

// 消息队列项（带优先级）
struct PriorityMessage {
    int priority = 999;          // 默认低优先级
    Message msg;                 // 默认构造（Message 有默认构造）
    
    PriorityMessage() = default; // ✅ 允许默认构造
    PriorityMessage(int p, const Message& m) : priority(p), msg(m) {}
    
    bool operator<(const PriorityMessage& other) const {
        return priority > other.priority; // max-heap → 用 > 实现 min-heap
    }
};

// ===== RouterCore 类定义 - 纯轮询模式 =====
class RouterCore {
public:
    static RouterCore& instance();  // 单例，全局唯一

    // 模块注册（可选，用于调试）
    void register_module(uint64_t module_id, const std::string& name);

    // 订阅消息类型（线程安全）
    void subscribe(MessageType type, std::function<void(const Message&)> callback);
    
    // 订阅特定 sender 的消息（可选增强）
    void subscribe_from(uint64_t sender_id, std::function<void(const Message&)> callback);

    // 发送消息（无锁，入队）
    bool send(const Message& msg);
    
    // 轮询获取消息（仅由 polling 线程调用）
    std::optional<Message> poll();

    // 启动 polling 线程（0 核）
    void start_polling();
    void stop();

    // 获取模块名（调试用）
    std::string get_module_name(uint64_t module_id) const;
    
    // 获取队列大小
    size_t get_queue_size() const;

    // 外部线程连接接口
    void connect_external_sender(std::function<void(const Message&)> sender);
    void connect_external_receiver(std::function<void(const Message&)> receiver);

private:
    RouterCore() = default;
    ~RouterCore() = default;

    // 内部数据结构
    std::unordered_map<uint64_t, std::string> module_names;
    std::unordered_map<MessageType, std::vector<std::function<void(const Message&)>>> subscribers;
    std::unordered_map<uint64_t, std::vector<std::function<void(const Message&)>>> sender_subscribers;
    mutable std::mutex subscriber_mtx;  // 仅在订阅/取消订阅时使用

    // 无锁消息队列（多生产者 - 单消费者）
    LockFreeMessageQueue message_queue;
    std::atomic<size_t> queue_size{0};
    
    // Polling 线程
    std::atomic<bool> running{false};
    std::thread polling_thread;
    
    // 外部连接接口
    std::vector<std::function<void(const Message&)>> external_senders;
    std::vector<std::function<void(const Message&)>> external_receivers;
    mutable std::mutex external_mtx;

    // Polling 线程函数
    void polling_loop();
    
    // 处理消息并分发
    void process_and_dispatch(const Message& msg);
    
    // 处理外部接收的消息
    void process_external_message(const Message& msg);
};

// ===== 全局快捷函数（方便模块使用）=====
inline bool route_send(const Message& msg) {
    return RouterCore::instance().send(msg);
}

inline void route_subscribe(MessageType type, std::function<void(const Message&)> cb) {
    RouterCore::instance().subscribe(type, cb);
}

#endif // LIVS_ROUTER_CORE_H