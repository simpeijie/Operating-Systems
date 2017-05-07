/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>

struct meta *head = NULL;

void *mm_malloc(size_t size) {
	/* YOUR CODE HERE */
    if (size == 0)
	    return NULL;
	struct meta *node = head;
	struct meta *prev = NULL;

	while (node != NULL && !(node->free && node->size >= size)) {
		prev = node;
		node = node->next;
	}
	/* No sufficiently large block is found;
		create more space on heap */
	struct meta *block;
	if (node == NULL) {
		if ((block = create_space(prev, size)) == NULL) {
			return NULL;
		}
		return block->data;
	}
	/* Found a large free block */
	// If data block can fit more than one block; split it into two
	size_t residual = node->size - size;
	if (residual >= sizeof(struct meta)) {
		split_block(node, size, residual);
	}
	else {
		node->free = false;
	}
	return node->data;
}

void *mm_realloc(void *ptr, size_t size) {
    /* YOUR CODE HERE */
    if (ptr == NULL && size == 0) {
    	return mm_malloc(0);
    }
    if (size == 0) {
    	mm_free(ptr);
    	return NULL;
    }
    if (ptr == NULL) {
    	return mm_malloc(size);
    }
    struct meta *block = get_block(ptr);
    if (size < block->size) {
    	return ptr;
    }
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
    	return NULL;
    }
    memcpy(new_ptr, ptr, block->size);
    memset((new_ptr + block->size), 0, size - block->size);
    mm_free(ptr);
    return new_ptr;
}

void mm_free(void *ptr) {
    /* YOUR CODE HERE */
    if (ptr == NULL) 
    	return;
    struct meta *block = get_block(ptr);
    if (block == NULL)
    	return;
    /* Handle coalescing of free memory */
    // Block to the left is free 
    if (block->prev != NULL) {
		if (block->prev->free) {
			struct meta *temp = block->prev;
			block->size += temp->size;
    	   	if (block->prev->prev != NULL) {
    			temp->prev->next = block;
    			temp->next = NULL;
    			block->prev = temp->prev;
    			temp->prev = NULL;
    		}
    		else {
    			block->prev = NULL;
    			temp->next = NULL;
    			head = block;
    		}
    	}
    }
    // Block to the right is free 
    if (block->next != NULL) {
	    if (block->next->free) {
	    	struct meta *temp = block->next;
	    	block->size += temp->size;
	    	if (block->next->next != NULL) {
		    	temp->next->prev = block;	
		    	temp->prev = NULL;
		    	block->next = temp->next;
		    	temp->next = NULL;
		    }
		    else {
		    	block->next = NULL;
		    	temp->prev = NULL;
		    }
    	}
    }
    memset(block->data, 0, block->size);
    block->free = true;
}

struct meta *create_space(struct meta *prev, size_t size) {
	void *brk = sbrk(sizeof(struct meta) + size); 
	if (brk == (void *) -1)
		return NULL;
	struct meta *block = (struct meta *) brk;
	block->prev = prev;
	block->next = NULL;
	block->free = false;
	block->size = size;
	memset(block->data, 0, size);
	/* Mapped region is empty */
	if (prev == NULL) {
		head = block;
	}
	else {
		block->next = prev->next;
		prev->next = block;
	}
	return block;
}

void split_block(struct meta *node, size_t size, size_t res_size) {
	node->size = size;
	node->free = false;
	struct meta *block = (void *) node + sizeof(struct meta) + size;
	block->prev = node;
	block->next = node->next;
	block->free = true;
	block->size = res_size - sizeof(struct meta);
	if (node->next != NULL) 
		node->next->prev = block;
	node->next = block;
}

struct meta *get_block(void *ptr) {
	struct meta *m = head;
	while (m != NULL && m->data != ptr) {
		m = m->next;
	}
	return m;
}
