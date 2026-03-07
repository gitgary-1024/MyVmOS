#ifndef LOCKFREE_MESSAGE_QUEUE_H
#define LOCKFREE_MESSAGE_QUEUE_H

#include "../router/MessageProtocol.h"
#include <atomic>
#include <optional>

/**
 * @brief 无锁消息队列（单生产者 - 单消费者模式）
 * 
 * 使用原子操作实现无锁队列
 * 多个生产者可以并发写入，单个消费者轮询读取
 */
class LockFreeMessageQueue {
public:
    static constexpr size_t MAX_QUEUE_SIZE = 1024;
    
    LockFreeMessageQueue() : head_(0), tail_(0) {}
    
    /**
     * @brief 尝试推送消息到队列（无锁）
     * @param msg 要推送的消息
     * @return true 如果推送成功，false 如果队列已满
     */
    bool try_push(const Message& msg) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % MAX_QUEUE_SIZE;
        
        // 检查队列是否已满
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // 队列已满
        }
        
        // 写入消息
        queue_[current_tail] = msg;
        
        // 更新尾指针
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    /**
     * @brief 尝试从队列弹出消息（无锁）
     * @return std::optional<Message> 如果有消息则返回，否则返回空
     */
    std::optional<Message> try_pop() {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        // 检查队列是否为空
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt; // 队列为空
        }
        
        // 读取消息
        Message msg = queue_[current_head];
        
        // 更新头指针
        head_.store((current_head + 1) % MAX_QUEUE_SIZE, std::memory_order_release);
        return msg;
    }
    
    /**
     * @brief 检查队列是否为空
     */
    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief 获取队列中的消息数量（近似值）
     */
    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (tail >= head) ? (tail - head) : (MAX_QUEUE_SIZE - head + tail);
    }
    
private:
    std::array<Message, MAX_QUEUE_SIZE> queue_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

#endif // LOCKFREE_MESSAGE_QUEUE_H
