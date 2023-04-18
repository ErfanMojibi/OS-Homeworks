/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines. Remove this comment and provide
 * a summary of your allocator's design here.
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <stdio.h>
/* Your final implementation should comment out this macro. */
//#define MM_USE_STUBS


s_block_ptr head = NULL;

s_block_ptr find_free_block(size_t size)
{
    if (head == NULL)
        return NULL;
    s_block_ptr current = head;
    while(current != NULL && current->size <= size)
        current = current->next;
    
    return current;

}
/* Split block according to size, b must exist */
void split_block(s_block_ptr b, size_t s){
    if (BLOCK_SIZE + s < b->size)
        return;

    s_block_ptr new_block = (s_block_ptr) ((void*)b->ptr + s);
    s_block_ptr next = b->next;
    
    // set new block meta data
    new_block->is_free = 1;
    new_block->size = b->size - s - BLOCK_SIZE;
    new_block->next = next;
    new_block->prev = b;
    new_block->ptr = (void*)(new_block + BLOCK_SIZE);
    
    b->next = new_block;
    return;
}

/* Try fusing block with neighbors */
s_block_ptr fusion(s_block_ptr b){
    s_block_ptr prev = b->prev;
    s_block_ptr next = b->next;

    if(prev != NULL && prev->is_free){
        prev->size += BLOCK_SIZE + b->size;
        prev->next = b->next;
        b = prev;
    }

    if(next != NULL && next->is_free){
        b->size += BLOCK_SIZE + next->size;
        b->next = next->next;
    }

    b->is_free = 1;
    memset(b->ptr, 0, b->size);
}

/* Get the block from addr */
s_block_ptr get_block(void *p){
    return (s_block_ptr)(p - (void*) BLOCK_SIZE);
}

/* Add a new block at the of heap,
 * return NULL if things go wrong
 */
void* extend_heap(size_t s){
    void* p = sbrk(BLOCK_SIZE + s);
    if(p == (void*) -1)
        return NULL;

    s_block_ptr new_block = (s_block_ptr) p; 
    new_block->size = s;
    new_block->is_free = 0;
    new_block->next = NULL;
    new_block->ptr = (void*) ((void*)new_block + BLOCK_SIZE);

    /* find last element in linked list and append new mmeory to list*/
    s_block_ptr last = head;
    if(last == NULL){
        head = new_block;
        new_block->prev = NULL;
    } else {
        while(last->next != NULL)
            last = last->next;
        last->next = new_block;
        new_block->prev = last;
    }

    memset((void *)new_block + BLOCK_SIZE, 0, new_block->size);
    return (void*)new_block + BLOCK_SIZE;
}

void *mm_malloc(size_t size)
{
#ifdef MM_USE_STUBS
    return calloc(1, size);
#else
//#error Not implemented.
    
    s_block_ptr ptr = find_free_block(size);
    if (ptr == NULL){
        return extend_heap(size);
    } else {
        split_block(ptr, size);
    }
    return ptr;
#endif
    
}

void *mm_realloc(void *ptr, size_t size)
{
#ifdef MM_USE_STUBS
    return realloc(ptr, size);
#else
//#error Not implemented.
#endif
}

void mm_free(void *ptr)
{
#ifdef MM_USE_STUBS
    free(ptr);
#else
    if(ptr == NULL)
        return;
    s_block_ptr block = get_block(ptr);
    block->is_free = 1;
    fusion(block);
#endif
}
