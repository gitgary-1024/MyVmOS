#ifndef LIVS_PERIPH_PERIPH_GIL_H
#define LIVS_PERIPH_PERIPH_GIL_H

#include <mutex>
#include <chrono>

class PeriphGIL {
public:
    explicit PeriphGIL(int periph_id, int channel_id)
        : periph_id_(periph_id), channel_id_(channel_id) {}

    bool try_lock(int timeout_ms) {
        auto dur = std::chrono::milliseconds(timeout_ms);
        return mtx_.try_lock_for(dur);
    }

    void unlock() {
        mtx_.unlock();
    }

    int get_periph_id() const { return periph_id_; }
    int get_channel_id() const { return channel_id_; }

private:
    const int periph_id_;
    const int channel_id_;
    std::timed_mutex mtx_;
};

#endif // LIVS_PERIPH_PERIPH_GIL_H