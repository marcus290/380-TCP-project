/** AVL binary search tree for buffering out-of-sequence packets in C, adapted from 
 * https://www.geeksforgeeks.org, provided under a Creative Commons Attribution-ShareAlike  
 * 4.0 International (CC BY-SA 4.0) license https://creativecommons.org/licenses/by-sa/4.0/.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "avl-oos.h"
#include "PacketLoss.h"

// // A utility function to get maximum of two integers 
int max_oos(int a, int b) 
{ 
    return ((a > b)? a : b); 
} 

// C function to search a given key in a given BST 
oos_avlNode* oos_search(oos_avlNode* root, uint64_t key) 
{ 
    // Base Cases: root is null or key is present at root 
    if (root == NULL || root->key == key) 
       return root; 
     
    // Key is greater than root's key 
    if (root->key < key) 
       return oos_search(root->right, key); 
  
    // Key is smaller than root's key 
    return oos_search(root->left, key); 
} 

// A utility function to create a new BST node 
oos_avlNode* oos_newNode(uint64_t item, struct heap* value) 
{ 
    oos_avlNode* temp =  calloc(1, sizeof(oos_avlNode)); 
    temp->key = item; 
    temp->left = temp->right = NULL; 
    temp->height = 1;
    temp->value = value;
    return temp; 
} 

// A utility function to do inorder traversal of BST 
void oos_inorder(oos_avlNode* root) 
{ 
    if (root != NULL) 
    { 
        oos_inorder(root->left); 
        printf("%lx \n", root->key); 
        oos_inorder(root->right); 
    } 
} 
   

// A utility function to get the height of the tree 
int oos_height(oos_avlNode* N) 
{ 
    if (N == NULL) 
        return 0; 
    return N->height; 
} 

// A utility function to right rotate subtree rooted with y 
oos_avlNode* oos_rightRotate(oos_avlNode* y) 
{ 
    oos_avlNode* x = y->left; 
    oos_avlNode* T2 = x->right; 
  
    // Perform rotation 
    x->right = y; 
    y->left = T2; 
  
    // Update heights 
    y->height = max_oos(oos_height(y->left), oos_height(y->right))+1; 
    x->height = max_oos(oos_height(x->left), oos_height(x->right))+1; 
  
    // Return new root 
    return x; 
} 


// A utility function to left rotate subtree rooted with x 
oos_avlNode* oos_leftRotate(oos_avlNode* x) 
{ 
    oos_avlNode* y = x->right; 
    oos_avlNode* T2 = y->left; 
  
    // Perform rotation 
    y->left = x; 
    x->right = T2; 
  
    //  Update heights 
    x->height = max_oos(oos_height(x->left), oos_height(x->right))+1; 
    y->height = max_oos(oos_height(y->left), oos_height(y->right))+1; 
  
    // Return new root 
    return y; 
} 

// Get Balance factor of node N 
int oos_getBalance(oos_avlNode* N) 
{ 
    if (N == NULL) 
        return 0; 
    return oos_height(N->left) - oos_height(N->right); 
} 

/* A utility function to insert a new node with given key in BST */
oos_avlNode* oos_insert(oos_avlNode* node, uint64_t key, struct heap* value) 
{ 
    /* 1.  Perform the normal BST insertion */
    if (node == NULL) 
        return(oos_newNode(key, value)); 
  
    if (key < node->key) 
        node->left  = oos_insert(node->left, key, value); 
    else if (key > node->key) 
        node->right = oos_insert(node->right, key, value); 
    else // Equal keys are not allowed in BST 
        return node; 
  
    /* 2. Update height of this ancestor node */
    node->height = 1 + max_oos(oos_height(node->left), 
                           oos_height(node->right)); 
  
    /* 3. Get the balance factor of this ancestor 
          node to check whether this node became 
          unbalanced */
    int balance = oos_getBalance(node); 
  
    // If this node becomes unbalanced, then 
    // there are 4 cases 
  
    // Left Left Case 
    if (balance > 1 && key < node->left->key) 
        return oos_rightRotate(node); 
  
    // Right Right Case 
    if (balance < -1 && key > node->right->key) 
        return oos_leftRotate(node); 
  
    // Left Right Case 
    if (balance > 1 && key > node->left->key) 
    { 
        node->left = oos_leftRotate(node->left); 
        return oos_rightRotate(node); 
    } 
  
    // Right Left Case 
    if (balance < -1 && key < node->right->key) 
    { 
        node->right = oos_rightRotate(node->right); 
        return oos_leftRotate(node); 
    } 
  
    /* return the (unchanged) node pointer */
    return node; 
} 

