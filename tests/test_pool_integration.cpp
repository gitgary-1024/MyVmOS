/**
 * @brief ThreadPool 和 ObjectPool 在 LIVS 系统中的集成示例
 * 
 * 本文件展示如何将线程池和对象池集成到现有的 VM 调度系统中
 */

#include <iostream>
#include <memory>
#include "utils/ThreadPool.h"
#include "utils/ObjectPool.h"
#include "vm/baseVM.h"
#include "router/MessageProtocol.h"

using namespace std;

// ============================================================================
// 示例 1: 使用线程池处理异步中断
// ============================================================================

class AsyncInterruptHandler {
public:
    explicit AsyncInterruptHandler(ThreadPool& pool) : thread_pool_(pool) {}
    
    /**
     * @brief 提交异步中断任务到线程池
     * 
     * 传统方式：每个异步中断都创建新线程
     * ```cpp
     * std::thread([=]() {
     *     handle_interrupt(vm_id, periph_id);
     * }).detach();
     * ```
     * 
     * 优化后：使用线程池执行
     */
    void submit_async_interrupt(uint64_t vm_id, int periph_id) {
        // 提交到线程池，避免频繁创建/销毁线程
        thread_pool_.submit([this, vm_id, periph_id]() {
            handle_interrupt(vm_id, periph_id);
        });
    }
    
private:
    void handle_interrupt(uint64_t vm_id, int periph_id) {
        cout << "[AsyncInterrupt] Handling interrupt for VM " << vm_id 
             << " -> periph " << periph_id << endl;
        
        // 模拟外设操作（耗时）
        this_thread::sleep_for(chrono::milliseconds(50));
        
        cout << "[AsyncInterrupt] Completed for VM " << vm_id << endl;
    }
    
    ThreadPool& thread_pool_;
};

// ============================================================================
// 示例 2: 使用对象池复用 Message 对象
// ============================================================================

class MessageFactory {
public:
    explicit MessageFactory(size_t pool_size = 100) 
        : message_pool_(pool_size, 0, true) {}  // 预分配 100 个，无上限，自动扩容
    
    /**
     * @brief 从池中获取 Message 对象
     * @return unique_ptr<Message> 消息对象
     */
    unique_ptr<Message> create_message(uint64_t sender, 
                                      uint64_t dest, 
                                      MessageType type) {
        auto msg = message_pool_.acquire();
        if (!msg) {
            throw runtime_error("Failed to acquire message from pool");
        }
        
        // 初始化 Message
        *msg = Message(sender, dest, type);
        return msg;
    }
    
    /**
     * @brief 归还 Message 对象到池中
     * @param msg 要归还的消息
     */
    void recycle_message(unique_ptr<Message> msg) {
        // ObjectPool 会自动调用 reset()
        message_pool_.release(std::move(msg));
    }
    
    /**
     * @brief 获取池统计信息
     */
    void print_stats() const {
        cout << "[MessagePool] Free: " << message_pool_.get_free_count()
             << ", Allocated: " << message_pool_.get_allocated_count()
             << endl;
    }
    
private:
    ObjectPool<Message> message_pool_;
};

// ============================================================================
// 示例 3: 使用对象池复用 InterruptTask 对象
// ============================================================================

struct InterruptTask {
    uint64_t vm_id;
    int periph_id;
    int timeout_ms;
    chrono::steady_clock::time_point create_time;
    
    void reset() {
        vm_id = 0;
        periph_id = 0;
        timeout_ms = 0;
        create_time = chrono::steady_clock::now();
    }
};

class InterruptTaskManager {
public:
    explicit InterruptTaskManager(size_t pool_size = 50) 
        : task_pool_(pool_size, 200, false) {}  // 预分配 50 个，最大 200 个
    
    /**
     * @brief 创建中断任务
     */
    unique_ptr<InterruptTask> create_task(uint64_t vm_id, 
                                         int periph_id, 
                                         int timeout_ms) {
        auto task = task_pool_.acquire();
        if (!task) {
            cerr << "[InterruptTaskManager] Pool capacity reached!" << endl;
            return nullptr;  // 限流
        }
        
        task->vm_id = vm_id;
        task->periph_id = periph_id;
        task->timeout_ms = timeout_ms;
        task->create_time = chrono::steady_clock::now();
        
        return task;
    }
    
    /**
     * @brief 回收中断任务
     */
    void recycle_task(unique_ptr<InterruptTask> task) {
        task_pool_.release(std::move(task));
    }
    
