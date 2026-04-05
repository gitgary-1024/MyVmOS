#include "X86CPU.h"
#include <iostream>
#include <cstring>
#include <iomanip>

// ===== 构造函数 =====
X86CPUVM::X86CPUVM(uint64_t vm_id, const X86VMConfig& config)
    : baseVM(vm_id)  // 调用 baseVM 构造函数，初始化 vm_id_
    , config_(config)
    , registers_{}
    , physical_memory_(config.memory_size)
    , state_(X86VMState::CREATED)
    , running_(false)
    , pending_interrupt_vector_(0)
    , interrupt_pending_(false)
    , total_instructions_(0)
    , total_cycles_(0)
{
    // 初始化寄存器
    std::fill(registers_.begin(), registers_.end(), 0);
    
    // 设置初始 RIP 和 RSP
    set_rip(config.entry_point);
    set_rsp(config.stack_pointer);
    set_rbp(0);
    
    // RFLAGS: 中断使能 (IF=1)
    set_register(X86Reg::RFLAGS, FLAG_IF);
}

X86CPUVM::~X86CPUVM() {
    stop();
}

// ===== 核心控制 =====
void X86CPUVM::start() {
    if (state_ == X86VMState::RUNNING) return;
    
    state_ = X86VMState::RUNNING;
    running_ = true;
    
    std::cout << "[X86VM-" << vm_id_ << "] Started at RIP=0x" 
              << std::hex << get_rip() << std::dec << std::endl;
}

void X86CPUVM::stop() {
    running_ = false;
    state_ = X86VMState::TERMINATED;
    
    std::cout << "[X86VM-" << vm_id_ << "] Stopped. Total instructions: " 
              << total_instructions_ << std::endl;
}

void X86CPUVM::reset() {
    std::fill(registers_.begin(), registers_.end(), 0);
    std::fill(physical_memory_.begin(), physical_memory_.end(), 0);
    
    set_rip(config_.entry_point);
    set_rsp(config_.stack_pointer);
    set_register(X86Reg::RFLAGS, FLAG_IF);
    
    state_ = X86VMState::CREATED;
    running_ = false;
    total_instructions_ = 0;
    total_cycles_ = 0;
}

// ===== 执行接口 =====
bool X86CPUVM::execute_instruction() {
    if (!running_ || state_ != X86VMState::RUNNING) {
        return false;
    }
    
    // 检查是否有待处理的中断
    if (interrupt_pending_ && get_flag(FLAG_IF)) {
        trigger_interrupt(pending_interrupt_vector_);
        interrupt_pending_ = false;
    }
    
    // 解码并执行指令
    int instruction_length = decode_and_execute();
    
    if (instruction_length > 0) {
        total_instructions_++;
        total_cycles_ += instruction_length;  // 简化的周期计算
        
        // 更新 RIP
        set_rip(get_rip() + instruction_length);
        
        // 调试回调
        if (on_instruction_executed_) {
            disassemble_current();
        }
    }
    
    return instruction_length > 0;  // 返回是否成功执行
}

void X86CPUVM::run_loop() {
    if (!running_) {
        start();
    }
    
    while (running_ && state_ == X86VMState::RUNNING) {
        int len = execute_instruction();
        
        // 如果遇到 HALT 或需要中断，退出循环
        if (len <= 0 || state_ != X86VMState::RUNNING) {
            break;
        }
    }
}

// ===== 寄存器访问 =====
uint64_t X86CPUVM::get_register(X86Reg reg) const {
    size_t idx = static_cast<size_t>(reg);
    if (idx < NUM_X86_REGS) {
        return registers_[idx];
    }
    return 0;
}

void X86CPUVM::set_register(X86Reg reg, uint64_t value) {
    size_t idx = static_cast<size_t>(reg);
    if (idx < NUM_X86_REGS) {
        registers_[idx] = value;
    }
}

// ===== 内存访问 =====
uint8_t X86CPUVM::read_byte(uint64_t addr) const {
    if (addr >= physical_memory_.size()) {
        std::cerr << "[X86VM-" << vm_id_ << "] Memory read out of bounds: 0x" 
                  << std::hex << addr << std::dec << std::endl;
        return 0;
    }
    return physical_memory_[addr];
}

uint16_t X86CPUVM::read_word(uint64_t addr) const {
    return *reinterpret_cast<const uint16_t*>(&physical_memory_[addr]);
}

uint32_t X86CPUVM::read_dword(uint64_t addr) const {
    return *reinterpret_cast<const uint32_t*>(&physical_memory_[addr]);
}

uint64_t X86CPUVM::read_qword(uint64_t addr) const {
    return *reinterpret_cast<const uint64_t*>(&physical_memory_[addr]);
}

