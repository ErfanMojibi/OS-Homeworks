/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines. Remove this comment and provide
 * a summary of your allocator's design here.
 */

#include "mm_alloc.h"

#include <stdlib.h>

/* Your final implementation should comment out this macro. */
#define MM_USE_STUBS

s_block_ptr head = NULL;

s_block_ptr find_free_block(size_t size)
{
    if (head == NULL)
        return NULL;
    s_block_ptr current = head;
    while(current != NULL && current->size < size)
        current = current->next;
    
    return current;

}
/* Split block according to size, b must exist */
void split_block(s_block_ptr b, size_t s);

/* Try fusing block with neighbors */
s_block_ptr fusion(s_block_ptr b);

/* Get the block from addr */
s_block_ptr get_block(void *p);

/* Add a new block at the of heap,
 * return NULL if things go wrong
 */
s_block_ptr extend_heap(size_t s){
    s_block_ptr new_block = (s_block_ptr) sbrk(0);

    sbrk(sizeof(struct s_block) + s);
    new_block->size = s;
    new_block->is_free = 0;
    new_block->next = NULL;
    

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

    memset((void *)new_block + sizeof(struct s_block), 0, new_block->size);
    return (void*)new_block + sizeof(struct s_block);
}

void *mm_malloc(size_t size)
{
#ifdef MM_USE_STUBS
    return calloc(1, size);
#else
#error Not implemented.
#endif
    s_block_ptr ptr = find_free_block(size);
    if(ptr == NULL){
        return extend_heap(size);
    } else {
        
    }

}

void *mm_realloc(void *ptr, size_t size)
{
#ifdef MM_USE_STUBS
    return realloc(ptr, size);
#else
#error Not implemented.
#endif
}

void mm_free(void *ptr)
{
#ifdef MM_USE_STUBS
    free(ptr);
#else
#error Not implemented.
#endif
}
