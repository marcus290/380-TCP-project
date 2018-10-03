/** Hash table in C, adapted from https://gist.github.com/martinkunev/1365481 */

#include <stdlib.h>
#include <stdint.h>

#include "min-heap.h"
#include "PacketLoss.h"

static const unsigned int base_size = 4;

// Compare function for the heap

int cmp(struct packet* a, struct packet* b) {
    return (a->seqNum <= b->seqNum);
}

// Prepares the heap for use
void heap_init(struct heap* h)
{
	*h = (struct heap){
		.size = base_size,
		.count = 0,
		.data = malloc(sizeof(struct packet*) * base_size)
	};
	if (!h->data) _exit(1); // Exit if the memory allocation fails
}

// Inserts element to the heap
void heap_push(struct heap* h, struct packet* value)
{
	unsigned int index, parent;

	// Resize the heap if it is too small to hold all the data
	if (h->count == h->size)
	{
		h->size <<= 1;
		h->data = realloc(h->data, sizeof(struct packet*) * h->size);
		if (!h->data) _exit(1); // Exit if the memory allocation fails
	}

	// Find out where to put the element and put it
	for(index = h->count++; index; index = parent)
	{
		parent = (index - 1) >> 1;
		if (cmp(h->data[parent], value)) break;
		h->data[index] = h->data[parent];
	}
	h->data[index] = value;
}

// Removes the smallest element from the heap
void heap_pop(struct heap* h)
{
	unsigned int index, swap, other;
	// Remove the biggest element
	struct packet* temp = h->data[--h->count];
	
	
	// Resize the heap if it's consuming too much memory
	if ((h->count <= (h->size >> 2)) && (h->size > base_size))
	{
		h->size >>= 1;
		h->data = realloc(h->data, sizeof(struct packet*) * h->size);
		if (!h->data) _exit(1); // Exit if the memory allocation fails
	}

	// Reorder the elements
	for(index = 0; 1; index = swap)
	{
		// Find the child to swap with
		swap = (index << 1) + 1;
		if (swap >= h->count) break; // If there are no children, the heap is reordered
		other = swap + 1;
		if ((other < h->count) && cmp(h->data[other], h->data[swap])) swap = other;
		if (cmp(temp, h->data[swap])) break; // If the smaller child is less than or equal to its parent, the heap is reordered

		h->data[index] = h->data[swap];
	}
	h->data[index] = temp;
}

// Heapifies a non-empty array
// void heapify(struct packet* data[restrict], unsigned int count)
// {
// 	unsigned int item, index, swap, other;
// 	struct packet* temp;

// 	// Move every non-leaf element to the right position in its subtree
// 	item = (count >> 1) - 1;
// 	while (1)
// 	{
// 		// Find the position of the current element in its subtree
// 		temp = data[item];
// 		for(index = item; 1; index = swap)
// 		{
// 			// Find the child to swap with
// 			swap = (index << 1) + 1;
// 			if (swap >= count) break; // If there are no children, the current element is positioned
// 			other = swap + 1;
// 			if ((other < count) && cmp(data[other], data[swap])) swap = other;
// 			if (cmp(temp, data[swap])) break; // If the bigger child is smaller than or equal to the parent, the heap is reordered

// 			data[index] = data[swap];
// 		}
// 		if (index != item) data[index] = temp;

// 		if (!item) return;
// 		--item;
// 	}
// }


