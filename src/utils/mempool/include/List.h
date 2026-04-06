/*
Linux 风格的双向循环链表
用于高效遍历和顺序访问
*/
#ifndef _LIST_H
#define _LIST_H

#include <cstddef>

// Linux list_head 结构 - 可以嵌入到任何数据结构中
struct ListHead {
    ListHead* next;
    ListHead* prev;
    
    // 初始化链表头
    static inline void init(ListHead* head) {
        head->next = head;
        head->prev = head;
    }
    
    void init() {
        init(this);
    }
};

// 添加节点到链表头部（O(1)）
inline void list_add(ListHead* new_node, ListHead* head) {
    ListHead* next = head->next;
    
    new_node->next = next;
    new_node->prev = head;
    head->next = new_node;
    next->prev = new_node;
}

// 添加节点到链表尾部（O(1)）
inline void list_add_tail(ListHead* new_node, ListHead* head) {
    ListHead* prev = head->prev;
    
    new_node->next = head;
    new_node->prev = prev;
    prev->next = new_node;
    head->prev = new_node;
}

// 从链表中删除节点（O(1)）
inline void list_del(ListHead* node) {
    ListHead* next = node->next;
    ListHead* prev = node->prev;
    
    prev->next = next;
    next->prev = prev;
    
    node->next = nullptr;
    node->prev = nullptr;
}

// 判断链表是否为空
inline bool list_empty(const ListHead* head) {
    return head->next == head;
}

// 获取包含 list_head 的父结构体指针（类似 container_of）
#define LIST_ENTRY(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

// 遍历链表
#define LIST_FOREACH(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

// 安全遍历链表（允许删除当前节点）
#define LIST_FOREACH_SAFE(pos, n, head) \
    for (pos = (head)->next, n = pos->next; \
         pos != (head); \
         pos = n, n = pos->next)

#endif
