/** AVL binary search tree for buffering out-of-sequence packets in C, adapted from 
 * https://www.geeksforgeeks.org, provided under a Creative Commons Attribution-ShareAlike  
 * 4.0 International (CC BY-SA 4.0) license https://creativecommons.org/licenses/by-sa/4.0/.
 */

typedef struct oos_avl_node oos_avlNode;

struct oos_avl_node{
    uint64_t key;
    oos_avlNode* right;
    oos_avlNode* left;
    int height;
    struct heap* value;
};

oos_avlNode* oos_search(oos_avlNode* root, uint64_t key);
oos_avlNode* oos_insert(oos_avlNode* node, uint64_t key, struct heap* value);
oos_avlNode* oos_deleteNode(oos_avlNode* root, uint64_t key);