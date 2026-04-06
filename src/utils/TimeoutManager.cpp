#include "TimeoutManager.h"
#include <iostream>
#include <algorithm>

TimeoutManager& TimeoutManager::instance() {
    static TimeoutManager instance;
    return instance;
}

TimeoutManager::TimeoutId TimeoutManager::generate_timeout_id() {
    return next_id_.fetch_add(1, std::memory_order_relaxed);
}

TimeoutManager::TimeoutId TimeoutManager::register_timeout(
    int64_t timeout_ms, TimeoutCallback callback, const std::string& tag) {
    
    if (timeout_ms <= 0 || !callback) {
        std::cerr << "[TimeoutManager] Invalid timeout parameters: ms=" 
                  << timeout_ms << ", callback=" << (callback ? "valid" : "null") << std::endl;
        return 0;  // 返回 0 表示失败
    }
    
    TimeoutId id = generate_timeout_id();
    auto expire_time = std::chrono::steady_clock::now() + 
                       std::chrono::milliseconds(timeout_ms);
    
    TimeoutConfig config;
    config.timeout_ms = timeout_ms;
    config.callback = callback;
    config.is_periodic = false;
    config.tag = tag;
    
    TimeoutEntry entry{id, expire_time, config};
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        timeout_queue_.push(entry);
        timeout_map_[id] = entry;
    }
    
    // 更新统计
    stats_.total_registered++;
    
    if (!tag.empty()) {
        std::cout << "[TimeoutManager] Registered timeout #" << id 
                  << ": " << tag << " (" << timeout_ms << "ms)" << std::endl;
    }
    
    return id;
}

TimeoutManager::TimeoutId TimeoutManager::register_periodic_timeout(
    int64_t period_ms, TimeoutCallback callback, const std::string& tag) {
    
    if (period_ms <= 0 || !callback) {
        std::cerr << "[TimeoutManager] Invalid periodic timeout parameters: period=" 
                  << period_ms << std::endl;
        return 0;
    }
    
    TimeoutId id = generate_timeout_id();
    auto expire_time = std::chrono::steady_clock::now() + 
                       std::chrono::milliseconds(period_ms);
    
    TimeoutConfig config;
    config.timeout_ms = period_ms;
    config.callback = callback;
    config.is_periodic = true;
    config.period_ms = period_ms;
    config.tag = tag;
    
    TimeoutEntry entry{id, expire_time, config};
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        timeout_queue_.push(entry);
        timeout_map_[id] = entry;
    }
    
    // 更新统计
    stats_.total_registered++;
    
    if (!tag.empty()) {
        std::cout << "[TimeoutManager] Registered periodic timeout #" << id 
                  << ": " << tag << " (period=" << period_ms << "ms)" << std::endl;
    }
    
    return id;
}

bool TimeoutManager::cancel_timeout(TimeoutId timeout_id) {
    if (timeout_id == 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mtx_);
    
    auto it = timeout_map_.find(timeout_id);
    if (it == timeout_map_.end()) {
        return false;  // 不存在
    }
    
    // 标记为已取消（不会从优先级队列中移除，但处理时会跳过）
    it->second.is_cancelled = true;
    
    // 更新统计
    stats_.cancelled_count++;
    
    if (!it->second.config.tag.empty()) {
        std::cout << "[TimeoutManager] Cancelled timeout #" << timeout_id 
                  << ": " << it->second.config.tag << std::endl;
    }
    
    return true;
}

size_t TimeoutManager::process_timeouts() {
    size_t processed = 0;
    auto now = std::chrono::steady_clock::now();
    
    std::vector<TimeoutEntry> triggered;
    std::vector<TimeoutEntry> reschedule;  // 需要重新调度的周期性超时
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        
        // 收集所有已超时的条目
        while (!timeout_queue_.empty()) {
            const auto& top = timeout_queue_.top();
            
            // 如果最早的超时还没到，停止
            if (top.expire_time > now) {
                break;
            }
            
            TimeoutEntry entry = top;
            timeout_queue_.pop();
            
            // 从 map 中检查是否已被取消
            auto it = timeout_map_.find(entry.id);
            if (it == timeout_map_.end()) {
                // 不存在，跳过
                continue;
            }
            
            if (it->second.is_cancelled) {
                // 已取消，从 map 中移除并跳过
                timeout_map_.erase(it);
                continue;
            }
            
            // 从 map 中移除
            timeout_map_.erase(it);
            
            triggered.push_back(entry);
            
            // 如果是周期性的，重新调度
            if (entry.config.is_periodic) {
                entry.expire_time = now + std::chrono::milliseconds(entry.config.period_ms);
                entry.is_cancelled = false;
                reschedule.push_back(entry);
            }
        }
        
        // 重新添加周期性超时
        for (auto& entry : reschedule) {
            timeout_queue_.push(entry);
            timeout_map_[entry.id] = entry;
        }
    }
    
    // 执行回调（在锁外，避免死锁）
    for (auto& entry : triggered) {
        try {
            entry.config.callback();
            processed++;
            
            // 更新统计
            stats_.triggered_count++;
            
            auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - (now - std::chrono::milliseconds(entry.config.timeout_ms)));
            stats_.total_trigger_time_ms += actual_duration.count();
            
            if (!entry.config.tag.empty()) {
                std::cout << "[TimeoutManager] Triggered timeout #" << entry.id 
                          << ": " << entry.config.tag << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[TimeoutManager] Exception in timeout callback #" 
                      << entry.id << ": " << e.what() << std::endl;
        }
    }
    
    return processed;
}

int64_t TimeoutManager::get_next_timeout_ms() const {
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (timeout_queue_.empty()) {
        return -1;  // 没有超时
    }
    
    auto now = std::chrono::steady_clock::now();
    const auto& top = timeout_queue_.top();
    
    if (top.expire_time <= now) {
        return 0;  // 已经超时
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        top.expire_time - now);
    
    return duration.count();
}

TimeoutManager::TimeoutStats TimeoutManager::get_stats() const {
    TimeoutStats stats;
    stats.total_registered = stats_.total_registered.load();
    stats.triggered_count = stats_.triggered_count.load();
    stats.cancelled_count = stats_.cancelled_count.load();
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stats.active_timeouts = timeout_map_.size();
    }
    
    if (stats.triggered_count > 0) {
        stats.avg_trigger_time_ms = static_cast<double>(stats_.total_trigger_time_ms.load()) / 
                                    static_cast<double>(stats.triggered_count);
    }
    
    return stats;
}

void TimeoutManager::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mtx_);
    stats_.total_registered = 0;
    stats_.triggered_count = 0;
    stats_.cancelled_count = 0;
    stats_.total_trigger_time_ms = 0;
}

size_t TimeoutManager::get_active_timeout_count() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return timeout_map_.size();
}

void TimeoutManager::clear_all() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // 清空队列和 map
    while (!timeout_queue_.empty()) {
        timeout_queue_.pop();
    }
    timeout_map_.clear();
    
    std::cout << "[TimeoutManager] Cleared all timeouts" << std::endl;
}
