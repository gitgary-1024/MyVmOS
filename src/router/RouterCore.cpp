#include "../../include/router/RouterCore.h"
#include <iostream>

RouterCore& RouterCore::instance() {
    static RouterCore instance;
    return instance;
}

void RouterCore::register_module(uint64_t module_id, const std::string& name) {
    module_names[module_id] = name;
    std::cout << "[Router] Register module: " << name << " (ID: " << module_id << ")" << std::endl;
}

void RouterCore::subscribe(MessageType type, std::function<void(const Message&)> callback) {
    std::lock_guard<std::mutex> lock(subscriber_mtx);
    subscribers[type].push_back(callback);
    std::cout << "[Router] Subscriber registered for message type: " << static_cast<int>(type) << std::endl;
}

void RouterCore::subscribe_from(uint64_t sender_id, std::function<void(const Message&)> callback) {
    std::lock_guard<std::mutex> lock(subscriber_mtx);
    sender_subscribers[sender_id].push_back(callback);
    std::cout << "[Router] Subscriber registered for sender: " << sender_id << std::endl;
}

bool RouterCore::send(const Message& msg) {
    // 无锁发送，直接扔进队列
    bool success = message_queue.try_push(msg);
    if (success) {
        queue_size.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::cerr << "[Router] Queue full, message dropped!" << std::endl;
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
    std::cout << "[Router] Polling thread started on core 0" << std::endl;
}

void RouterCore::stop() {
    if (!running.load(std::memory_order_relaxed)) {
        return;
    }
    
    running.store(false, std::memory_order_relaxed);
    
    if (polling_thread.joinable()) {
        polling_thread.join();
    }
    
    std::cout << "[Router] Stopped" << std::endl;
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
    std::cout << "[Router] External sender connected" << std::endl;
}

void RouterCore::connect_external_receiver(std::function<void(const Message&)> receiver) {
    std::lock_guard<std::mutex> lock(external_mtx);
    external_receivers.push_back(receiver);
    std::cout << "[Router] External receiver connected" << std::endl;
}

void RouterCore::polling_loop() {
    std::cout << "[Router] Polling loop started" << std::endl;
    
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
    
    std::cout << "[Router] Polling loop exited" << std::endl;
}

void RouterCore::process_and_dispatch(const Message& msg) {
    // 单线程处理，不需要锁
    
    std::cout << "[Router] Processing message: type=" << static_cast<int>(msg.type)
              << ", sender=" << msg.sender_id << ", receiver=" << msg.receiver_id << std::endl;
    
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
