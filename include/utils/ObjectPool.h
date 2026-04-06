#ifndef LIVS_UTILS_MEMORY_POOL_H
#define LIVS_UTILS_MEMORY_POOL_H

#include <cstddef>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <iostream>
#include <stdexcept>

/**
 * @brief 通用对象池 - 用于高频对象的内存复用
 * 
 * 特性：
 * 1. 预分配对象池，避免频繁 new/delete
 * 2. 自动扩容（可选）
 * 3. 线程安全
 * 4. 支持最大容量限制（防堆爆炸）
 * 5. RAII 风格：使用 unique_ptr 管理对象生命周期
 * 
 * 使用场景：
 * - Message 对象复用
 * - InterruptTask 对象复用
 * - VMContext 等高频创建/销毁的对象
 * 
 * @tparam T 对象类型
 */
template<typename T>
class ObjectPool {
public:
    /**
     * @brief 构造对象池
     * @param initial_size 初始对象数量（默认 0，按需分配）
     * @param max_size 最大对象数量（0 表示无限制）
     * @param auto_expand 是否自动扩容（超过 max_size 时行为）
     */
    explicit ObjectPool(size_t initial_size = 0, 
                       size_t max_size = 0,
                       bool auto_expand = false);
    
    ~ObjectPool();
    
    // 禁止拷贝和移动
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;
    
    /**
     * @brief 获取一个对象
     * @return std::unique_ptr<T> 对象智能指针
     * 
     * 如果池中有空闲对象，直接复用；否则新建对象。
     * 如果达到最大容量且不自动扩容，返回 nullptr。
     */
    std::unique_ptr<T> acquire();
    
    /**
     * @brief 归还对象到池中
     * @param obj 要归还的对象
     * 
     * 归还的对象会被重置状态（调用 reset() 方法，如果存在）。
     */
    void release(std::unique_ptr<T> obj);
    
    /**
     * @brief 获取当前空闲对象数量
     * @return size_t 空闲对象数
     */
    size_t get_free_count() const;
    
    /**
     * @brief 获取已分配的对象数量（总对象数 - 空闲对象数）
     * @return size_t 已分配对象数
     */
    size_t get_allocated_count() const;
    
    /**
     * @brief 获取池的最大容量
     * @return size_t 最大容量
     */
    size_t get_max_capacity() const { return max_size_; }
    
    /**
     * @brief 清空池中所有空闲对象
     */
    void clear();
    
    /**
     * @brief 预分配对象
     * @param count 预分配数量
     */
    void reserve(size_t count);
    
private:
    // 创建新对象
    std::unique_ptr<T> create_object();
    
    // 空闲对象池
    std::vector<std::unique_ptr<T>> free_pool_;
    
    // 池互斥锁
    mutable std::mutex pool_mutex_;
    
    // 最大容量（0 表示无限制）
    size_t max_size_;
    
    // 是否自动扩容
    bool auto_expand_;
    
    // 已创建的总对象数（包括已分配和空闲）
    std::atomic<size_t> total_objects_{0};
    
    // 统计：累计分配次数
    std::atomic<size_t> total_allocations_{0};
    
    // 统计：从池中复用次数
    std::atomic<size_t> reuse_count_{0};
};

// ============================================================================
// 模板实现
// ============================================================================

template<typename T>
ObjectPool<T>::ObjectPool(size_t initial_size, size_t max_size, bool auto_expand)
    : max_size_(max_size), auto_expand_(auto_expand) {
    
    if (initial_size > 0) {
        reserve(initial_size);
    }
    
    std::cout << "[ObjectPool<" << typeid(T).name() << ">] Created with "
              << "initial=" << initial_size 
              << ", max=" << (max_size_ == 0 ? "unlimited" : std::to_string(max_size_))
              << ", auto_expand=" << (auto_expand_ ? "true" : "false")
              << std::endl;
}

template<typename T>
ObjectPool<T>::~ObjectPool() {
    clear();
    std::cout << "[ObjectPool<" << typeid(T).name() << ">] Destroyed. "
              << "Total allocations: " << total_allocations_.load()
              << ", Reuse count: " << reuse_count_.load()
              << std::endl;
}

template<typename T>
std::unique_ptr<T> ObjectPool<T>::acquire() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // 尝试从空闲池获取
    if (!free_pool_.empty()) {
        auto obj = std::move(free_pool_.back());
        free_pool_.pop_back();
        reuse_count_.fetch_add(1);
        return obj;
    }
    
    // 检查是否达到最大容量
    if (max_size_ > 0 && total_objects_.load() >= max_size_) {
        if (!auto_expand_) {
            // 达到容量上限且不自动扩容，返回 nullptr
            std::cerr << "[ObjectPool] Pool capacity reached (" 
                      << max_size_ << "), returning nullptr" << std::endl;
            return nullptr;
        }
        // 自动扩容：允许超过 max_size_，但打印警告
        std::cerr << "[ObjectPool] Warning: Pool expanded beyond max capacity ("
                  << max_size_ << ")" << std::endl;
    }
    
    // 创建新对象
    auto obj = create_object();
    total_objects_.fetch_add(1);
    total_allocations_.fetch_add(1);
    return obj;
}

template<typename T>
void ObjectPool<T>::release(std::unique_ptr<T> obj) {
    if (!obj) {
        return;
    }
    
    // 重置对象状态（如果 T 有 reset() 方法）
    // 使用 SFINAE 检测是否有 reset 方法
    auto* raw_ptr = obj.get();
    (void)raw_ptr;  // 避免未使用变量警告
    
    // 尝试调用 reset()（如果存在）
    #if defined(__cpp_lib_is_invocable) || defined(__has_feature)
    constexpr bool has_reset = std::is_invocable_v<void(T::*)(), T>;
    #else
    constexpr bool has_reset = false;
    #endif
    
    if constexpr (has_reset) {
        raw_ptr->reset();
    }
    
    // 归还到空闲池
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        
        // 检查是否超过最大容量
        if (max_size_ > 0 && free_pool_.size() + 1 > max_size_) {
            // 超过容量，直接丢弃（不归还）
            obj.reset();
            total_objects_.fetch_sub(1);
        } else {
            free_pool_.push_back(std::move(obj));
        }
    }
}

template<typename T>
size_t ObjectPool<T>::get_free_count() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    return free_pool_.size();
}

template<typename T>
size_t ObjectPool<T>::get_allocated_count() const {
    return total_objects_.load() - get_free_count();
}

template<typename T>
void ObjectPool<T>::clear() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    free_pool_.clear();
    total_objects_.store(0);
}

template<typename T>
void ObjectPool<T>::reserve(size_t count) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    while (free_pool_.size() < count) {
        if (max_size_ > 0 && total_objects_.load() >= max_size_) {
            break;  // 达到最大容量
        }
        
        auto obj = create_object();
        free_pool_.push_back(std::move(obj));
        total_objects_.fetch_add(1);
    }
}

template<typename T>
std::unique_ptr<T> ObjectPool<T>::create_object() {
    try {
        return std::make_unique<T>();
    } catch (const std::bad_alloc& e) {
        std::cerr << "[ObjectPool] Failed to allocate object: " << e.what() << std::endl;
        throw;
    }
}

#endif // LIVS_UTILS_MEMORY_POOL_H
