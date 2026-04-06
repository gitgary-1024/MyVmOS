#ifndef LIVS_UTILS_LOADER_H
#define LIVS_UTILS_LOADER_H

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>

/**
 * @brief 二进制文件加载器
 * 
 * 功能：
 * 1. 从 .bin 文件加载原始二进制数据
 * 2. 支持指定加载地址
 * 3. 支持设置入口点
 * 4. 提供加载验证和调试信息
 * 
 * 使用场景：
 * - 加载编译器生成的二进制文件
 * - 加载 Bootloader
 * - 加载内核镜像
 * - 加载用户程序
 */
class Loader {
public:
    /**
     * @brief 加载的二进制文件格式
     */
    struct BinaryFormat {
        std::vector<uint8_t> data;      // 二进制数据
        uint64_t load_address = 0;      // 加载地址
        uint64_t entry_point = 0;       // 入口点
        size_t file_size = 0;           // 文件大小
        std::string source_file;        // 源文件名
    };

    /**
     * @brief 加载选项
     */
    struct LoadOptions {
        uint64_t base_address = 0;          // 基础地址（覆盖文件中的加载地址）
        bool verbose = false;               // 详细输出
        bool verify = true;                 // 验证加载
        size_t max_size = 0;                // 最大加载大小（0=不限制）
    };

    /**
     * @brief 从文件加载二进制
     * @param filename .bin 文件路径
     * @param options 加载选项
     * @return 加载的二进制格式
     */
    static BinaryFormat load_from_file(const std::string& filename, const LoadOptions& options);

    /**
     * @brief 从文件加载二进制（简化版本，使用默认选项）
     * @param filename .bin 文件路径
     * @return 加载的二进制格式
     */
    static BinaryFormat load_from_file(const std::string& filename) {
        return load_from_file(filename, LoadOptions{});
    }

    /**
     * @brief 从内存加载二进制数据
     * @param data 二进制数据指针
     * @param size 数据大小
     * @param load_addr 加载地址
     * @param entry 入口点
     * @return 加载的二进制格式
     */
    static BinaryFormat load_from_memory(const uint8_t* data, size_t size, 
                                         uint64_t load_addr = 0, uint64_t entry = 0);

    /**
     * @brief 将二进制数据填充到目标内存
     * @param binary 已加载的二进制
     * @param dest_memory 目标内存（引用）
     * @param memory_size 目标内存大小
     * @return 是否成功
     */
    template<typename MemoryType>
    static bool write_to_memory(const BinaryFormat& binary, MemoryType& dest_memory, size_t memory_size) {
        if (binary.load_address + binary.data.size() > memory_size) {
            std::cerr << "[Loader] Error: Binary too large for destination memory!" << std::endl;
            std::cerr << "  Binary size: " << binary.data.size() << " bytes" << std::endl;
            std::cerr << "  Load address: 0x" << std::hex << binary.load_address << std::dec << std::endl;
            std::cerr << "  Memory size: " << memory_size << " bytes" << std::endl;
            return false;
        }

        // 确保目标内存足够大
        if (dest_memory.size() < memory_size) {
            dest_memory.resize(memory_size);
        }

        // 复制数据
        for (size_t i = 0; i < binary.data.size(); ++i) {
            dest_memory[binary.load_address + i] = binary.data[i];
        }

        return true;
    }

    /**
     * @brief 打印加载信息
     * @param binary 已加载的二进制
     */
    static void print_info(const BinaryFormat& binary);

    /**
     * @brief 验证二进制数据（检查是否全为 0 或有明显错误）
     * @param binary 已加载的二进制
     * @return 是否有效
     */
    static bool verify(const BinaryFormat& binary);

private:
    /**
     * @brief 读取整个文件到缓冲区
     */
    static std::vector<uint8_t> read_file_buffer(const std::string& filename);

    /**
     * @brief 计算校验和（简单验证）
     */
    static uint32_t calculate_checksum(const std::vector<uint8_t>& data);
};

#endif // LIVS_UTILS_LOADER_H
