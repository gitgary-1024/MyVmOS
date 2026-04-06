/*
Linux 内存管理演示：list_head + 红黑树
高效查找 & 修改虚拟地址 & 实际地址映射
*/
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include "MemoryManager.h"

using namespace std;

void demo_basic_usage() {
    cout << "\n========== 基础使用示例 ==========" << endl;
    
    MemoryManager mm;
    
    // 创建 VMA - 代码段
    VMA* code = new VMA();
    code->vm_start = 0x400000;
    code->vm_end = 0x450000;
    code->vm_pfn = 0x1000;  // 物理页帧号
    code->vm_flags = VMA_READ | VMA_EXEC | VMA_PRIVATE;
    code->name = "[code]";
    
    // 创建 VMA - 数据段
    VMA* data = new VMA();
    data->vm_start = 0x450000;
    data->vm_end = 0x460000;
    data->vm_pfn = 0x1050;
    data->vm_flags = VMA_READ | VMA_WRITE | VMA_PRIVATE;
    data->name = "[data]";
    
    // 创建 VMA - 堆
    VMA* heap = new VMA();
    heap->vm_start = 0x1000000;
    heap->vm_end = 0x2000000;
    heap->vm_pfn = 0x2000;
    heap->vm_flags = VMA_READ | VMA_WRITE | VMA_PRIVATE | VMA_ANONYMOUS;
    heap->name = "[heap]";
    
    // 创建 VMA - 栈
    VMA* stack = new VMA();
    stack->vm_start = 0x7fff0000;
    stack->vm_end = 0x80000000;
    stack->vm_pfn = 0x5000;
    stack->vm_flags = VMA_READ | VMA_WRITE | VMA_PRIVATE | VMA_ANONYMOUS;
    stack->name = "[stack]";
    
    // 添加 VMA 到内存管理器
    cout << "添加 VMA..." << endl;
    mm.add_vma(code);
    mm.add_vma(data);
    mm.add_vma(heap);
    mm.add_vma(stack);
    
    // 打印所有 VMA
    mm.print_vmas();
}

void demo_fast_lookup() {
    cout << "\n========== 快速查找演示 (O(log n)) ==========" << endl;
    
    MemoryManager mm;
    
    // 创建多个 VMA
    for (int i = 0; i < 10; i++) {
        VMA* vma = new VMA();
        vma->vm_start = 0x100000 + i * 0x10000;  // 每个 VMA 间隔 64KB
        vma->vm_end = vma->vm_start + 0x8000;     // 大小 32KB
        vma->vm_pfn = 0x1000 + i * 0x100;
        vma->vm_flags = VMA_READ | VMA_WRITE;
        vma->name = "region_0";
        
        mm.add_vma(vma);
    }
    
    cout << "创建了 10 个 VMA" << endl;
    cout << "总 VMA 数：" << mm.get_vma_count() << endl << endl;
    
    // 测试查找性能
    uint64_t test_addrs[] = {
        0x100000,  // 第一个 VMA 的起始
        0x108000,  // 第一个 VMA 的中间
        0x110000,  // 第二个 VMA
        0x150000,  // 中间的 VMA
        0x190000,  // 最后一个 VMA
        0x200000   // 不存在的地址
    };
    
    cout << "查找测试:" << endl;
    for (uint64_t addr : test_addrs) {
        VMA* vma = mm.find_vma(addr);
        if (vma) {
            cout << "  0x" << hex << addr << dec 
                 << " → 找到 VMA: 0x" << hex << vma->vm_start 
                 << "-0x" << vma->vm_end << dec << endl;
        } else {
            cout << "  0x" << hex << addr << dec 
                 << " → 未找到" << endl;
        }
    }
}

void demo_address_translation() {
    cout << "\n========== 虚拟地址转物理地址演示 ==========" << endl;
    
    MemoryManager mm;
    
    // 创建堆区域
    VMA* heap = new VMA();
    heap->vm_start = 0x1000000;
    heap->vm_end = 0x2000000;
    heap->vm_pfn = 0x100;  // 从物理页 0x100 开始
    heap->vm_flags = VMA_READ | VMA_WRITE | VMA_ANONYMOUS;
    heap->name = "[heap]";
    mm.add_vma(heap);
    
    // 创建共享库映射
    VMA* lib = new VMA();
    lib->vm_start = 0x7f000000;
    lib->vm_end = 0x7f100000;
    lib->vm_pfn = 0x5000;
    lib->vm_flags = VMA_READ | VMA_EXEC | VMA_SHARED;
    lib->name = "[lib.so]";
    mm.add_vma(lib);
    
    mm.print_vmas();
    
    // 地址转换测试
    cout << "地址转换测试:" << endl;
    uint64_t virt_addrs[] = {0x1000000, 0x1001000, 0x1500000, 0x7f000100};
    
    for (uint64_t vaddr : virt_addrs) {
        uint64_t paddr = mm.virt_to_phys(vaddr);
        if (paddr) {
            cout << "  虚拟地址：0x" << hex << vaddr 
                 << " → 物理地址：0x" << paddr << dec << endl;
        }
    }
}

