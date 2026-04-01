/*
内存管理器
使用 list_head + 红黑树高效管理虚拟地址映射
*/
#ifndef _MEMORY_MANAGER_H
#define _MEMORY_MANAGER_H

#include "VMA.h"
#include <iostream>
#include <functional>

// 内存管理器 - 模拟 Linux 的 mm_struct
class MemoryManager {
private:
    RBRoot vma_root;      // 红黑树根 - 用于 O(log n) 查找
    ListHead vma_list;    // 链表头 - 用于 O(n) 遍历
    
    size_t total_vmas;    // VMA 总数
    
public:
    MemoryManager() {
        RBtree::init(&vma_root);
        ListHead::init(&vma_list);
        total_vmas = 0;
    }
    
    // ========== 核心操作 ==========
    
    // 添加 VMA（同时插入红黑树和链表）
    bool add_vma(VMA* vma) {
        if (!vma || vma->vm_start >= vma->vm_end) {
            return false;
        }
        
        // 检查是否与现有 VMA 重叠
        VMA* existing = find_vma(vma->vm_start);
        if (existing && existing->contains(vma->vm_start)) {
            std::cerr << "Error: Address range overlaps with existing VMA" << std::endl;
            return false;
        }
        
        // 1. 插入红黑树（按 vm_start 排序）
        insert_vma_rb(vma);
        
        // 2. 插入链表（保持地址顺序）
        insert_vma_list_ordered(vma);
        
        total_vmas++;
        
        // 3. 尝试合并相邻 VMA
        try_merge_adjacent(vma);
        
        return true;
    }
    
    // 删除 VMA
    void remove_vma(VMA* vma) {
        if (!vma) return;
        
        // 从红黑树删除
        RBtree::erase(&vma_root, &vma->rb_node);
        
        // 从链表删除
        list_del(&vma->list);
        
        total_vmas--;
    }
    
    // ========== 查找操作 ==========
    
    // 查找包含指定地址的 VMA - O(log n)
    VMA* find_vma(uint64_t addr) {
        RBNode* node = vma_root.rb_node;
        
        while (node) {
            VMA* vma = container_of_vma(node);
            
            if (addr < vma->vm_start) {
                node = node->left;
            } else if (addr >= vma->vm_end) {
                node = node->right;
            } else {
                return vma;  // 找到：vm_start <= addr < vm_end
            }
        }
        
        return nullptr;
    }
    
    // 查找第一个起始地址 >= addr 的 VMA
    VMA* find_vma_after(uint64_t addr) {
        RBNode* node = vma_root.rb_node;
        VMA* result = nullptr;
        
        while (node) {
            VMA* vma = container_of_vma(node);
            
            if (vma->vm_start >= addr) {
                result = vma;
                node = node->left;
            } else {
                node = node->right;
            }
        }
        
        return result;
    }
    
    // ========== 遍历操作 ==========
    
    // 遍历所有 VMA（通过链表 - O(n)）
    void for_each_vma(std::function<void(VMA*)> callback) {
        ListHead* pos;
        LIST_FOREACH(pos, &vma_list) {
            VMA* vma = LIST_ENTRY(pos, VMA, list);
            callback(vma);
        }
    }
    
    // 打印所有 VMA 信息
    void print_vmas() const {
        std::cout << "\n===== Virtual Memory Areas (" << total_vmas << " VMA(s)) =====" << std::endl;
        std::cout << "Address Range           Flags      PFN       Name" << std::endl;
        std::cout << "-----------------------------------------------------------" << std::endl;
        
        ListHead* pos;
        LIST_FOREACH(pos, &vma_list) {
            VMA* vma = LIST_ENTRY(pos, VMA, list);
            
            std::cout << "0x" << std::hex << vma->vm_start << "-0x" << vma->vm_end << std::dec
                      << "  ";
            
            // 打印标志
            if (vma->vm_flags & VMA_READ) std::cout << "r";
            else std::cout << "-";
            if (vma->vm_flags & VMA_WRITE) std::cout << "w";
            else std::cout << "-";
            if (vma->vm_flags & VMA_EXEC) std::cout << "x";
            else std::cout << "-";
            if (vma->vm_flags & VMA_SHARED) std::cout << "s";
            else std::cout << "p";
            
            std::cout << "        ";
            std::cout << "0x" << std::hex << vma->vm_pfn << std::dec << "         ";
            
            if (vma->name) std::cout << vma->name;
            
            std::cout << std::endl;
        }
        std::cout << "===============================================\n" << std::endl;
    }
    
    // ========== 地址转换 ==========
    
    // 虚拟地址转物理地址
    uint64_t virt_to_phys(uint64_t vaddr) {
        VMA* vma = find_vma(vaddr);
        if (!vma) {
            std::cerr << "Error: No mapping for virtual address 0x" 
                      << std::hex << vaddr << std::dec << std::endl;
            return 0;
        }
        return vma->virt_to_phys(vaddr);
    }
    
    // ========== 统计信息 ==========
    
    size_t get_vma_count() const { return total_vmas; }
    
    bool is_empty() const { return list_empty(&vma_list); }
    
private:
    // 辅助函数：从 RBNode 获取 VMA 指针
    static VMA* container_of_vma(RBNode* node) {
        return RB_ENTRY(node, VMA, rb_node);
    }
    
    // 插入到红黑树（根据 vm_start 排序）
    void insert_vma_rb(VMA* vma) {
        // 定义比较器：根据 vm_start 比较
        auto cmp = [](RBNode* a, RBNode* b) -> int {
            VMA* vma_a = container_of_vma(a);
            VMA* vma_b = container_of_vma(b);
            
            if (vma_a->vm_start < vma_b->vm_start) return -1;
            if (vma_a->vm_start > vma_b->vm_start) return 1;
            return 0;
        };
        
        // 使用带比较器的插入方法
        RBtree::insert_with_compare(&vma_root, &vma->rb_node, cmp);
    }
    
    // 插入到链表（保持地址顺序）
    void insert_vma_list_ordered(VMA* vma) {
        ListHead* pos;
        
        // 找到第一个 vm_start 大于当前 vma 的位置
        LIST_FOREACH(pos, &vma_list) {
            VMA* existing = LIST_ENTRY(pos, VMA, list);
            if (existing->vm_start > vma->vm_start) {
                list_add(&vma->list, existing->list.prev);
                return;
            }
        }
        
        // 如果没找到，添加到链表尾部
        list_add_tail(&vma->list, &vma_list);
    }
    
    // 尝试与相邻 VMA 合并
    void try_merge_adjacent(VMA* vma) {
        ListHead* prev_node = vma->list.prev;
        ListHead* next_node = vma->list.next;
        
        // 尝试与前一个 VMA 合并
        if (prev_node != &vma_list) {
            VMA* prev = LIST_ENTRY(prev_node, VMA, list);
            if (prev->can_merge_with(vma)) {
                // 合并到前一个 VMA
                prev->vm_end = vma->vm_end;
                remove_vma(vma);
                return;
            }
        }
        
        // 尝试与后一个 VMA 合并
        if (next_node != &vma_list) {
            VMA* next = LIST_ENTRY(next_node, VMA, list);
            if (vma->can_merge_with(next)) {
                // 合并到当前 VMA
                vma->vm_end = next->vm_end;
                remove_vma(next);
                return;
            }
        }
    }
};

#endif
