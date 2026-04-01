#include "Loader.h"

// ==================== 公开接口 ====================

Loader::BinaryFormat Loader::load_from_file(const std::string& filename, const LoadOptions& options) {
    BinaryFormat result;
    result.source_file = filename;

    if (options.verbose) {
        std::cout << "[Loader] Loading file: " << filename << std::endl;
    }

    // 读取文件
    std::vector<uint8_t> buffer = read_file_buffer(filename);
    
    if (buffer.empty()) {
        throw std::runtime_error("[Loader] File is empty or cannot be read: " + filename);
    }

    // 应用大小限制
    if (options.max_size > 0 && buffer.size() > options.max_size) {
        throw std::runtime_error("[Loader] File too large: " + std::to_string(buffer.size()) + 
                                " bytes (max: " + std::to_string(options.max_size) + ")");
    }

    result.data = std::move(buffer);
    result.file_size = result.data.size();
    result.load_address = options.base_address != 0 ? options.base_address : 0;
    result.entry_point = result.load_address;  // 默认入口点为加载地址

    if (options.verbose) {
        print_info(result);
    }

    // 验证
    if (options.verify && !verify(result)) {
        std::cerr << "[Loader] Warning: Binary verification failed!" << std::endl;
    }

    return result;
}

Loader::BinaryFormat Loader::load_from_memory(const uint8_t* data, size_t size, 
                                               uint64_t load_addr, uint64_t entry) {
    if (!data || size == 0) {
        throw std::invalid_argument("[Loader] Invalid data pointer or size");
    }

    BinaryFormat result;
    result.data.assign(data, data + size);
    result.file_size = size;
    result.load_address = load_addr;
    result.entry_point = entry;
    result.source_file = "<memory>";

    return result;
}

void Loader::print_info(const BinaryFormat& binary) {
    std::cout << "========================================" << std::endl;
    std::cout << "       Binary Loading Information      " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Source File:    " << binary.source_file << std::endl;
    std::cout << "File Size:      " << binary.file_size << " bytes" << std::endl;
    std::cout << "Load Address:   0x" << std::hex << binary.load_address << std::dec << std::endl;
    std::cout << "Entry Point:    0x" << std::hex << binary.entry_point << std::dec << std::endl;
    std::cout << "Data Range:     [0x" << std::hex << binary.load_address 
              << " - 0x" << (binary.load_address + binary.file_size - 1) << std::dec << "]" << std::endl;
    
    // 显示前 16 字节（如果有）
    if (binary.file_size > 0) {
        std::cout << "First 16 bytes:";
        for (size_t i = 0; i < std::min(size_t(16), binary.file_size); ++i) {
            if (i % 8 == 0) std::cout << std::endl << "  ";
            printf("%02X ", binary.data[i]);
        }
        std::cout << std::endl;
    }
    
    std::cout << "========================================" << std::endl;
}

bool Loader::verify(const BinaryFormat& binary) {
    if (binary.data.empty()) {
        std::cerr << "[Loader] Verification failed: Empty binary data" << std::endl;
        return false;
    }

    // 简单验证：检查是否全为 0
    bool all_zero = true;
    for (uint8_t byte : binary.data) {
        if (byte != 0) {
            all_zero = false;
            break;
        }
    }

    if (all_zero) {
        std::cerr << "[Loader] Warning: Binary data is all zeros" << std::endl;
        return false;
    }

    // 计算校验和
    uint32_t checksum = calculate_checksum(binary.data);
    
    if (checksum == 0) {
        std::cerr << "[Loader] Warning: Checksum is zero (may indicate invalid data)" << std::endl;
        return false;
    }

    return true;
}

// ==================== 私有辅助函数 ====================

std::vector<uint8_t> Loader::read_file_buffer(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        throw std::runtime_error("[Loader] Cannot open file: " + filename);
    }

    // 获取文件大小
    std::streamsize size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("[Loader] Cannot determine file size: " + filename);
    }

    // 重置文件指针到开头
    file.seekg(0, std::ios::beg);

    // 读取整个文件
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("[Loader] Failed to read file: " + filename);
    }

    return buffer;
}

uint32_t Loader::calculate_checksum(const std::vector<uint8_t>& data) {
    uint32_t sum = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        // 简单的累加和，带位置权重
        sum += static_cast<uint32_t>(data[i]) * (static_cast<uint32_t>(i) + 1);
    }
    return sum;
}
