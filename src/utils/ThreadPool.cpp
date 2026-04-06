#include "utils/ThreadPool.h"
#include <iostream>
#include <chrono>

ThreadPool::ThreadPool(size_t thread_count) {
    // 如果未指定线程数，使用硬件并发数
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = 4;  // 默认值
        }
    }
    
    std::cout << "[ThreadPool] Creating thread pool with " 
              << thread_count << " threads" << std::endl;
    
    // 创建工作线程
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown(true);
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        // 如果已停止，直接抛出异常
        if (stop_.load()) {
            throw std::runtime_error("Cannot submit task to stopped ThreadPool");
        }
        
        // 将任务加入队列
        tasks_.emplace(std::move(task));
    }
    
    // 通知一个工作线程
    condition_.notify_one();
}

void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // 等待任务或停止信号
            condition_.wait(lock, [this]() {
                return stop_.load() || !tasks_.empty();
            });
            
            // 如果已停止且没有剩余任务，退出线程
            if (stop_.load() && tasks_.empty()) {
                return;
            }
            
            // 取出任务
            task = std::move(tasks_.front());
            tasks_.pop();
            active_tasks_.fetch_add(1);
        }
        
        // 执行任务（在锁外，避免阻塞其他线程）
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "[ThreadPool] Task exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[ThreadPool] Task unknown exception" << std::endl;
        }
        
        active_tasks_.fetch_sub(1);
    }
}

size_t ThreadPool::get_pending_task_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::shutdown(bool wait_for_completion) {
    // 设置停止标志
    bool expected = false;
    if (!stop_.compare_exchange_strong(expected, true)) {
        return;  // 已经在停止过程中
    }
    
    std::cout << "[ThreadPool] Shutting down..." << std::endl;
    
    // 通知所有工作线程
    condition_.notify_all();
    
    if (wait_for_completion) {
        // 等待所有活动任务完成
        while (active_tasks_.load() > 0 || get_pending_task_count() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "[ThreadPool] All tasks completed, joining threads..." << std::endl;
    } else {
        std::cout << "[ThreadPool] Forcing shutdown, pending tasks will be discarded" << std::endl;
    }
    
    // 等待所有工作线程退出
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    std::cout << "[ThreadPool] Shutdown complete" << std::endl;
}
