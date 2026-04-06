#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <iostream>
#include <iomanip>

// x86-64 寄存器枚举（简化版）
enum class X86Reg : uint8_t {
    RAX = 0, RBX, RCX, RDX,
    RSI, RDI, RBP, RSP,
    R8, R9, R10, R11, R12, R13, R14, R15,
    RIP, RFLAGS,
    MAX_REG
};

// 标志位定义
constexpr uint64_t FLAG_CF = (1ULL << 0);   // Carry Flag
constexpr uint64_t FLAG_ZF = (1ULL << 6);   // Zero Flag
constexpr uint64_t FLAG_SF = (1ULL << 7);   // Sign Flag
constexpr uint64_t FLAG_OF = (1ULL << 11);  // Overflow Flag

/**
 * Mock VM（简化版虚拟机）
 * 
 * 用于 CFG 测试，提供：
 * - 18 个 x86-64 寄存器
 * - 64MB 线性内存
 * - 代码加载功能
 */
class MockVM {
private:
    std::array<uint64_t, static_cast<size_t>(X86Reg::MAX_REG)> registers_;
    std::vector<uint8_t> memory_;
    size_t code_size_;
    
public:
    MockVM() 
        : registers_{}, 
          memory_(64 * 1024 * 1024, 0),  // 64MB 内存
          code_size_(0) {
        // 初始化栈指针
        registers_[static_cast<size_t>(X86Reg::RSP)] = 0x03FF0000;  // 64MB 顶部
        registers_[static_cast<size_t>(X86Reg::RIP)] = 0;
        registers_[static_cast<size_t>(X86Reg::RFLAGS)] = 0;
    }
    
    /**
     * 加载代码到内存
     * @param code 代码指针
     * @param size 代码大小
     * @param load_addr 加载地址（默认为 0）
     */
    void load_code(const uint8_t* code, size_t size, uint64_t load_addr = 0) {
        if (load_addr + size > memory_.size()) {
            throw std::out_of_range("Code size exceeds memory");
        }
        
        for (size_t i = 0; i < size; i++) {
            memory_[load_addr + i] = code[i];
        }
        
        code_size_ = size;
        registers_[static_cast<size_t>(X86Reg::RIP)] = load_addr;
    }
    
    /**
     * 获取代码段指针
     */
    const uint8_t* get_code_ptr() const {
        return memory_.data();
    }
    
    /**
     * 获取代码段大小
     */
    size_t get_code_size() const {
        return code_size_;
    }
    
    /**
     * 获取寄存器值
     */
    uint64_t get_register(X86Reg reg) const {
        return registers_[static_cast<size_t>(reg)];
    }
    
    /**
     * 设置寄存器值
     */
    void set_register(X86Reg reg, uint64_t value) {
        registers_[static_cast<size_t>(reg)] = value;
    }
    
    /**
     * 获取 RFLAGS
     */
    uint64_t get_rflags() const {
        return registers_[static_cast<size_t>(X86Reg::RFLAGS)];
    }
    
    /**
     * 设置 RFLAGS
     */
    void set_rflags(uint64_t flags) {
        registers_[static_cast<size_t>(X86Reg::RFLAGS)] = flags;
    }
    
    /**
     * 读取内存（8字节）
     */
    uint64_t read_qword(uint64_t addr) const {
        if (addr + 7 >= memory_.size()) {
            throw std::out_of_range("Memory access out of bounds");
        }
        uint64_t value = 0;
        for (int i = 0; i < 8; i++) {
            value |= static_cast<uint64_t>(memory_[addr + i]) << (i * 8);
        }
        return value;
    }
    
    /**
     * 写入内存（8字节）
     */
    void write_qword(uint64_t addr, uint64_t value) {
        if (addr + 7 >= memory_.size()) {
            throw std::out_of_range("Memory access out of bounds");
        }
        for (int i = 0; i < 8; i++) {
            memory_[addr + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
        }
    }
    
    /**
     * 打印寄存器状态（调试用）
     */
    void print_registers() const {
        std::cout << "=== Register State ===" << std::endl;
        std::cout << "RAX: 0x" << std::hex << get_register(X86Reg::RAX) << std::dec << std::endl;
        std::cout << "RBX: 0x" << std::hex << get_register(X86Reg::RBX) << std::dec << std::endl;
        std::cout << "RCX: 0x" << std::hex << get_register(X86Reg::RCX) << std::dec << std::endl;
        std::cout << "RDX: 0x" << std::hex << get_register(X86Reg::RDX) << std::dec << std::endl;
        std::cout << "RSP: 0x" << std::hex << get_register(X86Reg::RSP) << std::dec << std::endl;
        std::cout << "RIP: 0x" << std::hex << get_register(X86Reg::RIP) << std::dec << std::endl;
        
        uint64_t flags = get_rflags();
        std::cout << "RFLAGS: 0x" << std::hex << flags << std::dec << std::endl;
        std::cout << "  CF=" << ((flags & FLAG_CF) ? 1 : 0);
        std::cout << " ZF=" << ((flags & FLAG_ZF) ? 1 : 0);
        std::cout << " SF=" << ((flags & FLAG_SF) ? 1 : 0);
        std::cout << " OF=" << ((flags & FLAG_OF) ? 1 : 0) << std::endl;
        std::cout << "=====================" << std::endl;
    }
    
    /**
     * 打印内存内容（调试用）
     * @param addr 起始地址
     * @param length 长度
     */
    void print_memory(uint64_t addr, size_t length) const {
        std::cout << "=== Memory [0x" << std::hex << addr << " - 0x" 
                  << (addr + length - 1) << "] ===" << std::dec << std::endl;
        
        for (size_t i = 0; i < length; i += 16) {
            printf("0x%08llx: ", addr + i);
            
            // 打印十六进制
            for (size_t j = 0; j < 16 && (i + j) < length; j++) {
                printf("%02x ", memory_[addr + i + j]);
            }
            
            // 补齐空格
            for (size_t j = length - i; j < 16; j++) {
                printf("   ");
            }
            
            printf(" |");
            
            // 打印 ASCII
            for (size_t j = 0; j < 16 && (i + j) < length; j++) {
                char c = memory_[addr + i + j];
                printf("%c", (c >= 32 && c < 127) ? c : '.');
            }
            
            printf("|\n");
        }
        
        std::cout << "=====================" << std::dec << std::endl;
    }
};
