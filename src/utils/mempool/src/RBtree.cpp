#include "RBtree.h"
#include <iostream>

// 获取节点的叔叔节点
static inline RBNode* get_uncle(RBNode* node) {
    RBNode* parent = node->parent;
    if (!parent || !parent->parent) return nullptr;
    
    RBNode* grandparent = parent->parent;
    return (parent == grandparent->left) ? grandparent->right : grandparent->left;
}

// 获取节点的祖父节点
static inline RBNode* get_grandparent(RBNode* node) {
    if (!node || !node->parent) return nullptr;
    return node->parent->parent;
}

// 左旋
void RBtree::rotate_left(RBRoot* root, RBNode* node) {
    RBNode* right = node->right;
    
    // 将右子节点的左子树挂到当前节点的右子树
    node->right = right->left;
    if (right->left) {
        right->left->parent = node;
    }
    
    // 更新右子节点的父节点
    right->parent = node->parent;
    
    if (!node->parent) {
        // 当前节点是根节点
        root->rb_node = right;
    } else if (node == node->parent->left) {
        node->parent->left = right;
    } else {
        node->parent->right = right;
    }
    
    // 当前节点成为右子节点的左子节点
    right->left = node;
    node->parent = right;
}

// 右旋
void RBtree::rotate_right(RBRoot* root, RBNode* node) {
    RBNode* left = node->left;
    
    // 将左子节点的右子树挂到当前节点的左子树
    node->left = left->right;
    if (left->right) {
        left->right->parent = node;
    }
    
    // 更新左子节点的父节点
    left->parent = node->parent;
    
    if (!node->parent) {
        // 当前节点是根节点
        root->rb_node = left;
    } else if (node == node->parent->right) {
        node->parent->right = left;
    } else {
        node->parent->left = left;
    }
    
    // 当前节点成为左子节点的右子节点
    left->right = node;
    node->parent = left;
}

// 插入后调整平衡
void RBtree::insert_fixup(RBRoot* root, RBNode* node) {
    RBNode* parent = node->parent;
    
    // 情况 1: 父节点是红色，需要调整
    while (parent && parent->color == RED) {
        RBNode* grandparent = get_grandparent(node);
        RBNode* uncle = get_uncle(node);
        
        // 父节点是祖父节点的左子节点
        if (parent == grandparent->left) {
            if (uncle && uncle->color == RED) {
                // 情况 1.1: 叔叔节点是红色
                // 只需重新着色
                parent->color = BLACK;
                uncle->color = BLACK;
                grandparent->color = RED;
                
                node = grandparent;
                parent = node->parent;
            } else {
                if (node == parent->right) {
                    // 情况 1.2: 叔叔节点是黑色，当前节点是右子节点
                    // 左旋转换为情况 1.3
                    rotate_left(root, parent);
                    std::swap(parent, node);
                }
                
                // 情况 1.3: 叔叔节点是黑色，当前节点是左子节点
                // 右旋 + 重新着色
                parent->color = BLACK;
                grandparent->color = RED;
                rotate_right(root, grandparent);
            }
        } else {
            // 对称情况：父节点是祖父节点的右子节点
            if (uncle && uncle->color == RED) {
                // 情况 2.1: 叔叔节点是红色
                parent->color = BLACK;
                uncle->color = BLACK;
                grandparent->color = RED;
                
                node = grandparent;
                parent = node->parent;
            } else {
                if (node == parent->left) {
                    // 情况 2.2: 叔叔节点是黑色，当前节点是左子节点
                    rotate_right(root, parent);
                    std::swap(parent, node);
                }
                
                // 情况 2.3: 叔叔节点是黑色，当前节点是右子节点
                parent->color = BLACK;
                grandparent->color = RED;
                rotate_left(root, grandparent);
            }
        }
    }
    
    // 确保根节点是黑色
    root->rb_node->color = BLACK;
}

