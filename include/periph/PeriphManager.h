#ifndef LIVS_PERIPH_MANAGER_H
#define LIVS_PERIPH_MANAGER_H

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <map>
#include <utility>
#include "PeriphGIL.h"
#include "router/MessageProtocol.h"

// 前向声明
class BoundedWaitQueue;

class PeriphManager {
public:
    explicit PeriphManager(int periph_count = 3, int channels_per_periph = 2);
    ~PeriphManager();

    // 注册外设（由具体外设构造时调用）
    void register_peripheral(int periph_id, const std::string& name);

    // 获取指定外设通道的 GIL
    PeriphGIL* get_gil(int periph_id, int channel_id);

    // VM 等待外设（叫号模型入口）
    void wait_for_periph(uint64_t vm_id, int periph_id, int timeout_ms = 2000);

    // 外设处理完后唤醒等待者
    void wakeup_waiter(uint64_t vm_id, int periph_id);

    // 处理来自 Router 的中断请求（同步/异步）
    void handle_interrupt_request(const Message& msg);

private:
    struct PeripheralInfo {
        std::string name;
        std::vector<std::unique_ptr<PeriphGIL>> gils;
    };

    std::vector<PeripheralInfo> peripherals_;
    std::mutex mtx_;

    // 使用 map 存储等待队列，key = (periph_id, channel_id)
    std::map<std::pair<int, int>, std::vector<uint64_t>> waiting_map_;
};

#endif // LIVS_PERIPH_MANAGER_H