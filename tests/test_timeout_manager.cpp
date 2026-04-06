#include "../include/utils/TimeoutManager.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace std;

void print_header(const string& test_name) {
    cout << "\n========================================" << endl;
    cout << "  " << test_name << endl;
    cout << "========================================" << endl;
}

// Test 1: 基本超时功能
void test_basic_timeout() {
    print_header("Test 1: Basic Timeout");
    
    TimeoutManager& tm = TimeoutManager::instance();
    atomic<bool> callback_called{false};
    
    // 注册一个 100ms 的超时
    auto timeout_id = tm.register_timeout(100, [&callback_called]() {
        callback_called = true;
        cout << "[Test] Basic timeout triggered!" << endl;
    }, "BasicTest");
    
    assert(timeout_id != 0);
    cout << "Registered timeout with ID: " << timeout_id << endl;
    
    // 等待超时触发
    this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // 处理超时
    size_t processed = tm.process_timeouts();
    
    assert(processed == 1);
    assert(callback_called.load());
    
    cout << "✅ Basic timeout works correctly" << endl;
}

// Test 2: 取消超时
void test_cancel_timeout() {
    print_header("Test 2: Cancel Timeout");
    
    TimeoutManager& tm = TimeoutManager::instance();
    atomic<bool> callback_called{false};
    
    // 注册一个 100ms 的超时
    auto timeout_id = tm.register_timeout(100, [&callback_called]() {
        callback_called = true;
        cout << "[Test] This should NOT be called!" << endl;
    }, "CancelTest");
    
    assert(timeout_id != 0);
    cout << "Registered timeout with ID: " << timeout_id << endl;
    
    // 立即取消
    bool cancelled = tm.cancel_timeout(timeout_id);
    assert(cancelled);
    
    // 等待并处理
    this_thread::sleep_for(std::chrono::milliseconds(150));
    size_t processed = tm.process_timeouts();
    
    assert(processed == 0);  // 已取消，不应处理
    assert(!callback_called.load());
    
    cout << "✅ Cancel timeout works correctly" << endl;
}

