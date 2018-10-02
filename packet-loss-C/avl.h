/** AVL binary search tree in C, adapted from https://www.geeksforgeeks.org */

typedef struct avl_node avlNode;

struct avl_node{
    uint64_t key;
    avlNode* right;
    avlNode* left;
    int height;
    struct connStatus* value;
};

typedef struct {
    uint64_t key;
    struct heap* value;
} oOS_avlNode;

avlNode* search(avlNode* root, uint64_t key);
avlNode* insert(avlNode* node, uint64_t key, struct connStatus* value);
avlNode* deleteNode(avlNode* root, uint64_t key);