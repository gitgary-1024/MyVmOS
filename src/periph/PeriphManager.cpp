#include "periph/PeriphManager.h"
#include <iostream>
#include <algorithm>

PeriphManager::PeriphManager(int periph_count, int channels_per_periph)
    : peripherals_(periph_count) {
    for (int i = 0; i < periph_count; ++i) {
        peripherals_[i].gils.resize(channels_per_periph);
        for (int j = 0; j < channels_per_periph; ++j) {
            peripherals_[i].gils[j] = std::make_unique<PeriphGIL>(i+1, j);
        }
    }
}

PeriphManager::~PeriphManager() = default;

void PeriphManager::register_peripheral(int periph_id, const std::string& name) {
    if (periph_id > 0 && periph_id <= static_cast<int>(peripherals_.size())) {
        peripherals_[periph_id-1].name = name;
    }
}

PeriphGIL* PeriphManager::get_gil(int periph_id, int channel_id) {
    if (periph_id > 0 && periph_id <= static_cast<int>(peripherals_.size()) &&
        channel_id >= 0 && channel_id < static_cast<int>(peripherals_[periph_id-1].gils.size())) {
        return peripherals_[periph_id-1].gils[channel_id].get();
    }
    return nullptr;
}

void PeriphManager::wait_for_periph(uint64_t vm_id, int periph_id, int timeout_ms) {
    auto* gil = get_gil(periph_id, 0);
    if (gil && gil->try_lock(timeout_ms)) {
        return; // 成功获取
    }
    // 加入等待队列（默认 channel 0）
    std::lock_guard<std::mutex> lock(mtx_);
    auto key = std::make_pair(periph_id, 0);
    waiting_map_[key].push_back(vm_id);
}

void PeriphManager::wakeup_waiter(uint64_t vm_id, int periph_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto key = std::make_pair(periph_id, 0);
    auto it = waiting_map_.find(key);
    if (it != waiting_map_.end()) {
        auto& q = it->second;
        auto pos = std::find(q.begin(), q.end(), vm_id);
        if (pos != q.end()) {
            q.erase(pos);
        }
        if (q.empty()) {
            waiting_map_.erase(it);
        }
    }
}