#ifndef LIVS_UTILS_THREAD_POOL_H
#define LIVS_UTILS_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>

/**
 * @brief 通用线程池 - 用于异步任务执行
 * 
 * 特性：
 * 1. 固定数量工作线程，避免频繁创建/销毁线程
 * 2. 任务队列支持无限增长（受内存限制）
 * 3. 支持提交带返回值的任务
 * 4. 优雅关闭：等待所有任务完成
 * 5. 线程安全：支持多线程同时提交任务
 * 
 * 使用场景：
 * - 异步中断处理
 * - 后台数据刷新
 * - 延迟任务执行
 */
class ThreadPool {
public:
    /**
     * @brief 构造线程池
     * @param thread_count 工作线程数量（建议设置为 CPU 核心数）
     */
    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency());
    
    ~ThreadPool();
    
    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    /**
     * @brief 提交无返回值任务
     * @param task 任务函数
     * 
     * 示例：
     * ```cpp
     * pool.submit([]() {
     *     std::cout << "Hello from thread pool" << std::endl;
     * });
     * ```
     */
    void submit(std::function<void()> task);
    
    /**
     * @brief 提交带返回值任务
     * @tparam F 函数类型
     * @tparam Args 参数类型
     * @param f 任务函数
     * @param args 函数参数
     * @return std::future<ReturnType> 获取返回值的 future
     * 
     * 示例：
     * ```cpp
     * auto result = pool.submit([](int x, int y) {
     *     return x + y;
     * }, 3, 4);
     * std::cout << "Result: " << result.get() << std::endl;  // 输出 7
     * ```
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<decltype(f(args...))>;
    
    /**
     * @brief 获取当前等待中的任务数量
     * @return size_t 等待任务数
     */
    size_t get_pending_task_count() const;
    
    /**
     * @brief 获取工作线程数量
     * @return size_t 线程数
     */
    size_t get_thread_count() const { return workers_.size(); }
    
    /**
     * @brief 判断线程池是否正在运行
     * @return bool 运行状态
     */
    bool is_running() const { return !stop_.load(); }
    
    /**
     * @brief 停止线程池
     * @param wait_for_completion 是否等待所有任务完成
     * 
     * - true: 等待所有任务完成后关闭（优雅关闭）
     * - false: 立即停止，未执行的任务将被丢弃
     */
    void shutdown(bool wait_for_completion = true);
    
private:
    // 工作线程函数
    void worker_loop();
    
    // 工作线程集合
    std::vector<std::thread> workers_;
    
    // 任务队列（存储函数对象）
    std::queue<std::function<void()>> tasks_;
    
    // 任务队列互斥锁
    mutable std::mutex queue_mutex_;
    
    // 条件变量：通知工作线程有任务到达
    std::condition_variable condition_;
    
    // 停止标志
    std::atomic<bool> stop_{false};
    
    // 活动任务计数（正在执行的任务数）
    std::atomic<size_t> active_tasks_{0};
};

// ============================================================================
// 模板函数实现
// ============================================================================

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<decltype(f(args...))>
{
    using ReturnType = decltype(f(args...));
    
    // 创建包装任务（绑定参数）
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    // 获取 future 用于获取返回值
    std::future<ReturnType> result = task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        // 如果已停止，直接抛出异常
        if (stop_.load()) {
            throw std::runtime_error("Cannot submit task to stopped ThreadPool");
        }
        
        // 将任务加入队列
        tasks_.emplace([task]() { (*task)(); });
    }
    
    // 通知一个工作线程
    condition_.notify_one();
    
    return result;
}

#endif // LIVS_UTILS_THREAD_POOL_H