void X86CPUVM::write_byte(uint64_t addr, uint8_t value) {
    if (addr >= physical_memory_.size()) {
        std::cerr << "[X86VM-" << vm_id_ << "] Memory write out of bounds: 0x" 
                  << std::hex << addr << std::dec << std::endl;
        return;
    }
    physical_memory_[addr] = value;
}

void X86CPUVM::write_word(uint64_t addr, uint16_t value) {
    *reinterpret_cast<uint16_t*>(&physical_memory_[addr]) = value;
}

void X86CPUVM::write_dword(uint64_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(&physical_memory_[addr]) = value;
}

void X86CPUVM::write_qword(uint64_t addr, uint64_t value) {
    *reinterpret_cast<uint64_t*>(&physical_memory_[addr]) = value;
}

// ===== 程序加载 =====
void X86CPUVM::load_binary(const std::vector<uint8_t>& binary, uint64_t load_addr) {
    if (load_addr + binary.size() > physical_memory_.size()) {
        std::cerr << "[X86VM-" << vm_id_ << "] Binary too large for memory!" << std::endl;
        return;
    }
    
    std::memcpy(&physical_memory_[load_addr], binary.data(), binary.size());
    
    std::cout << "[X86VM-" << vm_id_ << "] Loaded " << binary.size() 
              << " bytes at 0x" << std::hex << load_addr << std::dec << std::endl;
}

void X86CPUVM::load_elf(const std::vector<uint8_t>& elf_binary) {
    // TODO: 实现 ELF 解析
    // 目前简化为直接加载二进制
    load_binary(elf_binary, config_.entry_point);
}

// ===== 中断处理 =====
void X86CPUVM::handle_interrupt(const InterruptResult& result) {
    // 从中断返回
    if (result.return_value >= 0) {
        // ✅ 新增：设置 syscall 返回值到 RAX
        set_register(X86Reg::RAX, static_cast<uint64_t>(result.return_value));
        
        // 恢复现场（简化版本）
        uint64_t ret_rip = pop();
        uint64_t ret_rflags = pop();
        
        set_rip(ret_rip);
        set_register(X86Reg::RFLAGS, ret_rflags);
        
        state_ = X86VMState::RUNNING;
    }
}

void X86CPUVM::trigger_interrupt(uint8_t vector) {
    // 保存现场
    push(get_rflags());
    push(get_rip());
    
    // 简单的中断向量表基址
    uint64_t idt_base = 0;
    uint64_t handler_addr = read_qword(idt_base + vector * 8);
    
    set_rip(handler_addr);
    
    // 清除 IF 标志（禁止嵌套中断）
    set_flag(FLAG_IF, false);
    
    state_ = X86VMState::WAITING_INTERRUPT;
}

void X86CPUVM::trigger_syscall(uint64_t syscall_num) {
    // 系统调用实现：通过路由树向外设发送消息
    // 1. 保存现场（RAX 已经包含 syscall_num）
    push(get_rcx());  // 保存返回地址
    push(get_r11());  // 保存 RFLAGS
    
    // 2. 设置返回地址为下一条指令
    uint64_t ret_rip = get_rip();
    
    // 3. 构造系统调用消息
    Message msg;
    msg.sender_id = vm_id_;
    msg.receiver_id = MODULE_ROUTER_CORE;  // 发送给路由核心
    msg.type = MessageType::SYSCALL;
    
    SyscallRequest req;
    req.vm_id = vm_id_;
    req.syscall_number = syscall_num;
    
    // 新增：从 x86-64 系统调用约定寄存器获取参数
    // 参数传递顺序：RDI, RSI, RDX, R10, R8, R9
    req.arg1 = get_register(X86Reg::RDI);  // 参数 1
    req.arg2 = get_register(X86Reg::RSI);  // 参数 2
    req.arg3 = get_register(X86Reg::RDX);  // 参数 3
    req.arg4 = get_register(X86Reg::R10);  // 参数 4
    
    msg.set_payload(req);
    
    // 4. 打印日志（后续会通过 VmManager 发送到路由树）
    std::cout << "[X86VM-" << vm_id_ << "] SYSCALL #" << syscall_num 
              << "(arg1=" << req.arg1 << ", arg2=" << req.arg2 
              << ", arg3=" << req.arg3 << ", arg4=" << req.arg4 
              << ") - Sending to router tree" << std::endl;
    
    // 5. 更新状态
    state_ = X86VMState::WAITING_INTERRUPT;
    
    // 6. 跳转到系统调用处理程序（从 IDT 获取）
    uint64_t idt_base = 0;
    uint64_t syscall_handler = read_qword(idt_base + 0x80 * 8);  // 使用 0x80 号中断向量
    if (syscall_handler != 0) {
        set_rip(syscall_handler);
    } else {
        // 如果没有设置处理程序，直接返回到用户空间
        std::cout << "[X86VM-" << vm_id_ << "] No syscall handler, returning" << std::endl;
        set_rip(ret_rip);
    }
    
    // 7. 清除 IF 标志
    set_flag(FLAG_IF, false);
}

