#include "../utils/Loader/Loader.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <cassert>

/**
 * @brief 创建测试用的二进制文件
 */
void create_test_binary(const std::string& filename) {
    // 创建一些测试数据（模拟 x86 机器码）
    std::vector<uint8_t> test_data = {
        0xB8, 0x01, 0x00, 0x00, 0x00,  // mov eax, 1
        0xBB, 0x02, 0x00, 0x00, 0x00,  // mov ebx, 2
        0x01, 0xD8,                     // add eax, ebx
        0xF4,                           // hlt
        0x90, 0x90, 0x90, 0x90         // nop x4
    };

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create test file: " + filename);
    }
    
    file.write(reinterpret_cast<const char*>(test_data.data()), test_data.size());
    file.close();
    
    std::cout << "[Test] Created test binary: " << filename 
              << " (" << test_data.size() << " bytes)" << std::endl;
}

/**
 * @brief 测试 1: 基本文件加载
 */
void test_basic_load() {
    std::cout << "\n=== Test 1: Basic File Loading ===" << std::endl;
    
    const std::string test_file = "test_program.bin";
    create_test_binary(test_file);
    
    try {
        Loader::LoadOptions options;
        options.verbose = true;
        options.verify = true;
        
        auto binary = Loader::load_from_file(test_file, options);
        
        assert(binary.file_size > 0);
        assert(!binary.data.empty());
        assert(binary.load_address == 0);
        assert(binary.entry_point == 0);
        
        std::cout << "✓ Basic load test passed" << std::endl;
        
        // 清理
        std::remove(test_file.c_str());
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        throw;
    }
}

/**
 * @brief 测试 2: 自定义加载地址
 */
void test_custom_load_address() {
    std::cout << "\n=== Test 2: Custom Load Address ===" << std::endl;
    
    const std::string test_file = "test_program.bin";
    create_test_binary(test_file);
    
    try {
        Loader::LoadOptions options;
        options.base_address = 0x1000;  // 加载到 4KB 处
        options.verbose = true;
        
        auto binary = Loader::load_from_file(test_file, options);
        
        assert(binary.load_address == 0x1000);
        assert(binary.entry_point == 0x1000);
        
        std::cout << "✓ Custom load address test passed" << std::endl;
        
        std::remove(test_file.c_str());
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        throw;
    }
}

/**
 * @brief 测试 3: 内存加载
 */
void test_memory_load() {
    std::cout << "\n=== Test 3: Memory Loading ===" << std::endl;
    
    uint8_t test_data[] = {0x90, 0x90, 0x90, 0x90};  // 4 个 nop
    
    try {
        auto binary = Loader::load_from_memory(test_data, sizeof(test_data), 0x2000, 0x2000);
        
        assert(binary.file_size == 4);
        assert(binary.load_address == 0x2000);
        assert(binary.entry_point == 0x2000);
        assert(binary.data.size() == 4);
        
        std::cout << "✓ Memory load test passed" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        throw;
    }
}

/**
 * @brief 测试 4: 写入内存（模拟 VM 内存）
 */
void test_write_to_vm_memory() {
    std::cout << "\n=== Test 4: Write to VM Memory ===" << std::endl;
    
    // 创建测试二进制
    const std::string test_file = "test_program.bin";
    create_test_binary(test_file);
    
    try {
        // 加载二进制
        Loader::LoadOptions options;
        options.base_address = 0x100;  // 加载到 256 字节处
        auto binary = Loader::load_from_file(test_file, options);
        
        // 模拟 VM 内存（使用 vector<uint8_t>）
        std::vector<uint8_t> vm_memory(0x10000);  // 64KB 内存
        
        // 写入内存
        bool success = Loader::write_to_memory(binary, vm_memory, vm_memory.size());
        assert(success);
        
        // 验证数据是否正确写入
        for (size_t i = 0; i < binary.data.size(); ++i) {
            assert(vm_memory[binary.load_address + i] == binary.data[i]);
        }
        
        std::cout << "  Verified: Data correctly written to VM memory" << std::endl;
        std::cout << "  Memory at 0x100: ";
        for (size_t i = 0; i < std::min(size_t(8), binary.data.size()); ++i) {
            printf("%02X ", vm_memory[0x100 + i]);
        }
        std::cout << std::endl;
        
        std::cout << "✓ Write to VM memory test passed" << std::endl;
        
        std::remove(test_file.c_str());
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        throw;
    }
}

/**
 * @brief 测试 5: 错误处理
 */
void test_error_handling() {
    std::cout << "\n=== Test 5: Error Handling ===" << std::endl;
    
    // 测试 1: 不存在的文件
    try {
        Loader::load_from_file("nonexistent_file.bin");
        std::cerr << "✗ Should have thrown exception for nonexistent file" << std::endl;
        throw std::runtime_error("Expected exception not thrown");
    } catch (const std::runtime_error& e) {
        std::cout << "  ✓ Correctly handled nonexistent file: " << e.what() << std::endl;
    }
    
    // 测试 2: 空数据指针
    try {
        Loader::load_from_memory(nullptr, 10, 0, 0);
        std::cerr << "✗ Should have thrown exception for null pointer" << std::endl;
        throw std::runtime_error("Expected exception not thrown");
    } catch (const std::invalid_argument& e) {
        std::cout << "  ✓ Correctly handled null pointer: " << e.what() << std::endl;
    }
    
    // 测试 3: 超出内存大小
    try {
        Loader::BinaryFormat binary;
        binary.data = {1, 2, 3, 4};
        binary.load_address = 0x100;
        
        std::vector<uint8_t> small_memory(10);  // 只有 10 字节
        bool success = Loader::write_to_memory(binary, small_memory, small_memory.size());
        
        if (success) {
            std::cerr << "✗ Should have failed for oversized binary" << std::endl;
            throw std::runtime_error("Should have failed");
        }
        
        std::cout << "  ✓ Correctly rejected oversized binary" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "  ✓ Correctly handled oversized binary: " << e.what() << std::endl;
    }
    
    std::cout << "✓ Error handling test passed" << std::endl;
}

/**
 * @brief 测试 6: 验证功能
 */
void test_verification() {
    std::cout << "\n=== Test 6: Binary Verification ===" << std::endl;
    
    // 有效数据
    Loader::BinaryFormat valid_binary;
    valid_binary.data = {0xB8, 0x01, 0x00, 0x00, 0x00};
    assert(Loader::verify(valid_binary));
    std::cout << "  ✓ Valid binary verified successfully" << std::endl;
    
    // 全零数据
    Loader::BinaryFormat zero_binary;
    zero_binary.data = {0x00, 0x00, 0x00, 0x00};
    assert(!Loader::verify(zero_binary));
    std::cout << "  ✓ Zero-filled binary correctly rejected" << std::endl;
    
    // 空数据
    Loader::BinaryFormat empty_binary;
    empty_binary.data = {};
    assert(!Loader::verify(empty_binary));
    std::cout << "  ✓ Empty binary correctly rejected" << std::endl;
    
    std::cout << "✓ Verification test passed" << std::endl;
}

int main() {
    system("chcp 65001 > nul");
    
    std::cout << "========================================" << std::endl;
    std::cout << "      Loader Test Suite                " << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        test_basic_load();
        test_custom_load_address();
        test_memory_load();
        test_write_to_vm_memory();
        test_error_handling();
        test_verification();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "   All tests passed successfully!     " << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "   Test failed: " << e.what() << std::endl;
        std::cerr << "========================================" << std::endl;
        return 1;
    }
}
