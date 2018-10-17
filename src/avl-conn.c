/** AVL binary search tree for storing open connections in C, adapted from 
 * https://www.geeksforgeeks.org, provided under a Creative Commons Attribution-ShareAlike  
 * 4.0 International (CC BY-SA 4.0) license https://creativecommons.org/licenses/by-sa/4.0/.
 */

#include <stdlib.h>
#include <stdint.h>

#include "avl-conn.h"
#include "PacketLoss.h"

// // A utility function to get maximum of two integers 
int max_conn(int a, int b) 
{ 
    return (a > b)? a : b; 
} 

// C function to search a given key in a given BST 
avlNode* search(avlNode* root, uint64_t key) 
{ 
    // Base Cases: root is null or key is present at root 
    if (root == NULL || root->key == key) 
       return root; 
     
    // Key is greater than root's key 
    if (root->key < key) 
       return search(root->right, key); 
  
    // Key is smaller than root's key 
    return search(root->left, key); 
} 

// A utility function to create a new BST node 
avlNode* newNode(uint64_t item, struct connStatus* value) 
{ 
    avlNode* temp =  calloc(1, sizeof(avlNode)); 
    temp->key = item; 
    temp->left = temp->right = NULL; 
    temp->height = 1;
    temp->value = value;
    return temp; 
} 

// A utility function to do inorder traversal of BST 
void inorder(avlNode* root) 
{ 
    if (root != NULL) 
    { 
        inorder(root->left); 
        printf("%llx \n", root->key); 
        inorder(root->right); 
    } 
} 
   

// A utility function to get the height of the tree 
int height(avlNode* N) 
{ 
    if (N == NULL) 
        return 0; 
    return N->height; 
} 

// A utility function to right rotate subtree rooted with y 
avlNode* rightRotate(avlNode* y) 
{ 
    avlNode* x = y->left; 
    avlNode* T2 = x->right; 
  
    // Perform rotation 
    x->right = y; 
    y->left = T2; 
  
    // Update heights 
    y->height = max_conn(height(y->left), height(y->right))+1; 
    x->height = max_conn(height(x->left), height(x->right))+1; 
  
    // Return new root 
    return x; 
} 


// A utility function to left rotate subtree rooted with x 
avlNode* leftRotate(avlNode* x) 
{ 
    avlNode* y = x->right; 
    avlNode* T2 = y->left; 
  
    // Perform rotation 
    y->left = x; 
    x->right = T2; 
  
    //  Update heights 
    x->height = max_conn(height(x->left), height(x->right))+1; 
    y->height = max_conn(height(y->left), height(y->right))+1; 
  
    // Return new root 
    return y; 
} 

// Get Balance factor of node N 
int getBalance(avlNode* N) 
{ 
    if (N == NULL) 
        return 0; 
    return height(N->left) - height(N->right); 
} 

/* A utility function to insert a new node with given key in BST */
avlNode* insert(avlNode* node, uint64_t key, struct connStatus* value) 
{ 
    /* 1.  Perform the normal BST insertion */
    if (node == NULL) 
        return(newNode(key, value)); 
  
    if (key < node->key) 
        node->left  = insert(node->left, key, value); 
    else if (key > node->key) 
        node->right = insert(node->right, key, value); 
    else // Equal keys are not allowed in BST 
        return node; 
  
    /* 2. Update height of this ancestor node */
    node->height = 1 + max_conn(height(node->left), 
                           height(node->right)); 
  
    /* 3. Get the balance factor of this ancestor 
          node to check whether this node became 
          unbalanced */
    int balance = getBalance(node); 
  
    // If this node becomes unbalanced, then 
    // there are 4 cases 
  
    // Left Left Case 
    if (balance > 1 && key < node->left->key) 
        return rightRotate(node); 
  
    // Right Right Case 
    if (balance < -1 && key > node->right->key) 
        return leftRotate(node); 
  
    // Left Right Case 
    if (balance > 1 && key > node->left->key) 
    { 
        node->left =  leftRotate(node->left); 
        return rightRotate(node); 
    } 
  
    // Right Left Case 
    if (balance < -1 && key < node->right->key) 
    { 
        node->right = rightRotate(node->right); 
        return leftRotate(node); 
    } 
  
    /* return the (unchanged) node pointer */
    return node; 
} 

/* Given a non-empty binary search tree, return the 
   node with minimum key value found in that tree. 
   Note that the entire tree does not need to be 
   searched. */
avlNode* minValueNode(avlNode* node) 
{ 
    avlNode* current = node; 
  
    /* loop down to find the leftmost leaf */
    while (current->left != NULL) 
        current = current->left; 
  
    return current; 
} 

// Recursive function to delete a node with given key 
// from subtree with given root. It returns root of 
// the modified subtree. 
avlNode* deleteNode(avlNode* root, uint64_t key) 
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
        root->left = deleteNode(root->left, key); 
  
    // If the key to be deleted is greater than the 
    // root's key, then it lies in right subtree 
    else if( key > root->key ) 
        root->right = deleteNode(root->right, key); 
  
    // if key is same as root's key, then This is 
    // the node to be deleted 
    else
    { 
        // node with only one child or no child 
        if( (root->left == NULL) || (root->right == NULL) ) 
        { 
            avlNode* temp = root->left ? root->left : root->right; 
  
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
            avlNode* temp = minValueNode(root->right); 
  
            // Copy the inorder successor's data to this node 
            root->key = temp->key; 
            root->height = temp->height;
            root->value = temp->value;
  
            // Delete the inorder successor 
            root->right = deleteNode(root->right, temp->key); 
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
    root->height = 1 + max_conn(height(root->left), 
                           height(root->right)); 
  
    // STEP 3: GET THE BALANCE FACTOR OF THIS NODE (to 
    // check whether this node became unbalanced) 
    int balance = getBalance(root); 
  
    // If this node becomes unbalanced, then there are 4 cases 
  
    // Left Left Case 
    if (balance > 1 && getBalance(root->left) >= 0) 
        return rightRotate(root); 
  
    // Left Right Case 
    if (balance > 1 && getBalance(root->left) < 0) 
    { 
        root->left =  leftRotate(root->left); 
        return rightRotate(root); 
    } 
  
    // Right Right Case 
    if (balance < -1 && getBalance(root->right) <= 0) 
        return leftRotate(root); 
  
    // Right Left Case 
    if (balance < -1 && getBalance(root->right) > 0) 
    { 
        root->right = rightRotate(root->right); 
        return leftRotate(root); 
    } 
  
    return root; 
} 