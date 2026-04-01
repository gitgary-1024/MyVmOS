/*
单元测试：验证 list_head + 红黑树内存管理系统的正确性
*/
#include <iostream>
#include <cassert>
#include "../include/DataStructure/MemoryManager.h"

using namespace std;

void test_vma_creation() {
    cout << "测试：VMA 创建... ";
    
    MemoryManager mm;
    assert(mm.is_empty());
    assert(mm.get_vma_count() == 0);
    
    VMA* vma = new VMA();
    vma->vm_start = 0x1000;
    vma->vm_end = 0x2000;
    vma->vm_pfn = 0x100;
    vma->vm_flags = VMA_READ | VMA_WRITE;
    
    assert(mm.add_vma(vma));
    assert(!mm.is_empty());
    assert(mm.get_vma_count() == 1);
    
    cout << "✓ 通过" << endl;
}

void test_vma_lookup() {
    cout << "测试：VMA 查找... ";
    
    MemoryManager mm;
    
    // 创建三个不重叠的 VMA
    VMA* vma1 = new VMA();
    vma1->vm_start = 0x1000;
    vma1->vm_end = 0x2000;
    vma1->vm_pfn = 0x100;
    mm.add_vma(vma1);
    
    VMA* vma2 = new VMA();
    vma2->vm_start = 0x3000;
    vma2->vm_end = 0x4000;
    vma2->vm_pfn = 0x200;
    mm.add_vma(vma2);
    
    VMA* vma3 = new VMA();
    vma3->vm_start = 0x5000;
    vma3->vm_end = 0x6000;
    vma3->vm_pfn = 0x300;
    mm.add_vma(vma3);
    
    // 测试查找
    assert(mm.find_vma(0x1000) == vma1);   // 起始地址
    assert(mm.find_vma(0x1500) == vma1);   // 中间地址
    assert(mm.find_vma(0x1FFF) == vma1);   // 结束前一个地址
    
    assert(mm.find_vma(0x3500) == vma2);
    assert(mm.find_vma(0x5500) == vma3);
    
    // 测试未找到的情况
    assert(mm.find_vma(0x2000) == nullptr);  // 间隙
    assert(mm.find_vma(0x2500) == nullptr);
    assert(mm.find_vma(0x6000) == nullptr);  // 超出范围
    
    cout << "✓ 通过" << endl;
}

void test_address_translation() {
    cout << "测试：地址转换... ";
    
    MemoryManager mm;
    
    VMA* vma = new VMA();
    vma->vm_start = 0x10000;
    vma->vm_end = 0x20000;
    vma->vm_pfn = 0x100;  // 物理页从 0x100 开始
    vma->vm_flags = VMA_READ | VMA_WRITE;
    mm.add_vma(vma);
    
    // 虚拟地址转物理地址
    assert(mm.virt_to_phys(0x10000) == 0x100000);  // 0x100 << 12 = 0x100000
    assert(mm.virt_to_phys(0x10001) == 0x100001);  // 偏移 1 字节
    assert(mm.virt_to_phys(0x11000) == 0x101000);  // 偏移 0x1000
    
    cout << "✓ 通过" << endl;
}

void test_vma_merge() {
    cout << "测试：VMA 自动合并... ";
    
    MemoryManager mm;
    
    // 创建两个相邻且属性相同的 VMA
    VMA* vma1 = new VMA();
    vma1->vm_start = 0x1000;
    vma1->vm_end = 0x2000;
    vma1->vm_pfn = 0x100;  // 物理页从 0x100 开始
    vma1->vm_flags = VMA_READ | VMA_WRITE;
    
    VMA* vma2 = new VMA();
    vma2->vm_start = 0x2000;  // 紧接 vma1
    vma2->vm_end = 0x3000;
    // 物理地址应该连续：vma1 的结束物理地址 = 0x100 + (0x2000-0x1000)/0x1000 = 0x100 + 1 = 0x101
    vma2->vm_pfn = 0x101;     // 下一个物理页
    vma2->vm_flags = VMA_READ | VMA_WRITE;
    
    mm.add_vma(vma1);
    mm.add_vma(vma2);
    
    // 应该自动合并为一个 VMA
    assert(mm.get_vma_count() == 1);
    
    VMA* merged = mm.find_vma(0x1500);
    assert(merged != nullptr);
    assert(merged->vm_start == 0x1000);
    assert(merged->vm_end == 0x3000);
    
    cout << "✓ 通过" << endl;
}

