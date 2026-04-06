/*
Linux 风格的红黑树
用于高效查找（O(log n)）
*/
#ifndef _RBTREE_H
#define _RBTREE_H

#include <cstdint>

// 红黑树节点颜色
enum Color { RED = 0, BLACK = 1 };

// 红黑树节点结构
struct RBNode {
    RBNode* left;    // 左子节点
    RBNode* right;   // 右子节点
    RBNode* parent;  // 父节点
    Color color;     // 节点颜色
    
    RBNode() : left(nullptr), right(nullptr), parent(nullptr), color(RED) {}
};

// 红黑树根节点
struct RBRoot {
    RBNode* rb_node;  // 根节点指针
    
    RBRoot() : rb_node(nullptr) {}
};

// 红黑树操作类
class RBtree {
public:
    // 初始化红黑树
    static inline void init(RBRoot* root) {
        root->rb_node = nullptr;
    }
    
    // 插入节点（通用版本 - 使用指针地址比较）
    static void insert(RBRoot* root, RBNode* node);
    
    // 插入节点（带自定义比较器版本）
    template<typename Compare>
    static void insert_with_compare(RBRoot* root, RBNode* node, Compare cmp);
    
    // 删除节点
    static void erase(RBRoot* root, RBNode* node);
    
    // 查找节点（需要提供比较函数）
    template<typename Compare>
    static RBNode* search(RBRoot* root, Compare cmp);
    
    // 插入后调整平衡（公开给 MemoryManager 使用）
    static void insert_fixup(RBRoot* root, RBNode* node);
    
private:
    // 左旋
    static void rotate_left(RBRoot* root, RBNode* node);
    
    // 右旋
    static void rotate_right(RBRoot* root, RBNode* node);
    
    // 删除后调整平衡
    static void erase_fixup(RBRoot* root, RBNode* node);
};

// 模板函数实现放在头文件中
template<typename Compare>
void RBtree::insert_with_compare(RBRoot* root, RBNode* node, Compare cmp) {
    RBNode** current = &root->rb_node;
    RBNode* parent = nullptr;
    
    // 使用自定义比较器找到插入位置
    while (*current) {
        parent = *current;
        int result = cmp(node, *current);
        
        if (result < 0) {
            current = &((*current)->left);
        } else {
            current = &((*current)->right);
        }
    }
    
    // 链接新节点
    *current = node;
    node->parent = parent;
    node->left = nullptr;
    node->right = nullptr;
    node->color = RED;
    
    // 调整平衡
    insert_fixup(root, node);
}

// 辅助宏：获取包含 RBNode 的父结构体
#define RB_ENTRY(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

#endif
