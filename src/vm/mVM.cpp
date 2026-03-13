#include "vm/mVM.h"
#include "vm/VmManager.h"
#include "log/Logging.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>

mVM::mVM(uint64_t vm_id) : baseVM(vm_id), running_(false), pending_input_reg_(-1) {
    // 订阅 VM 唤醒通知
    route_subscribe(MessageType::VM_WAKEUP_NOTIFY, [this](const Message& msg) {
        if (msg.is_payload<VMWakeUpNotify>()) {
            auto* wake = msg.get_payload<VMWakeUpNotify>();
            if (wake->vm_id == this->vm_id_) {
                this->on_wakeup();
            }
        }
    });
    
    std::cout << "[VM " << vm_id << "] Created" << std::endl;
    LOG_INFO_MOD("VM", (std::string("Created VM ") + std::to_string(vm_id)).c_str());
}

void mVM::start() {
    if (running_.load(std::memory_order_relaxed)) return;
    
    // 注册到VM管理器
    // 注意：暂时不注册，避免智能指针问题
    // vm_manager_register_vm(std::shared_ptr<mVM>(this, [](mVM*){}));
    
    // 设置初始状态为RUNNING
    {
        std::lock_guard<std::mutex> lock(exec_mtx);
        state_ = VMState::RUNNING;
        LOG_INFO_MOD("VM", (std::string("VM ") + std::to_string(vm_id_) + " state set to RUNNING").c_str());
    }
    
    running_.store(true, std::memory_order_relaxed);
    vm_thread = std::thread(&mVM::run_loop, this);
}

void mVM::stop() {
    running_.store(false, std::memory_order_relaxed);
    if (vm_thread.joinable()) {
        vm_thread.join();
    }
    // 从VM管理器注销
    vm_manager_unregister_vm(vm_id_);
}

void mVM::load_program(const std::vector<uint32_t>& code) {
    std::lock_guard<std::mutex> lock(exec_mtx);
    memory_ = code;
    pc_ = 0;
}