void demo_vma_merge() {
    cout << "\n========== VMA 合并演示 ==========" << endl;
    
    MemoryManager mm;
    
    // 创建两个相邻且属性相同的 VMA
    VMA* vma1 = new VMA();
    vma1->vm_start = 0x100000;
    vma1->vm_end = 0x110000;
    vma1->vm_pfn = 0x100;
    vma1->vm_flags = VMA_READ | VMA_WRITE;
    vma1->name = "region_A";
    
    VMA* vma2 = new VMA();
    vma2->vm_start = 0x110000;  // 紧接 vma1
    vma2->vm_end = 0x120000;
    vma2->vm_pfn = 0x110;       // 物理地址也连续
    vma2->vm_flags = VMA_READ | VMA_WRITE;  // 相同标志
    vma2->name = "region_B";
    
    cout << "添加两个相邻的 VMA:" << endl;
    cout << "  VMA1: 0x100000-0x110000" << endl;
    cout << "  VMA2: 0x110000-0x120000" << endl;
    
    mm.add_vma(vma1);
    mm.add_vma(vma2);
    
    cout << "\n合并后的 VMA:" << endl;
    mm.print_vmas();
    
    // 创建不相邻的 VMA（不会合并）
    VMA* vma3 = new VMA();
    vma3->vm_start = 0x200000;
    vma3->vm_end = 0x210000;
    vma3->vm_pfn = 0x200;
    vma3->vm_flags = VMA_READ | VMA_WRITE;
    vma3->name = "region_C";
    
    mm.add_vma(vma3);
    
    cout << "添加第三个不相邻的 VMA:" << endl;
    mm.print_vmas();
}

void demo_list_traversal() {
    cout << "\n========== 链表遍历演示 (O(n)) ==========" << endl;
    
    MemoryManager mm;
    
    // 创建多个 VMA
    const char* names[] = {"[code]", "[data]", "[heap]", "[stack]", "[mmap]"};
    uint64_t starts[] = {0x400000, 0x450000, 0x1000000, 0x7fff0000, 0x7f000000};
    uint64_t sizes[] = {0x50000, 0x10000, 0x1000000, 0x10000, 0x100000};
    
    for (int i = 0; i < 5; i++) {
        VMA* vma = new VMA();
        vma->vm_start = starts[i];
        vma->vm_end = starts[i] + sizes[i];
        vma->vm_pfn = 0x1000 + i * 0x100;
        vma->vm_flags = VMA_READ | VMA_WRITE;
        vma->name = names[i];
        mm.add_vma(vma);
    }
    
    cout << "按地址顺序遍历所有 VMA（通过链表）:" << endl;
    int index = 0;
    mm.for_each_vma([&index](VMA* vma) {
        cout << "  [" << index++ << "] " << vma->name 
             << ": 0x" << hex << vma->vm_start 
             << "-0x" << vma->vm_end << dec << endl;
    });
}

void demo_performance_comparison() {
    cout << "\n========== 性能对比演示 ==========" << endl;
    
    MemoryManager mm;
    
    // 创建大量 VMA
    const int NUM_VMAS = 1000;
    for (int i = 0; i < NUM_VMAS; i++) {
        VMA* vma = new VMA();
        vma->vm_start = 0x100000 + i * 0x10000;
        vma->vm_end = vma->vm_start + 0x8000;
        vma->vm_pfn = 0x1000 + i * 0x100;
        vma->vm_flags = VMA_READ | VMA_WRITE;
        vma->name = "region";
        mm.add_vma(vma);
    }
    
    cout << "创建了 " << NUM_VMAS << " 个 VMA" << endl;
    
    // 红黑树查找：O(log n) ≈ O(log 1000) ≈ 10 次比较
    cout << "\n红黑树查找 (O(log n)):" << endl;
    cout << "  查找次数：~" << (int)(log(NUM_VMAS) / log(2)) << " 次比较" << endl;
    
    // 链表查找：O(n) ≈ O(1000) ≈ 500 次比较（平均）
    cout << "链表查找 (O(n)):" << endl;
    cout << "  查找次数：~" << NUM_VMAS / 2 << " 次比较（平均）" << endl;
    
    cout << "\n加速比：约 " << (NUM_VMAS / 2) / (int)(log(NUM_VMAS) / log(2)) << " 倍！" << endl;
    
    // 实际测试
    uint64_t target_addr = 0x100000 + 500 * 0x10000;
    auto start = std::chrono::high_resolution_clock::now();
    VMA* result = mm.find_vma(target_addr);
    auto end = std::chrono::high_resolution_clock::now();
    
    cout << "\n实际查找测试:" << endl;
    cout << "  查找地址：0x" << hex << target_addr << dec << endl;
    if (result) {
        cout << "  找到 VMA: 0x" << hex << result->vm_start 
             << "-0x" << result->vm_end << dec << endl;
    }
    cout << "  耗时：" << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() 
         << " ns" << endl;
}

int main() {
    system("chcp 65001 > nul");
    cout << "╔═══════════════════════════════════════════════════════╗" << endl;
    cout << "║  Linux 内存管理：list_head + 红黑树                   ║" << endl;
    cout << "║  高效查找 & 修改虚拟地址 & 实际地址映射               ║" << endl;
    cout << "╚═══════════════════════════════════════════════════════╝" << endl;
    
    demo_basic_usage();
    demo_fast_lookup();
    demo_address_translation();
    demo_vma_merge();
    demo_list_traversal();
    demo_performance_comparison();
    
    cout << "\n========== 演示完成 ==========" << endl;
    
    return 0;
}