// ===== 标志位操作 =====
void X86CPUVM::update_flags_arithmetic(uint64_t result, uint64_t op1, uint64_t op2, bool is_signed) {
    // ZF: 零标志
    set_flag(FLAG_ZF, result == 0);
    
    // SF: 符号标志
    set_flag(FLAG_SF, (result >> 63) & 1);
    
    // CF: 进位标志 (无符号溢出)
    if (is_signed) {
        // 有符号运算
        bool overflow = ((op1 ^ result) & ~(op1 ^ op2)) >> 63;
        set_flag(FLAG_OF, overflow);
    } else {
        // 无符号运算
        bool carry = (op1 + op2 < op1);
        set_flag(FLAG_CF, carry);
        set_flag(FLAG_OF, false);
    }
    
    // PF: 奇偶标志
    uint8_t low_byte = result & 0xFF;
    int parity = 0;
    while (low_byte) {
        parity ^= low_byte & 1;
        low_byte >>= 1;
    }
    set_flag(FLAG_PF, parity == 0);
}

void X86CPUVM::set_flag(uint64_t flag, bool value) {
    uint64_t rflags = get_rflags();
    if (value) {
        rflags |= flag;
    } else {
        rflags &= ~flag;
    }
    set_register(X86Reg::RFLAGS, rflags);
}

bool X86CPUVM::get_flag(uint64_t flag) const {
    return (get_rflags() & flag) != 0;
}

// ===== 堆栈操作 =====
void X86CPUVM::push(uint64_t value) {
    set_rsp(get_rsp() - 8);
    write_qword(get_rsp(), value);
}

uint64_t X86CPUVM::pop() {
    uint64_t value = read_qword(get_rsp());
    set_rsp(get_rsp() + 8);
    return value;
}

// ===== 调试接口 =====
void X86CPUVM::dump_registers() const {
    std::cout << "\n===== X86VM-" << vm_id_ << " Register State =====" << std::endl;
    std::cout << std::hex << std::showbase;
    
    std::cout << "RAX: " << std::setw(16) << get_register(X86Reg::RAX) << "  ";
    std::cout << "RBX: " << std::setw(16) << get_register(X86Reg::RBX) << "  ";
    std::cout << "RCX: " << std::setw(16) << get_register(X86Reg::RCX) << "  ";
    std::cout << "RDX: " << std::setw(16) << get_register(X86Reg::RDX) << std::endl;
    
    std::cout << "RSI: " << std::setw(16) << get_register(X86Reg::RSI) << "  ";
    std::cout << "RDI: " << std::setw(16) << get_register(X86Reg::RDI) << "  ";
    std::cout << "RBP: " << std::setw(16) << get_register(X86Reg::RBP) << "  ";
    std::cout << "RSP: " << std::setw(16) << get_register(X86Reg::RSP) << std::endl;
    
    std::cout << "R8:  " << std::setw(16) << get_register(X86Reg::R8) << "  ";
    std::cout << "R9:  " << std::setw(16) << get_register(X86Reg::R9) << "  ";
    std::cout << "R10: " << std::setw(16) << get_register(X86Reg::R10) << "  ";
    std::cout << "R11: " << std::setw(16) << get_register(X86Reg::R11) << std::endl;
    
    std::cout << "R12: " << std::setw(16) << get_register(X86Reg::R12) << "  ";
    std::cout << "R13: " << std::setw(16) << get_register(X86Reg::R13) << "  ";
    std::cout << "R14: " << std::setw(16) << get_register(X86Reg::R14) << "  ";
    std::cout << "R15: " << std::setw(16) << get_register(X86Reg::R15) << std::endl;
    
    std::cout << "RIP: " << std::setw(16) << get_rip() << std::endl;
    std::cout << "RFLAGS: " << std::setw(16) << get_rflags() << std::endl;
    
    std::cout << std::dec << std::noshowbase;
    std::cout << "=====================================\n" << std::endl;
}

void X86CPUVM::dump_memory(uint64_t addr, size_t len) const {
    std::cout << "\nMemory at 0x" << std::hex << addr << ":" << std::endl;
    
    for (size_t i = 0; i < len; i += 16) {
        std::cout << std::setw(16) << (addr + i) << ": ";
        
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            std::cout << std::setw(2) << static_cast<int>(read_byte(addr + i + j)) << " ";
        }
        
        std::cout << std::endl;
    }
    
    std::cout << std::dec << std::endl;
}

void X86CPUVM::disassemble_current() const {
    // TODO: 实现完整的反汇编
    // 目前只打印当前地址
    std::cout << "[0x" << std::hex << get_rip() << std::dec << "] Executing..." << std::endl;
}
