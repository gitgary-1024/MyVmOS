#ifndef LIVS_VM_X86_CPU_H
#define LIVS_VM_X86_CPU_H

#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include "../router/MessageProtocol.h"
#include "baseVM.h"  // 新增：继承 baseVM

// ===== x86 寄存器定义 =====
enum class X86Reg {
    RAX = 0, RBX = 1, RCX = 2, RDX = 3,
    RSI = 4, RDI = 5, RBP = 6, RSP = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    RIP = 16, RFLAGS = 17
};

constexpr size_t NUM_X86_REGS = 18;

// ===== RFLAGS 标志位 =====
constexpr uint64_t FLAG_CF = (1ULL << 0);   // 进位标志
constexpr uint64_t FLAG_PF = (1ULL << 2);   // 奇偶标志
constexpr uint64_t FLAG_AF = (1ULL << 4);   // 辅助进位标志
constexpr uint64_t FLAG_ZF = (1ULL << 6);   // 零标志
constexpr uint64_t FLAG_SF = (1ULL << 7);   // 符号标志
constexpr uint64_t FLAG_TF = (1ULL << 8);   // 陷阱标志
constexpr uint64_t FLAG_IF = (1ULL << 9);   // 中断使能标志
constexpr uint64_t FLAG_DF = (1ULL << 10);  // 方向标志
constexpr uint64_t FLAG_OF = (1ULL << 11);  // 溢出标志

// ===== VM 状态 =====
enum class X86VMState {
    CREATED,
    RUNNING,
    HALTED,
    WAITING_INTERRUPT,
    TERMINATED
};

// ===== x86 VM 配置 =====
struct X86VMConfig {
    size_t memory_size = 64 * 1024 * 1024;  // 64MB 内存
    uint64_t entry_point = 0x100000;         // 入口地址 (1MB 处，典型内核加载地址)
    uint64_t stack_pointer = 0x800000;       // 栈顶
    bool enable_paging = false;              // 是否启用分页
    bool enable_long_mode = true;            // 64 位模式
};

// ===== x86 CPU VM 类 =====
class X86CPUVM : public baseVM {
public:
    explicit X86CPUVM(uint64_t vm_id, const X86VMConfig& config = X86VMConfig());
    ~X86CPUVM();

    // ===== 核心控制接口 =====
    void start() override;
    void stop() override;
    void reset();  // X86 特有方法
    VMState get_state() const override { 
        return state_ == X86VMState::RUNNING ? VMState::RUNNING : VMState::CREATED; 
    }
    
    // ===== 执行接口 =====
    bool execute_instruction() override;  // 实现 baseVM 接口
    int execute_instruction_x86();        // x86 特定版本，返回执行的字节数
    void run_loop();                      // 连续执行（X86 特有方法）
    
    // ===== 状态查询 =====
    // get_state() 已经在上面定义为返回 VMState
    X86VMState get_x86_state() const { return state_; }  // X86 特定状态
    uint64_t get_vm_id() const { return vm_id_; }
    
    // ===== 寄存器访问 =====
    uint64_t get_register(X86Reg reg) const;
    void set_register(X86Reg reg, uint64_t value);
    
    // 便捷访问
    uint64_t get_rip() const { return registers_[static_cast<size_t>(X86Reg::RIP)]; }
    uint64_t get_rsp() const { return registers_[static_cast<size_t>(X86Reg::RSP)]; }
    uint64_t get_rbp() const { return registers_[static_cast<size_t>(X86Reg::RBP)]; }
    uint64_t get_rcx() const { return registers_[static_cast<size_t>(X86Reg::RCX)]; }
    uint64_t get_r11() const { return registers_[static_cast<size_t>(X86Reg::R11)]; }
    uint64_t get_rflags() const { return registers_[static_cast<size_t>(X86Reg::RFLAGS)]; }
    
    void set_rip(uint64_t value) { registers_[static_cast<size_t>(X86Reg::RIP)] = value; }
    void set_rsp(uint64_t value) { registers_[static_cast<size_t>(X86Reg::RSP)] = value; }
    void set_rbp(uint64_t value) { registers_[static_cast<size_t>(X86Reg::RBP)] = value; }
    
    // ===== 内存访问 =====
    uint8_t read_byte(uint64_t addr) const;
    uint16_t read_word(uint64_t addr) const;
    uint32_t read_dword(uint64_t addr) const;
    uint64_t read_qword(uint64_t addr) const;
    