/* Given a non-empty binary search tree, return the 
   node with minimum key value found in that tree. 
   Note that the entire tree does not need to be 
   searched. */
oos_avlNode* oos_minValueNode(oos_avlNode* node) 
{ 
    oos_avlNode* current = node; 
  
    /* loop down to find the leftmost leaf */
    while (current->left != NULL) 
        current = current->left; 
  
    return current; 
} 

// Recursive function to delete a node with given key 
// from subtree with given root. It returns root of 
// the modified subtree. 
oos_avlNode* oos_deleteNode(oos_avlNode* root, uint64_t key) 
{ 
    // STEP 1: PERFORM STANDARD BST DELETE 
//   if (key == (uint64_t) 0xf1f400005ba81LL && search(root, (uint64_t) 0x101f4000089defLL) != NULL) {
// 			printf("Entering Step 1: %llx expecting seq num %d since %.3f\n", 0x101f4000089defLL, search(root, (uint64_t) 0x101f4000089defLL)->value->seqNum, search(root, (uint64_t) 0x101f4000089defLL)->value->timeStamp);
// 		    inorder(root);
        // }
       
    if (root == NULL) 
        return root; 
  
    // If the key to be deleted is smaller than the 
    // root's key, then it lies in left subtree 
    if ( key < root->key ) 
        root->left = oos_deleteNode(root->left, key); 
  
    // If the key to be deleted is greater than the 
    // root's key, then it lies in right subtree 
    else if( key > root->key ) 
        root->right = oos_deleteNode(root->right, key); 
  
    // if key is same as root's key, then This is 
    // the node to be deleted 
    else
    { 
        // node with only one child or no child 
        if( (root->left == NULL) || (root->right == NULL) ) 
        { 
            oos_avlNode* temp = root->left ? root->left : root->right; 
  
            // No child case 
            if (temp == NULL) 
            { 
                temp = root; 
                root = NULL; 
            } 
            else // One child case 
             *root = *temp; // Copy the contents of 
                            // the non-empty child 
            free(temp); 
        } 
        else
        { 
            // node with two children: Get the inorder 
            // successor (smallest in the right subtree) 
            oos_avlNode* temp = oos_minValueNode(root->right); 
  
            // Copy the inorder successor's data to this node 
            root->key = temp->key; 
            root->height = temp->height;
            root->value = temp->value;
  
            // Delete the inorder successor 
            root->right = oos_deleteNode(root->right, temp->key); 
        } 
    } 
  
    // If the tree had only one node then return 
    if (root == NULL) 
      return root; 
//   if (key == (uint64_t) 0xf1f400005ba81LL && search(root, (uint64_t) 0x101f4000089defLL) != NULL) {
// 			printf("Entering Step 2: %llx expecting seq num %d since %.3f\n", 0x101f4000089defLL, search(root, (uint64_t) 0x101f4000089defLL)->value->seqNum, search(root, (uint64_t) 0x101f4000089defLL)->value->timeStamp);
//             inorder(root);
//         }
        
    // STEP 2: UPDATE HEIGHT OF THE CURRENT NODE 
    root->height = 1 + max_oos(oos_height(root->left), 
                           oos_height(root->right)); 
  
    // STEP 3: GET THE BALANCE FACTOR OF THIS NODE (to 
    // check whether this node became unbalanced) 
    int balance = oos_getBalance(root); 
  
    // If this node becomes unbalanced, then there are 4 cases 
  
    // Left Left Case 
    if (balance > 1 && oos_getBalance(root->left) >= 0) 
        return oos_rightRotate(root); 
  
    // Left Right Case 
    if (balance > 1 && oos_getBalance(root->left) < 0) 
    { 
        root->left =  oos_leftRotate(root->left); 
        return oos_rightRotate(root); 
    } 
  
    // Right Right Case 
    if (balance < -1 && oos_getBalance(root->right) <= 0) 
        return oos_leftRotate(root); 
  
    // Right Left Case 
    if (balance < -1 && oos_getBalance(root->right) > 0) 
    { 
        root->right = oos_rightRotate(root->right); 
        return oos_leftRotate(root); 
    } 
  
    return root; 
} 