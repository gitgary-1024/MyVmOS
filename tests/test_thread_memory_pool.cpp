#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include "utils/ThreadPool.h"
#include "utils/ObjectPool.h"

using namespace std;

// ============================================================================
// 测试 ThreadPool
// ============================================================================

void test_thread_pool_basic() {
    cout << "\n=== Test 1: ThreadPool Basic Functionality ===" << endl;
    
    ThreadPool pool(4);  // 4 个工作线程
    
    // 提交简单任务
    pool.submit([]() {
        cout << "[Task 1] Hello from thread pool" << endl;
    });
    
    // 提交带参数的任务
    auto future = pool.submit([](int x, int y) {
        return x + y;
    }, 3, 4);
    
    cout << "Result: " << future.get() << endl;
    
    // 提交多个任务
    atomic<int> counter{0};
    for (int i = 0; i < 10; ++i) {
        pool.submit([&counter, i]() {
            counter.fetch_add(1);
            cout << "[Task " << i << "] Counter: " << counter.load() << endl;
        });
    }
    
    this_thread::sleep_for(chrono::milliseconds(100));
    cout << "Final counter: " << counter.load() << endl;
}

void test_thread_pool_concurrent() {
    cout << "\n=== Test 2: ThreadPool Concurrent Tasks ===" << endl;
    
    ThreadPool pool(4);
    atomic<int> task_count{0};
    
    cout << "Submitting 20 tasks..." << endl;
    auto start = chrono::steady_clock::now();
    
    for (int i = 0; i < 20; ++i) {
        pool.submit([&task_count, i]() {
            this_thread::sleep_for(chrono::milliseconds(50));  // 模拟耗时任务
            task_count.fetch_add(1);
            cout << "[Task " << i << "] Completed by thread " 
                 << this_thread::get_id() << endl;
        });
    }
    
    // 等待所有任务完成
    while (task_count.load() < 20) {
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    
    auto end = chrono::steady_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    
    cout << "All 20 tasks completed in " << duration << "ms" << endl;
    cout << "Expected time with 4 threads: ~250ms (20/4 * 50ms)" << endl;
}

void test_thread_pool_graceful_shutdown() {
    cout << "\n=== Test 3: ThreadPool Graceful Shutdown ===" << endl;
    
    {
        ThreadPool pool(2);
        
        for (int i = 0; i < 5; ++i) {
            pool.submit([i]() {
                cout << "[Task " << i << "] Starting..." << endl;
                this_thread::sleep_for(chrono::milliseconds(100));
                cout << "[Task " << i << "] Completed" << endl;
            });
        }
        
        cout << "Tasks submitted, pool will wait for completion on destroy..." << endl;
    }  // pool 析构函数会等待所有任务完成
    
    cout << "Pool destroyed successfully" << endl;
}

// ============================================================================
// 测试 ObjectPool
// ============================================================================

// 测试用对象
class TestObject {
public:
    TestObject() : id_(next_id_++), data_(0) {
        creation_count_.fetch_add(1);
    }
    
    ~TestObject() {
        destruction_count_.fetch_add(1);
    }
    
    void reset() {
        data_ = 0;
    }
    
    void set_data(int value) {
        data_ = value;
    }
    
    int get_data() const {
        return data_;
    }
    
    static void reset_stats() {
        creation_count_.store(0);
        destruction_count_.store(0);
    }
    
    static int get_creation_count() {
        return creation_count_.load();
    }
    
    static int get_destruction_count() {
        return destruction_count_.load();
    }
    
private:
    int id_;
    int data_;
    static atomic<int> creation_count_;
    static atomic<int> destruction_count_;
    static atomic<int> next_id_;
};

// 静态成员初始化
atomic<int> TestObject::creation_count_{0};
atomic<int> TestObject::destruction_count_{0};
atomic<int> TestObject::next_id_{1};

void test_object_pool_basic() {
    cout << "\n=== Test 4: ObjectPool Basic Functionality ===" << endl;
    
    TestObject::reset_stats();
    ObjectPool<TestObject> pool(5, 10);  // 初始 5 个，最大 10 个
    
    cout << "Initial free count: " << pool.get_free_count() << endl;
    cout << "Total created so far: " << TestObject::get_creation_count() << endl;
    
    // 获取对象
    auto obj1 = pool.acquire();
    obj1->set_data(100);
    cout << "Acquired obj1, data=" << obj1->get_data() << endl;
    
    auto obj2 = pool.acquire();
    obj2->set_data(200);
    cout << "Acquired obj2, data=" << obj2->get_data() << endl;
    
    cout << "Free count after acquire: " << pool.get_free_count() << endl;
    
    // 归还对象
    pool.release(std::move(obj1));
    cout << "Released obj1, free count: " << pool.get_free_count() << endl;
    
    // 再次获取（应该复用）
    auto obj3 = pool.acquire();
    cout << "Acquired obj3 (reused?), data=" << obj3->get_data() 
         << " (should be 0 after reset)" << endl;
    
    cout << "Total objects created: " << TestObject::get_creation_count() << endl;
    cout << "Allocated count: " << pool.get_allocated_count() << endl;
}

void test_object_pool_capacity() {
    cout << "\n=== Test 5: ObjectPool Capacity Limit ===" << endl;
    
    TestObject::reset_stats();
    ObjectPool<TestObject> pool(0, 5, false);  // 无预分配，最大 5 个，不自动扩容
    
    vector<unique_ptr<TestObject>> objects;
    
    cout << "Acquiring 5 objects (max capacity)..." << endl;
    for (int i = 0; i < 5; ++i) {
        auto obj = pool.acquire();
        if (obj) {
            objects.push_back(std::move(obj));
            cout << "Acquired object " << i << endl;
        } else {
            cout << "Failed to acquire object " << i << " (pool full)" << endl;
        }
    }
    
    cout << "Trying to acquire 6th object (should fail)..." << endl;
    auto obj6 = pool.acquire();
    if (!obj6) {
        cout << "Correctly returned nullptr (pool at capacity)" << endl;
    }
    
    // 释放一个对象
    cout << "Releasing one object..." << endl;
    pool.release(std::move(objects[0]));
    objects.erase(objects.begin());
    
    cout << "Trying again (should succeed)..." << endl;
    auto obj7 = pool.acquire();
    if (obj7) {
        cout << "Successfully acquired recycled object" << endl;
    }
}

void test_object_pool_auto_expand() {
    cout << "\n=== Test 6: ObjectPool Auto Expand ===" << endl;
    
    TestObject::reset_stats();
    ObjectPool<TestObject> pool(0, 3, true);  // 最大 3 个，自动扩容
    
    cout << "Acquiring 5 objects (exceeds max=3, should auto-expand)..." << endl;
    for (int i = 0; i < 5; ++i) {
        auto obj = pool.acquire();
        if (obj) {
            cout << "Acquired object " << i << endl;
        } else {
            cout << "Failed to acquire object " << i << endl;
        }
    }
    
    cout << "Total objects created (should be 5 despite max=3): " 
         << TestObject::get_creation_count() << endl;
}

void test_object_pool_performance() {
    cout << "\n=== Test 7: ObjectPool Performance Comparison ===" << endl;
    
    const int ITERATIONS = 10000;
    
    // 测试对象池性能
    TestObject::reset_stats();
    ObjectPool<TestObject> pool(100, 0);  // 预分配 100 个
    
    auto start = chrono::steady_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto obj = pool.acquire();
        obj->set_data(i);
        pool.release(std::move(obj));
    }
    auto pool_time = chrono::steady_clock::now() - start;
    
    // 测试直接 new/delete 性能
    start = chrono::steady_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto* obj = new TestObject();
        obj->set_data(i);
        delete obj;
    }
    auto raw_time = chrono::steady_clock::now() - start;
    
    cout << "ObjectPool time: " 
         << chrono::duration_cast<chrono::milliseconds>(pool_time).count() 
         << "ms" << endl;
    cout << "Raw new/delete time: " 
         << chrono::duration_cast<chrono::milliseconds>(raw_time).count() 
         << "ms" << endl;
    cout << "ObjectPool created " << TestObject::get_creation_count() 
         << " objects (should be close to 100 due to reuse)" << endl;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    cout << "========================================" << endl;
    cout << "  ThreadPool & ObjectPool Test Suite  " << endl;
    cout << "========================================" << endl;
    
    try {
        // ThreadPool 测试
        test_thread_pool_basic();
        test_thread_pool_concurrent();
        test_thread_pool_graceful_shutdown();
        
        // ObjectPool 测试
        test_object_pool_basic();
        test_object_pool_capacity();
        test_object_pool_auto_expand();
        test_object_pool_performance();
        
        cout << "\n========================================" << endl;
        cout << "  All tests completed successfully!   " << endl;
        cout << "========================================" << endl;
        
    } catch (const exception& e) {
        cerr << "\n❌ Test failed with exception: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
