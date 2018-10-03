/** AVL binary search tree for storing open connections in C, adapted from 
 * https://www.geeksforgeeks.org, provided under a Creative Commons Attribution-ShareAlike  
 * 4.0 International (CC BY-SA 4.0) license https://creativecommons.org/licenses/by-sa/4.0/.
 */

typedef struct avl_node avlNode;

struct avl_node{
    uint64_t key;
    avlNode* right;
    avlNode* left;
    int height;
    struct connStatus* value;
};

avlNode* search(avlNode* root, uint64_t key);
avlNode* insert(avlNode* node, uint64_t key, struct connStatus* value);
avlNode* deleteNode(avlNode* root, uint64_t key);