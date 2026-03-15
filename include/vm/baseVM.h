#ifndef LIVS_VM_BASEVM_H
#define LIVS_VM_BASEVM_H

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include "../router/MessageProtocol.h"
#include "../router/RouterCore.h"
// VmManager在实现文件中包含

// ===== VM 状态 =====
enum class VMState {
    CREATED,
    RUNNING,
    SUSPENDED_WAITING_INTERRUPT,
    TERMINATED
};

// ===== 调度上下文（新增）=====
struct VMScheduleCtx {
    int priority = 0;                    // 优先级（0=普通，1=高优 - 同步中断，-1=低优）
    bool has_quota = false;              // 是否有运行名额
    uint64_t instructions_executed = 0;  // 统计信息
    uint64_t vruntime = 0;               // 虚拟运行时间（用于公平调度）
    int blocked_reason = 0;              // 阻塞原因（0=无，1=等待外设，2=等待中断）
};

// ===== 指令码（仅 4 条）=====
enum class Opcode {
    NOP = 0,
    ADD = 1,      // ADD r1, r2  → R[r1] += R[r2]
    INPUT = 2,    // INPUT r1     → 触发 INT 1 (periph_id=1)，结果存入 R[r1]
    OUTPUT = 3,   // OUTPUT r1    → 触发 INT 2 (periph_id=2)，R[r1] 作为参数
    HALT = 4
};

// ===== 寄存器定义（仅 2 个）=====
constexpr size_t NUM_REGISTERS = 2;
using Register = int32_t;

class baseVM {
public:
    explicit baseVM(uint64_t vm_id) : vm_id_(vm_id), pc_(0), state_(VMState::CREATED) {}
    virtual ~baseVM() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual VMState get_state() const { return state_; }

    virtual bool execute_instruction() = 0;
    virtual void load_program(const std::vector<uint32_t>& code) = 0;

    virtual void handle_interrupt(const InterruptResult& result) = 0;

    // --- 调度接口（新增）---
    inline const VMScheduleCtx& get_sched_ctx() const { return sched_ctx_; }
    inline VMScheduleCtx& mutable_sched_ctx() { return sched_ctx_; }
    
    void grant_quota() { sched_ctx_.has_quota = true; }
    void revoke_quota() { sched_ctx_.has_quota = false; }
    bool has_quota() const { return sched_ctx_.has_quota; }

    // --- 通信：通过 Router 发送中断请求（经过 VM Manager）---
    void send_interrupt_request(int periph_id, int timeout_ms = 2000) {
        Message msg(vm_id_, MODULE_VM_MANAGER, MessageType::INTERRUPT_REQUEST);
        InterruptRequest req;
        req.vm_id = vm_id_;
        req.periph_id = periph_id;
        req.set_interrupt_type(InterruptType::SYSTEM);  // 使用 SYSTEM 类型
        req.timeout_ms = timeout_ms;
        msg.set_payload(req);
        route_send(msg);
    }

    uint64_t get_vm_id() const { return vm_id_; }
    Register get_register(size_t idx) const { 
        return idx < NUM_REGISTERS ? registers_[idx] : 0; 
    }
    void set_register(size_t idx, Register val) {
        if (idx < NUM_REGISTERS) registers_[idx] = val;
    }

protected:
    uint64_t vm_id_;
    size_t pc_;
    Register registers_[NUM_REGISTERS] = {0}; // R0, R1
    std::vector<uint32_t> memory_;
    VMState state_;
    VMScheduleCtx sched_ctx_;  // 调度上下文（新增）

    static Opcode decode_opcode(uint32_t inst) { return static_cast<Opcode>(inst & 0xFF); }
    static uint32_t decode_operand1(uint32_t inst) { return (inst >> 8) & 0xFF; }
    static uint32_t decode_operand2(uint32_t inst) { return (inst >> 16) & 0xFFFF; }
};

#endif // LIVS_VM_BASEVM_H