void test_no_merge_different_flags() {
    cout << "测试：不同标志不合并... ";
    
    MemoryManager mm;
    
    VMA* vma1 = new VMA();
    vma1->vm_start = 0x1000;
    vma1->vm_end = 0x2000;
    vma1->vm_pfn = 0x100;
    vma1->vm_flags = VMA_READ;  // 只读
    
    VMA* vma2 = new VMA();
    vma2->vm_start = 0x2000;
    vma2->vm_end = 0x3000;
    vma2->vm_pfn = 0x200;
    vma2->vm_flags = VMA_READ | VMA_WRITE;  // 可写，标志不同
    
    mm.add_vma(vma1);
    mm.add_vma(vma2);
    
    // 标志不同，不应合并
    assert(mm.get_vma_count() == 2);
    
    cout << "✓ 通过" << endl;
}

void test_list_traversal() {
    cout << "测试：链表顺序遍历... ";
    
    MemoryManager mm;
    
    // 乱序添加 VMA
    VMA* vma3 = new VMA();
    vma3->vm_start = 0x5000;
    vma3->vm_end = 0x6000;
    mm.add_vma(vma3);
    
    VMA* vma1 = new VMA();
    vma1->vm_start = 0x1000;
    vma1->vm_end = 0x2000;
    mm.add_vma(vma1);
    
    VMA* vma2 = new VMA();
    vma2->vm_start = 0x3000;
    vma2->vm_end = 0x4000;
    mm.add_vma(vma2);
    
    // 链表应该按地址顺序遍历
    int count = 0;
    uint64_t last_end = 0;
    
    mm.for_each_vma([&count, &last_end](VMA* vma) {
        count++;
        if (count > 1) {
            assert(vma->vm_start >= last_end);  // 确保顺序
        }
        last_end = vma->vm_end;
    });
    
    assert(count == 3);
    
    cout << "✓ 通过" << endl;
}

void test_overlap_rejection() {
    cout << "测试：重叠地址拒绝... ";
    
    MemoryManager mm;
    
    VMA* vma1 = new VMA();
    vma1->vm_start = 0x1000;
    vma1->vm_end = 0x2000;
    mm.add_vma(vma1);
    
    // 尝试添加重叠的 VMA
    VMA* overlap = new VMA();
    overlap->vm_start = 0x1500;  // 在 vma1 范围内
    overlap->vm_end = 0x2500;
    
    assert(!mm.add_vma(overlap));  // 应该失败
    
    cout << "✓ 通过" << endl;
}

void test_large_scale() {
    cout << "测试：大规模 VMA 管理（1000 个）... ";
    
    MemoryManager mm;
    
    // 创建 1000 个 VMA
    for (int i = 0; i < 1000; i++) {
        VMA* vma = new VMA();
        vma->vm_start = 0x100000 + i * 0x10000;
        vma->vm_end = vma->vm_start + 0x8000;
        vma->vm_pfn = 0x1000 + i * 0x100;
        vma->vm_flags = VMA_READ | VMA_WRITE;
        mm.add_vma(vma);
    }
    
    assert(mm.get_vma_count() == 1000);
    
    // 随机查找测试
    for (int i = 0; i < 100; i++) {
        int idx = rand() % 1000;
        uint64_t addr = 0x100000 + idx * 0x10000 + 0x1000;
        VMA* vma = mm.find_vma(addr);
        assert(vma != nullptr);
    }
    
    cout << "✓ 通过" << endl;
}

int main() {
    system("chcp 65001 > nul");
    cout << "\n╔══════════════════════════════════════════╗" << endl;
    cout << "║  单元测试：list_head + 红黑树内存管理   ║" << endl;
    cout << "╚══════════════════════════════════════════╝\n" << endl;
    
    test_vma_creation();
    test_vma_lookup();
    test_address_translation();
    test_vma_merge();
    test_no_merge_different_flags();
    test_list_traversal();
    test_overlap_rejection();
    test_large_scale();
    
    cout << "\n╔══════════════════════════════════════════╗" << endl;
    cout << "║  ✓ 所有测试通过！                        ║" << endl;
    cout << "╚══════════════════════════════════════════╝\n" << endl;
    
    return 0;
}
