#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

/* Simple kernel heap — bump allocator with free list */
void  heap_init(void);
void *kmalloc(uint32_t size);
void  kfree(void *ptr);
uint32_t heap_used(void);

#endif