void mVM::run_loop() {
    while (running_.load(std::memory_order_relaxed)) {
        // 检查是否处于挂起状态
        {
            std::lock_guard<std::mutex> lock(exec_mtx);
            if (state_ == VMState::SUSPENDED_WAITING_INTERRUPT) {
                // 挂起状态下不执行指令，只是等待
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }
        
        if (pc_ >= memory_.size()) {
            std::lock_guard<std::mutex> lock(exec_mtx);
            registers_[0] = -2;
            break;
        }

        uint32_t inst = memory_[pc_];
        pc_++;

        if (!execute_instruction()) {
            // 检查是否是因为挂起等待中断
            std::lock_guard<std::mutex> lock(exec_mtx);
            if (state_ == VMState::SUSPENDED_WAITING_INTERRUPT) {
                // 保持挂起状态，不设置为终止
                pc_--; // 回退PC，等待唤醒后重新执行当前指令
                continue; // 继续循环，等待中断唤醒
            }
            break;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    std::lock_guard<std::mutex> lock(exec_mtx);
    if (state_ != VMState::SUSPENDED_WAITING_INTERRUPT) {
        state_ = VMState::TERMINATED;
    }
}

bool mVM::execute_instruction() {
    std::lock_guard<std::mutex> lock(exec_mtx);

    if (pc_ > memory_.size()) return false;

    uint32_t inst = memory_[pc_ - 1];
    Opcode op = decode_opcode(inst);
    uint32_t r1 = decode_operand1(inst);
    uint32_t r2 = decode_operand2(inst);

    std::cout << "[VM " << vm_id_ << "] Executing instruction at PC=" << (pc_-1) 
              << ", raw_inst=0x" << std::hex << inst << std::dec
              << ", opcode=" << static_cast<int>(op) << ", r1=" << r1 << std::endl;
    LOG_INFO_MOD("VM", (std::string("Executing instruction at PC=") + std::to_string(pc_-1) + 
                       ", opcode=" + std::to_string(static_cast<int>(op)) + 
                       ", r1=" + std::to_string(r1)).c_str());

    std::cout << "[VM " << vm_id_ << "] Entering switch statement, op=" << static_cast<int>(op) << std::endl;
    LOG_INFO_MOD("VM", (std::string("Entering switch, op=") + std::to_string(static_cast<int>(op))).c_str());
    
    switch (op) {
        case Opcode::NOP:
            std::cout << "[VM " << vm_id_ << "] Executing NOP" << std::endl;
            LOG_INFO_MOD("VM", "Executing NOP");
            break;

        case Opcode::ADD:
            std::cout << "[VM " << vm_id_ << "] Executing ADD" << std::endl;
            LOG_INFO_MOD("VM", "Executing ADD");
            if (r1 < NUM_REGISTERS && r2 < NUM_REGISTERS) {
                registers_[r1] += registers_[r2];
            }
            break;

        case Opcode::INPUT:
            std::cout << "[VM " << vm_id_ << "] INPUT instruction: saving to reg " << r1 << std::endl;
            LOG_INFO_MOD("VM", (std::string("INPUT instruction: saving to reg ") + std::to_string(r1)).c_str());
            // ✅ 保存目标寄存器索引
            pending_input_reg_ = static_cast<int>(r1);
            send_interrupt_request(3, 2000); // periph_id=3 for Terminal
            state_ = VMState::SUSPENDED_WAITING_INTERRUPT;
            std::cout << "[VM " << vm_id_ << "] State changed to SUSPENDED_WAITING_INTERRUPT" << std::endl;
            LOG_INFO_MOD("VM", "State changed to SUSPENDED_WAITING_INTERRUPT");
            return false;

        case Opcode::OUTPUT:
            std::cout << "[VM " << vm_id_ << "] Executing OUTPUT" << std::endl;
            LOG_INFO_MOD("VM", "Executing OUTPUT");
            send_interrupt_request(3, 2000); // periph_id=3 for Terminal
            state_ = VMState::SUSPENDED_WAITING_INTERRUPT;
            return false;

        case Opcode::HALT:
            std::cout << "[VM " << vm_id_ << "] HALT instruction" << std::endl;
            LOG_INFO_MOD("VM", "HALT instruction");
            return false;

        default:
            std::cout << "[VM " << vm_id_ << "] Unknown opcode: " << static_cast<int>(op) << std::endl;
            LOG_INFO_MOD("VM", (std::string("Unknown opcode: ") + std::to_string(static_cast<int>(op))).c_str());
            registers_[0] = -3;
            return false;
    }
    
    std::cout << "[VM " << vm_id_ << "] Exiting switch statement" << std::endl;
    LOG_INFO_MOD("VM", "Exiting switch statement");

    return true;
}

void mVM::handle_interrupt(const InterruptResult& result) {
    std::cout << "[VM " << vm_id_ << "] handle_interrupt called with return_value=" << result.return_value << std::endl;
    LOG_INFO_MOD("VM", (std::string("handle_interrupt called with return_value=") + std::to_string(result.return_value)).c_str());
    std::lock_guard<std::mutex> lock(exec_mtx);
    if (result.is_timeout) {
        registers_[0] = -1;
    } else {
        // ✅ 修复：存入 pending_input_reg_ 指定的寄存器，而非总是 R0
        if (pending_input_reg_ >= 0 && pending_input_reg_ < static_cast<int>(NUM_REGISTERS)) {
            registers_[pending_input_reg_] = result.return_value;
            std::cout << "[VM " << vm_id_ << "] Stored " << result.return_value << " in register " << pending_input_reg_ << std::endl;
            LOG_INFO_MOD("VM", (std::string("Stored ") + std::to_string(result.return_value) + " in register " + std::to_string(pending_input_reg_)).c_str());
        }
    }
    pending_input_reg_ = -1; // 重置
    state_ = VMState::RUNNING; // 恢复运行状态
    std::cout << "[VM " << vm_id_ << "] State changed to RUNNING" << std::endl;
    LOG_INFO_MOD("VM", "State changed to RUNNING");

    // 发送唤醒消息
    Message notify_msg(vm_id_, MODULE_SCHEDULER, MessageType::VM_WAKEUP_NOTIFY);
    VMWakeUpNotify wake{vm_id_};
    notify_msg.set_payload(wake);
    std::cout << "[VM " << vm_id_ << "] Sending VM_WAKEUP_NOTIFY" << std::endl;
    LOG_INFO_MOD("VM", "Sending VM_WAKEUP_NOTIFY");
    route_send(notify_msg);
}

void mVM::on_wakeup() {
    std::lock_guard<std::mutex> lock(exec_mtx);
    if (state_ == VMState::SUSPENDED_WAITING_INTERRUPT) {
        state_ = VMState::RUNNING;
        std::cout << "[VM " << vm_id_ << "] Woken up from interrupt wait" << std::endl;
        LOG_INFO_MOD("VM", "Woken up from interrupt wait");
    }
}