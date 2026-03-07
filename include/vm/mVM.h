#ifndef LIVS_VM_MVM_H
#define LIVS_VM_MVM_H

#include "baseVM.h"
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

class mVM : public baseVM {
public:
    explicit mVM(uint64_t vm_id);

    void start() override;
    void stop() override;

    bool execute_instruction() override;
    void load_program(const std::vector<uint32_t>& code) override;

    void handle_interrupt(const InterruptResult& result) override;

    size_t get_pc() const { return pc_; }

private:
    void on_wakeup();

private:
    std::thread vm_thread;
    std::atomic<bool> running_;
    std::mutex exec_mtx;
    int pending_input_reg_; // ✅ 新增：记录 INPUT 指令的目标寄存器

    void run_loop();
};

#endif // LIVS_VM_MVM_H