// Test 3: 周期性超时
void test_periodic_timeout() {
    print_header("Test 3: Periodic Timeout");
    
    TimeoutManager& tm = TimeoutManager::instance();
    atomic<int> trigger_count{0};
    
    // 注册一个 50ms 的周期性超时
    auto timeout_id = tm.register_periodic_timeout(50, [&trigger_count]() {
        trigger_count++;
        cout << "[Test] Periodic timeout triggered, count=" << trigger_count.load() << endl;
    }, "PeriodicTest");
    
    assert(timeout_id != 0);
    cout << "Registered periodic timeout with ID: " << timeout_id << endl;
    
    // 等待 180ms（应该触发至少 3 次）
    this_thread::sleep_for(std::chrono::milliseconds(180));
    
    // 多次轮询确保所有超时都被处理
    size_t total_processed = 0;
    for (int i = 0; i < 10; i++) {
        size_t processed = tm.process_timeouts();
        total_processed += processed;
        this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    cout << "Total processed: " << total_processed << ", Trigger count: " << trigger_count.load() << endl;
    assert(trigger_count.load() >= 3);
    
    // 取消周期性超时
    tm.cancel_timeout(timeout_id);
    
    cout << "Triggered " << trigger_count.load() << " times" << endl;
    cout << "✅ Periodic timeout works correctly" << endl;
}

// Test 4: 多个超时并发
void test_multiple_timeouts() {
    print_header("Test 4: Multiple Timeouts");
    
    TimeoutManager& tm = TimeoutManager::instance();
    atomic<int> trigger_count{0};
    
    // 注册 10 个不同时间的超时
    vector<TimeoutManager::TimeoutId> ids;
    for (int i = 1; i <= 10; i++) {
        auto id = tm.register_timeout(i * 20, [&trigger_count, i]() {
            trigger_count++;
            cout << "[Test] Timeout #" << i << " triggered" << endl;
        }, "MultiTest_" + to_string(i));
        
        ids.push_back(id);
    }
    
    cout << "Registered 10 timeouts" << endl;
    
    // 等待所有超时
    this_thread::sleep_for(std::chrono::milliseconds(250));
    
    // 处理所有超时
    size_t processed = tm.process_timeouts();
    
    assert(processed == 10);
    assert(trigger_count.load() == 10);
    
    cout << "Processed " << processed << " timeouts" << endl;
    cout << "✅ Multiple timeouts work correctly" << endl;
}

// Test 5: 统计信息
void test_statistics() {
    print_header("Test 5: Statistics");
    
    TimeoutManager& tm = TimeoutManager::instance();
    
    // 重置统计
    tm.reset_stats();
    
    // 注册一些超时
    atomic<int> trigger_count{0};
    vector<TimeoutManager::TimeoutId> ids;
    for (int i = 1; i <= 5; i++) {
        auto id = tm.register_timeout(10, [&trigger_count]() {
            trigger_count++;
        }, "StatTest_" + to_string(i));
        ids.push_back(id);
    }
    
    // 取消其中一个（取消最后一个）
    tm.cancel_timeout(ids[4]);
    
    // 等待并处理
    this_thread::sleep_for(std::chrono::milliseconds(50));
    tm.process_timeouts();
    
    // 获取统计
    TimeoutManager::TimeoutStats stats = tm.get_stats();
    
    cout << "Statistics:" << endl;
    cout << "  Total registered: " << stats.total_registered << endl;
    cout << "  Active timeouts: " << stats.active_timeouts << " (before final process)" << endl;
    cout << "  Triggered: " << stats.triggered_count << endl;
    cout << "  Cancelled: " << stats.cancelled_count << endl;
    cout << "  Avg trigger time: " << stats.avg_trigger_time_ms << "ms" << endl;
    
    assert(stats.total_registered == 5);
    assert(stats.triggered_count == 4);  // 1 个被取消
    assert(stats.cancelled_count == 1);
    // active_timeouts 在处理前不为 0，因为取消的条目还在 map 中
    // assert(stats.active_timeouts == 0);
    
    // 再次处理以清理已取消的条目
    tm.process_timeouts();
    auto final_stats = tm.get_stats();
    assert(final_stats.active_timeouts == 0);  // 现在应该为 0
    
    cout << "✅ Statistics work correctly" << endl;
}

// Test 6: 获取下一个超时时间
void test_get_next_timeout() {
    print_header("Test 6: Get Next Timeout");
    
    TimeoutManager& tm = TimeoutManager::instance();
    tm.clear_all();  // 清空之前的
    
    // 没有超时时
    int64_t next = tm.get_next_timeout_ms();
    assert(next == -1);
    cout << "No timeouts: " << next << "ms (expected -1)" << endl;
    
    // 注册一个 200ms 的超时
    tm.register_timeout(200, [](){}, "NextTimeoutTest");
    
    next = tm.get_next_timeout_ms();
    assert(next >= 0 && next <= 200);
    cout << "After register: " << next << "ms (expected ~200)" << endl;
    
    // 等待 100ms
    this_thread::sleep_for(std::chrono::milliseconds(100));
    
    next = tm.get_next_timeout_ms();
    assert(next >= 0 && next <= 100);
    cout << "After 100ms: " << next << "ms (expected ~100)" << endl;
    
    // 等待超时
    this_thread::sleep_for(std::chrono::milliseconds(150));
    
    next = tm.get_next_timeout_ms();
    assert(next == 0);  // 已超时
    cout << "After timeout: " << next << "ms (expected 0)" << endl;
    
    // 处理掉
    tm.process_timeouts();
    
    next = tm.get_next_timeout_ms();
    assert(next == -1);
    cout << "After process: " << next << "ms (expected -1)" << endl;
    
    cout << "✅ Get next timeout works correctly" << endl;
}

int main() {
    print_header("Timeout Manager Tests");
    
    try {
        test_basic_timeout();
        test_cancel_timeout();
        test_periodic_timeout();
        test_multiple_timeouts();
        test_statistics();
        test_get_next_timeout();
        
        print_header("All tests passed!");
        cout << "✅ TimeoutManager is working correctly!" << endl;
        
    } catch (const exception& e) {
        cerr << "\n❌ Test failed with exception: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