// 删除后调整平衡
void RBtree::erase_fixup(RBRoot* root, RBNode* node) {
    while (node != root->rb_node && node->color == BLACK) {
        if (node == node->parent->left) {
            RBNode* sibling = node->parent->right;
            
            if (sibling->color == RED) {
                // 情况 1: 兄弟节点是红色
                sibling->color = BLACK;
                node->parent->color = RED;
                rotate_left(root, node->parent);
                sibling = node->parent->right;
            }
            
            if ((!sibling->left || sibling->left->color == BLACK) &&
                (!sibling->right || sibling->right->color == BLACK)) {
                // 情况 2: 兄弟节点的两个子节点都是黑色
                sibling->color = RED;
                node = node->parent;
            } else {
                if (!sibling->right || sibling->right->color == BLACK) {
                    // 情况 3: 兄弟节点的左子节点是红色，右子节点是黑色
                    if (sibling->left) sibling->left->color = BLACK;
                    sibling->color = RED;
                    rotate_right(root, sibling);
                    sibling = node->parent->right;
                }
                
                // 情况 4: 兄弟节点的右子节点是红色
                sibling->color = node->parent->color;
                node->parent->color = BLACK;
                if (sibling->right) sibling->right->color = BLACK;
                rotate_left(root, node->parent);
                node = root->rb_node;
            }
        } else {
            // 对称情况
            RBNode* sibling = node->parent->left;
            
            if (sibling->color == RED) {
                sibling->color = BLACK;
                node->parent->color = RED;
                rotate_right(root, node->parent);
                sibling = node->parent->left;
            }
            
            if ((!sibling->left || sibling->left->color == BLACK) &&
                (!sibling->right || sibling->right->color == BLACK)) {
                sibling->color = RED;
                node = node->parent;
            } else {
                if (!sibling->left || sibling->left->color == BLACK) {
                    if (sibling->right) sibling->right->color = BLACK;
                    sibling->color = RED;
                    rotate_left(root, sibling);
                    sibling = node->parent->left;
                }
                
                sibling->color = node->parent->color;
                node->parent->color = BLACK;
                if (sibling->left) sibling->left->color = BLACK;
                rotate_right(root, node->parent);
                node = root->rb_node;
            }
        }
    }
    
    if (node) node->color = BLACK;
}

// 插入节点（通用版本，使用外部比较器）
// 注意：此函数使用指针地址作为 key 进行简单比较
// 对于 VMA 等需要自定义 key 的场景，请使用 MemoryManager::insert_vma_rb()
void RBtree::insert(RBRoot* root, RBNode* node) {
    RBNode** current = &root->rb_node;
    RBNode* parent = nullptr;
    
    // 找到插入位置（二叉搜索树）
    // 注意：这里使用指针地址作为 key 进行比较
    // 这是一个简化的通用实现，实际应用中应该根据具体数据结构的 key 值比较
    while (*current) {
        parent = *current;
        // 使用指针地址比较（仅适用于演示和测试）
        if (node < *current) {
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

// 删除节点
void RBtree::erase(RBRoot* root, RBNode* node) {
    RBNode* child = nullptr;
    RBNode* successor = nullptr;
    Color removed_color = node->color;
    
    // 情况 1: 节点有两个非空子节点
    if (node->left && node->right) {
        // 找到右子树的最小节点（中序后继）
        successor = node->right;
        while (successor->left) {
            successor = successor->left;
        }
        
        // 用后继节点替换当前节点
        node->left->parent = successor;
        successor->left = node->left;
        
        if (successor != node->right) {
            child = successor->right;
            if (child) child->parent = successor->parent;
            successor->parent->left = child;
            successor->right = node->right;
            successor->right->parent = successor;
        } else {
            child = successor->right;
            if (child) child->parent = successor;
        }
        
        if (node->parent) {
            if (node == node->parent->left) {
                node->parent->left = successor;
            } else {
                node->parent->right = successor;
            }
        } else {
            root->rb_node = successor;
        }
        
        successor->parent = node->parent;
        std::swap(successor->color, node->color);
        successor = node;
    } else {
        // 情况 2: 节点有 0 个或 1 个子节点
        child = node->left ? node->left : node->right;
        
        if (child) {
            child->parent = node->parent;
        }
        
        if (!node->parent) {
            root->rb_node = child;
        } else {
            if (node == node->parent->left) {
                node->parent->left = child;
            } else {
                node->parent->right = child;
            }
        }
        
        if (removed_color == BLACK && child) {
            erase_fixup(root, child);
        }
    }
}