/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines. Remove this comment and provide
 * a summary of your allocator's design here.
 */

#include "mm_alloc.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include<string.h>
/* Your final implementation should comment out this macro. */
//#define MM_USE_STUBS


s_block_ptr head = NULL;

s_block_ptr find_free_block(size_t size)
{
    if (head == NULL)
        return NULL;
    for(s_block_ptr ptr = head; ptr != NULL; ptr = ptr->next){
        if(ptr->is_free && ptr->size >= size){
            return ptr;
        }
    }
    
    return NULL;

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
    
    new_block->ptr = ((void*)new_block + BLOCK_SIZE);
    
    b->next = new_block;
    if(next != NULL)
        next->prev = new_block;
        
    b->size = s;
    memset(new_block->ptr, 0, new_block->size);
    return;
}

/* Try fusing block with neighbors */
s_block_ptr fusion(s_block_ptr b){
    s_block_ptr prev = b->prev;
    s_block_ptr next = b->next;

    if(prev != NULL && prev->is_free){
        prev->size = prev->size + BLOCK_SIZE + b->size;
        prev->next = b->next;
        b = prev;
        if(next != NULL)
            next->prev = b;
    }

    if(next != NULL && next->is_free){
        b->size = b->size + BLOCK_SIZE + next->size;
        b->next = next->next;
        if(next->next != NULL)
            next->prev = b;
    }

    b->is_free = 1;
    return b;
}

/* Get the block from addr */
s_block_ptr get_block(void *p){
    s_block_ptr block = (s_block_ptr)(p - (void*) BLOCK_SIZE);
    for(s_block_ptr item = head; item != NULL; item = item->next)
        if(item == block)
            return block;
    return NULL;
}

/* Add a new block at the of heap,
 * return NULL if things go wrong
 */
void* extend_heap(size_t s){
    void* p = sbrk(s+BLOCK_SIZE);
    if(p == (void*) -1)
        return NULL;
    s_block_ptr new_block = (s_block_ptr) p; 
    new_block->size = s;
    new_block->is_free = 0;
    new_block->next = NULL;
    new_block->ptr = ((void*)new_block + BLOCK_SIZE);

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
    return;
#else
//#error Not implemented.
    if(size == 0)
        return NULL;

    s_block_ptr ptr = find_free_block(size);
    if (ptr == NULL){
        return extend_heap(size);
    } else {
        split_block(ptr, size);
    }
    ptr->is_free = 0;
    memset((void*)ptr + BLOCK_SIZE, 0, ptr->size);
    return (void*)ptr + BLOCK_SIZE;
#endif
    
}

void *mm_realloc(void *ptr, size_t size)
{
#ifdef MM_USE_STUBS
    return;
#else
//#error Not implemented.
    s_block_ptr ptr_block = get_block(ptr);
    if(ptr_block == NULL)
        return NULL;

    void* new_mem = mm_malloc(size);
    if(new_mem){
        if(size < ptr_block->size){
            memcpy(new_mem, ptr, size);
            mm_free(ptr);
            return new_mem;
        } else {
            memcpy(new_mem, ptr, ptr_block->size);
            mm_free(ptr);
            return new_mem;
        }
    }
    return NULL;
#endif
}

void mm_free(void *ptr)
{
#ifdef MM_USE_STUBS
    return;
#else
    if(ptr == NULL)
        return;

    s_block_ptr block = get_block(ptr);
    if(block == NULL)
        return NULL;

    block->is_free = 1;
    fusion(block);
#endif
}
