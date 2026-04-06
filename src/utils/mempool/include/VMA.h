/*
虚拟内存区域（Virtual Memory Area）
模拟 Linux 内核的 vm_area_struct
*/
#ifndef _VMA_H
#define _VMA_H

#include "List.h"
#include "RBtree.h"
#include <cstdint>
#include <cstddef>

// VMA 标志位
enum VMAFlags {
    VMA_READ      = 1 << 0,   // 可读
    VMA_WRITE     = 1 << 1,   // 可写
    VMA_EXEC      = 1 << 2,   // 可执行
    VMA_SHARED    = 1 << 3,   // 共享映射
    VMA_PRIVATE   = 1 << 4,   // 私有映射
    VMA_ANONYMOUS = 1 << 5,   // 匿名映射
    VMA_FILE_BACKED = 1 << 6, // 文件映射
};

// 虚拟内存区域结构
struct VMA {
    // ========== 红黑树节点（用于快速查找）==========
    RBNode rb_node;
    
    // ========== 链表节点（用于顺序遍历）==========
    ListHead list;
    
    // ========== 虚拟地址范围（查找的 key）==========
    uint64_t vm_start;  // 起始虚拟地址
    uint64_t vm_end;    // 结束虚拟地址（不包含）
    
    // ========== 物理地址信息 ==========
    uint64_t vm_pfn;    // 物理页帧号（Page Frame Number）
                        // 对于匿名映射：实际分配的物理页
                        // 对于文件映射：文件内容的缓存页
    
    // ========== 权限和标志 ==========
    uint32_t vm_flags;  // VMAFlags 组合
    
    // ========== 其他信息 ==========
    const char* name;   // 区域名称（如 "[heap]", "[stack]"）
    
    VMA() : vm_start(0), vm_end(0), vm_pfn(0), vm_flags(0), name(nullptr) {
        rb_node = RBNode();
        list.init();
    }
    
    // 判断地址是否在 VMA 范围内
    bool contains(uint64_t addr) const {
        return addr >= vm_start && addr < vm_end;
    }
    
    // 判断是否可以合并到另一个 VMA
    bool can_merge_with(const VMA* other) const {
        // 地址连续且属性相同
        if (vm_end != other->vm_start) return false;
        if (vm_flags != other->vm_flags) return false;
        if (vm_pfn + ((vm_end - vm_start) >> 12) != other->vm_pfn) return false;
        return true;
    }
    
    // 计算虚拟地址对应的物理地址
    uint64_t virt_to_phys(uint64_t vaddr) const {
        if (!contains(vaddr)) return 0;
        uint64_t offset = vaddr - vm_start;
        return (vm_pfn << 12) + offset;  // 假设页大小 4KB
    }
};

// VMA 比较函数对象（用于红黑树查找）
struct VMACmpByAddr {
    uint64_t target_addr;
    
    VMACmpByAddr(uint64_t addr) : target_addr(addr) {}
    
    // 返回值：
    //   < 0 : target < node
    //   = 0 : target == node
    //   > 0 : target > node
    int operator()(const VMA* vma) const {
        if (target_addr < vma->vm_start) return -1;
        if (target_addr >= vma->vm_end) return 1;
        return 0;
    }
};

#endif
