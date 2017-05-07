/*
 * mm_alloc.h
 *
 * A clone of the interface documented in "man 3 malloc".
 */

#pragma once

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

struct meta {
	struct meta *prev;
	struct meta *next;
	bool free;
	size_t size;
	char data[0];
};

void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
void mm_free(void *ptr);

struct meta *create_space(struct meta *prev, size_t size);
void split_block(struct meta *node, size_t size, size_t res_size);
struct meta *get_block(void *ptr);