    /**
     * @brief 批量创建任务（演示对象池优势）
     */
    void batch_create_tasks() {
        vector<unique_ptr<InterruptTask>> tasks;
        
        cout << "\n[Batch Create] Creating 100 interrupt tasks..." << endl;
        for (int i = 0; i < 100; ++i) {
            auto task = create_task(i % 10, 3, 2000);
            if (task) {
                tasks.push_back(move(task));
            }
        }
        
        cout << "[Batch Create] Created " << tasks.size() << " tasks" << endl;
        cout << "[Batch Create] Pool stats - Free: " 
             << task_pool_.get_free_count() 
             << ", Allocated: " << task_pool_.get_allocated_count() << endl;
        
        // 回收所有任务
        for (auto& task : tasks) {
            recycle_task(move(task));
        }
        
        cout << "[Batch Create] Recycled all tasks" << endl;
        cout << "[Batch Create] Pool stats - Free: " 
             << task_pool_.get_free_count() 
             << ", Allocated: " << task_pool_.get_allocated_count() << endl;
    }
    
private:
    ObjectPool<InterruptTask> task_pool_;
};

// ============================================================================
// 示例 4: 在线程池中使用对象池（组合使用）
// ============================================================================

class IntegratedScheduler {
public:
    IntegratedScheduler() 
        : thread_pool_(4),  // 4 个工作线程
          message_pool_(50) {}
    
    /**
     * @brief 处理 VM 中断请求（异步模式）
     */
    void handle_vm_interrupt(uint64_t vm_id, int periph_id) {
        // 创建消息对象
        auto msg = message_pool_.acquire();
        if (!msg) {
            cerr << "[IntegratedScheduler] Message pool exhausted!" << endl;
            return;
        }
        
        *msg = Message(vm_id, MODULE_VM_MANAGER, MessageType::INTERRUPT_REQUEST);
        
        // 提交到线程池处理
        thread_pool_.submit([this, msg = move(msg)]() mutable {
            try {
                // 模拟处理
                cout << "[IntegratedScheduler] Processing interrupt for VM " 
                     << msg->sender_id << endl;
                
                this_thread::sleep_for(chrono::milliseconds(30));
                
                // 处理完成后归还消息对象（ObjectPool 会自动调用 reset()）
                message_pool_.release(move(msg));
                
                cout << "[IntegratedScheduler] Completed and recycled message" << endl;
                
            } catch (const exception& e) {
                cerr << "[IntegratedScheduler] Exception: " << e.what() << endl;
                // 异常时也要归还对象
                if (msg) {
                    message_pool_.release(move(msg));
                }
            }
        });
    }
    
    /**
     * @brief 等待所有任务完成并关闭
     */
    void shutdown() {
        thread_pool_.shutdown(true);
    }
    
private:
    ThreadPool thread_pool_;
    ObjectPool<Message> message_pool_;
};

// ============================================================================
// 主函数：演示所有示例
// ============================================================================

int main() {
    cout << "========================================" << endl;
    cout << "  LIVS Integration Examples           " << endl;
    cout << "========================================" << endl;
    
    // 示例 1: 异步中断处理
    cout << "\n--- Example 1: Async Interrupt Handler ---" << endl;
    {
        ThreadPool pool(2);
        AsyncInterruptHandler handler(pool);
        
        // 模拟多个 VM 同时发起异步中断
        for (int i = 0; i < 5; ++i) {
            handler.submit_async_interrupt(i, 3);
        }
        
        this_thread::sleep_for(chrono::milliseconds(200));
    }
    
    // 示例 2: Message 对象池
    cout << "\n--- Example 2: Message Factory ---" << endl;
    {
        MessageFactory factory;
        
        // 创建和回收多个 Message
        for (int i = 0; i < 10; ++i) {
            auto msg = factory.create_message(i, MODULE_VM_MANAGER, 
                                             MessageType::INTERRUPT_REQUEST);
            cout << "[Example 2] Created message " << i << endl;
            factory.recycle_message(move(msg));
        }
        
        factory.print_stats();
    }
    
    // 示例 3: InterruptTask 对象池
    cout << "\n--- Example 3: Interrupt Task Manager ---" << endl;
    {
        InterruptTaskManager manager;
        manager.batch_create_tasks();
    }
    
    // 示例 4: 集成使用（线程池 + 对象池）
    cout << "\n--- Example 4: Integrated Scheduler ---" << endl;
    {
        IntegratedScheduler scheduler;
        
        // 模拟多个 VM 的中断请求
        for (int i = 0; i < 8; ++i) {
            scheduler.handle_vm_interrupt(i, 3);
        }
        
        this_thread::sleep_for(chrono::milliseconds(500));
        scheduler.shutdown();
    }
    
    cout << "\n========================================" << endl;
    cout << "  All examples completed successfully! " << endl;
    cout << "========================================" << endl;
    
    return 0;
}
