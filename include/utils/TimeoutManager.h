#ifndef TIMEOUT_MANAGER_H
#define TIMEOUT_MANAGER_H

#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <functional>
#include <optional>
#include <atomic>
#include <string>

/**
 * @brief 统一超时管理器
 * 
 * 功能：
 * 1. 统一管理所有超时请求（中断、消息等）
 * 2. 高精度定时器轮（最小 1ms）
 * 3. 自动检测和处理超时
 * 4. 支持一次性超时和周期性超时
 * 5. 无锁性能统计
 * 
 * 使用示例：
 * // 注册超时
 * auto timeout_id = TimeoutManager::instance().register_timeout(
 *     2000,  // 2 秒超时
 *     []() { std::cout << "Timeout!"; }
 * );
 * 
 * // 取消超时
 * TimeoutManager::instance().cancel_timeout(timeout_id);
 */
class TimeoutManager {
public:
    // 超时 ID 类型
    using TimeoutId = uint64_t;
    
    // 超时回调函数类型
    using TimeoutCallback = std::function<void()>;
    
    // 超时配置
    struct TimeoutConfig {
        int64_t timeout_ms;              // 超时时间（毫秒）
        TimeoutCallback callback;        // 超时回调
        bool is_periodic = false;        // 是否周期性超时
        int64_t period_ms = 0;           // 周期（如果 is_periodic=true）
        std::string tag;                 // 标签（用于调试）
        
        TimeoutConfig() = default;
        TimeoutConfig(int64_t ms, TimeoutCallback cb) 
            : timeout_ms(ms), callback(cb) {}
    };
    
    // 超时统计信息
    struct TimeoutStats {
        size_t total_registered = 0;     // 总注册数
        size_t active_timeouts = 0;      // 活跃超时数
        size_t triggered_count = 0;      // 已触发数
        size_t cancelled_count = 0;      // 已取消数
        double avg_trigger_time_ms = 0.0; // 平均触发时间
    };
    
    /**
     * @brief 获取单例实例
     */
    static TimeoutManager& instance();
    
    /**
     * @brief 注册一次性超时
     * @param timeout_ms 超时时间（毫秒）
     * @param callback 超时回调
     * @param tag 标签（可选，用于调试）
     * @return 超时 ID，用于取消
     */
    TimeoutId register_timeout(int64_t timeout_ms, TimeoutCallback callback, 
                               const std::string& tag = "");
    
    /**
     * @brief 注册周期性超时
     * @param period_ms 周期时间（毫秒）
     * @param callback 回调函数
     * @param tag 标签（可选）
     * @return 超时 ID
     */
    TimeoutId register_periodic_timeout(int64_t period_ms, TimeoutCallback callback,
                                        const std::string& tag = "");
    
    /**
     * @brief 取消超时
     * @param timeout_id 超时 ID
     * @return true=成功取消，false=不存在或已触发
     */
    bool cancel_timeout(TimeoutId timeout_id);
    
    /**
     * @brief 检查并处理所有超时的超时
     * @return 处理的超时数量
     */
    size_t process_timeouts();
    
    /**
     * @brief 获取下一个超时的剩余时间
     * @return 剩余毫秒数，如果没有超时则返回 -1
     */
    int64_t get_next_timeout_ms() const;
    
    /**
     * @brief 获取统计信息
     */
    TimeoutStats get_stats() const;
    
    /**
     * @brief 重置统计信息
     */
    void reset_stats();
    
    /**
     * @brief 获取活跃的超时数量
     */
    size_t get_active_timeout_count() const;
    
    /**
     * @brief 清空所有超时
     */
    void clear_all();
    
    // 禁止拷贝和移动
    TimeoutManager(const TimeoutManager&) = delete;
    TimeoutManager& operator=(const TimeoutManager&) = delete;
    TimeoutManager(TimeoutManager&&) = delete;
    TimeoutManager& operator=(TimeoutManager&&) = delete;
    
private:
    TimeoutManager() = default;
    ~TimeoutManager() = default;
    
    // 内部超时条目
    struct TimeoutEntry {
        TimeoutId id;
        std::chrono::steady_clock::time_point expire_time;
        TimeoutConfig config;
        bool is_cancelled = false;
        
        TimeoutEntry() = default;
        TimeoutEntry(TimeoutId id_, std::chrono::steady_clock::time_point expire,
                     const TimeoutConfig& cfg)
            : id(id_), expire_time(expire), config(cfg) {}
        
        // 用于优先级队列（早到期的优先）
        bool operator>(const TimeoutEntry& other) const {
            return expire_time > other.expire_time;
        }
    };
    
    // 生成唯一的超时 ID
    TimeoutId generate_timeout_id();
    
    // 数据成员
    mutable std::mutex mtx_;
    std::priority_queue<TimeoutEntry, std::vector<TimeoutEntry>, std::greater<TimeoutEntry>> timeout_queue_;
    std::unordered_map<TimeoutId, TimeoutEntry> timeout_map_;  // 快速查找
    std::atomic<TimeoutId> next_id_{1};
    
    // 统计信息
    mutable std::mutex stats_mtx_;
    struct Stats {
        std::atomic<size_t> total_registered{0};
        std::atomic<size_t> triggered_count{0};
        std::atomic<size_t> cancelled_count{0};
        std::atomic<int64_t> total_trigger_time_ms{0};
    } stats_;
};

#endif // TIMEOUT_MANAGER_H