    void write_byte(uint64_t addr, uint8_t value);
    void write_word(uint64_t addr, uint16_t value);
    void write_dword(uint64_t addr, uint32_t value);
    void write_qword(uint64_t addr, uint64_t value);
    
    // ===== 程序加载 =====
    void load_binary(const std::vector<uint8_t>& binary, uint64_t load_addr = 0);
    void load_elf(const std::vector<uint8_t>& elf_binary);  // ELF 格式支持
    
    // baseVM 接口实现
    void load_program(const std::vector<uint32_t>& code) override {
        // 将 32 位代码转换为字节流加载
        std::vector<uint8_t> binary(code.size() * 4);
        for (size_t i = 0; i < code.size(); ++i) {
            binary[i * 4 + 0] = code[i] & 0xFF;
            binary[i * 4 + 1] = (code[i] >> 8) & 0xFF;
            binary[i * 4 + 2] = (code[i] >> 16) & 0xFF;
            binary[i * 4 + 3] = (code[i] >> 24) & 0xFF;
        }
        load_binary(binary, config_.entry_point);
    }
    
    // ===== 中断处理 =====
    void handle_interrupt(const InterruptResult& result) override;  // 实现 baseVM 接口
    void trigger_interrupt(uint8_t vector);  // 触发硬件中断
    void trigger_syscall(uint64_t syscall_num);  // 触发系统调用
    
    // ===== 调试接口 =====
    void dump_registers() const;
    void dump_memory(uint64_t addr, size_t len) const;
    void disassemble_current() const;  // 反汇编当前指令
    
    // ===== CFG 分析接口 =====
    void build_cfg(uint64_t entry_addr);  // 构建控制流图
    bool has_cfg() const { return cfg_ != nullptr; }
    const void* get_cfg() const { return cfg_; }  // 返回 CFG 指针（用于外部查询）
    
private:
    // ===== x86 指令解码与执行 =====
    int decode_and_execute();
    
    // 指令执行函数（按功能分类）
    int execute_mov(uint64_t rip);
    int execute_push_pop(uint64_t rip);
    int execute_arithmetic(uint64_t rip);
    int execute_logical(uint64_t rip);
    int execute_branch(uint64_t rip);
    int execute_call_ret(uint64_t rip);
    int execute_stack_ops(uint64_t rip);
    int execute_flag_ops(uint64_t rip);
    int execute_interrupt(uint64_t rip);  // INT n 指令
    int execute_string_ops(uint64_t rip);
    int execute_data_transfer(uint64_t rip);
    
    // ===== ModR/M 解码 =====
    struct ModRMDecoding {
        uint8_t mod;
        uint8_t reg;
        uint8_t rm;
        bool has_sib;
        bool has_displacement;
        int displacement_size;  // 0, 8, 32
        bool is_memory_operand;
        uint64_t memory_address;  // 如果是内存操作数，计算后的地址
    };
    
    ModRMDecoding decode_modrm(uint64_t rip, int& instruction_length);
    uint64_t calculate_effective_address(const ModRMDecoding& decoding, uint64_t rip);
    
    // ===== 标志位更新 =====
    void update_flags_arithmetic(uint64_t result, uint64_t op1, uint64_t op2, bool is_signed);
    void set_flag(uint64_t flag, bool value);
    bool get_flag(uint64_t flag) const;
    
    // ===== 堆栈操作 =====
    void push(uint64_t value);
    uint64_t pop();
    
    // ===== 工具函数 =====
    template<typename T>
    T read_at_rip(int offset) const;
    
    template<typename T>
    void write_at_rip(int offset, T value);

private:
    // vm_id_ 继承自 baseVM，不需要重复定义
    X86VMConfig config_;
    
    // 寄存器文件
    std::array<uint64_t, NUM_X86_REGS> registers_;
    
    // 物理内存
    std::vector<uint8_t> physical_memory_;
    
    // VM 状态
    X86VMState state_;
    bool running_;
    
    // 中断相关
    uint8_t pending_interrupt_vector_;
    bool interrupt_pending_;
    
    // 统计信息
    uint64_t total_instructions_;
    uint64_t total_cycles_;
    
    // 调试回调
    std::function<void(uint64_t rip, const std::string& instruction)> on_instruction_executed_;
    
    // CFG 分析（可选）
    void* cfg_;  // ControlFlowGraph*，使用 void* 避免头文件依赖
};

#endif // LIVS_VM_X86_CPU_H
