#include <iostream>
#include <thread>
#include <chrono>
#include "../include/vm/Scheduler/CFS_Scheduler.h"

using namespace std;

/**
 * @brief 验证 condition_variable 修复自旋锁问题
 */
void test_cv_stop_response() {
    cout << "=== 测试 condition_variable 停止响应 ===" << endl << endl;
    
    auto cfs_sched = make_shared<vm::CFSScheduler>();
    
    // 启动调度器
    cout << "[测试] 启动调度器..." << endl;
    cfs_sched->start();
    
    // 运行 100ms
    this_thread::sleep_for(chrono::milliseconds(100));
    
    cout << "[测试] 停止调度器（应该立即响应）..." << endl;
    auto start = chrono::steady_clock::now();
    
    cfs_sched->stop();
    
    auto end = chrono::steady_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    
    cout << "[测试] 停止耗时：" << duration << "ms" << endl;
    
    if (duration < 20) {
        cout << "[成功] condition_variable 工作正常，线程被立即唤醒！" << endl;
    } else {
        cout << "[失败] 停止耗时过长，可能存在问题" << endl;
    }
    cout << endl;
}

int main() {
    system("chcp 65001 > nul");
    test_cv_stop_response();
    return 0;
}
