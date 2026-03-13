#include "../../include/router/RouterCore.h"
#include "log/Logging.h"
#include <iostream>

RouterCore& RouterCore::instance() {
    static RouterCore instance;
    return instance;
}

void RouterCore::register_module(uint64_t module_id, const std::string& name) {
    module_names[module_id] = name;
    LOG_INFO_MOD("Router", (std::string("Register module: ") + name + " (ID: " + std::to_string(module_id) + ")").c_str());
}

void RouterCore::subscribe(MessageType type, std::function<void(const Message&)> callback) {
    std::lock_guard<std::mutex> lock(subscriber_mtx);
    subscribers[type].push_back(callback);
    LOG_INFO_MOD("Router", (std::string("Subscriber registered for message type: ") + std::to_string(static_cast<int>(type))).c_str());
}

void RouterCore::subscribe_from(uint64_t sender_id, std::function<void(const Message&)> callback) {
    std::lock_guard<std::mutex> lock(subscriber_mtx);
    sender_subscribers[sender_id].push_back(callback);
    LOG_INFO_MOD("Router", (std::string("Subscriber registered for sender: ") + std::to_string(sender_id)).c_str());
}

bool RouterCore::send(const Message& msg) {
    // 无锁发送，直接扔进队列
    bool success = message_queue.try_push(msg);
    if (success) {
        queue_size.fetch_add(1, std::memory_order_relaxed);
    } else {
        LOG_ERR_MOD("Router", "Queue full, message dropped!");
    }
    return success;
}

std::optional<Message> RouterCore::poll() {
    // 仅由 polling 线程调用
    auto result = message_queue.try_pop();
    if (result.has_value()) {
        queue_size.fetch_sub(1, std::memory_order_relaxed);
    }
    return result;
}

void RouterCore::start_polling() {
    if (running.load(std::memory_order_relaxed)) {
        return;
    }
    
    running.store(true, std::memory_order_relaxed);
    polling_thread = std::thread(&RouterCore::polling_loop, this);
    LOG_INFO_MOD("Router", "Polling thread started on core 0");
}

void RouterCore::stop() {
    if (!running.load(std::memory_order_relaxed)) {
        return;
    }
    
    running.store(false, std::memory_order_relaxed);
    
    if (polling_thread.joinable()) {
        polling_thread.join();
    }
    
    LOG_INFO_MOD("Router", "Stopped");
}

std::string RouterCore::get_module_name(uint64_t module_id) const {
    auto it = module_names.find(module_id);
    return (it != module_names.end()) ? it->second : "Unknown";
}

size_t RouterCore::get_queue_size() const {
    return queue_size.load(std::memory_order_relaxed);
}

void RouterCore::connect_external_sender(std::function<void(const Message&)> sender) {
    std::lock_guard<std::mutex> lock(external_mtx);
    external_senders.push_back(sender);
    LOG_INFO_MOD("Router", "External sender connected");
}

void RouterCore::connect_external_receiver(std::function<void(const Message&)> receiver) {
    std::lock_guard<std::mutex> lock(external_mtx);
    external_receivers.push_back(receiver);
    LOG_INFO_MOD("Router", "External receiver connected");
}

void RouterCore::polling_loop() {
    LOG_INFO_MOD("Router", "Polling loop started");
    
    while (running.load(std::memory_order_relaxed)) {
        // 轮询获取消息
        auto msg_opt = poll();
        
        if (msg_opt.has_value()) {
            // 处理并分发
            process_and_dispatch(msg_opt.value());
        } else {
            // 队列为空，短暂休眠避免空转
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
    
    LOG_INFO_MOD("Router", "Polling loop exited");
}

void RouterCore::process_and_dispatch(const Message& msg) {
    // 单线程处理，不需要锁
    
    LOG_INFO_MOD("Router", (std::string("Processing message: type=") + std::to_string(static_cast<int>(msg.type)) + 
                           ", sender=" + std::to_string(msg.sender_id) + 
                           ", receiver=" + std::to_string(msg.receiver_id)).c_str());
    
    // 1. 按消息类型分发
    {
        std::lock_guard<std::mutex> lock(subscriber_mtx);
        auto it = subscribers.find(msg.type);
        if (it != subscribers.end()) {
            for (auto& callback : it->second) {
                callback(msg);
            }
        }
    }
    
    // 2. 按发送者分发
    {
        std::lock_guard<std::mutex> lock(subscriber_mtx);
        auto it = sender_subscribers.find(msg.sender_id);
        if (it != sender_subscribers.end()) {
            for (auto& callback : it->second) {
                callback(msg);
            }
        }
    }
    
    // 3. 通知外部接收器
    {
        std::lock_guard<std::mutex> lock(external_mtx);
        for (auto& receiver : external_receivers) {
            receiver(msg);
        }
    }
}

void RouterCore::process_external_message(const Message& msg) {
    // 处理外部消息（如果需要）
    send(msg);
}
