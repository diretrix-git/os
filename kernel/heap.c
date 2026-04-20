#include "heap.h"
#include "pmm.h"

/*
 * Very simple first-fit free-list allocator.
 * Each allocation is prefixed by a header.
 * Pages are grabbed from the PMM on demand.
 */

#define HEAP_MAGIC  0xDEADBEEF
#define HEAP_START  0x200000   /* 2 MB — above kernel */
#define HEAP_MAX    0x800000   /* grow up to 8 MB */

typedef struct block_header {
    uint32_t magic;
    uint32_t size;      /* payload size (not including header) */
    uint8_t  free;
    struct block_header *next;
} block_header_t;

static block_header_t *heap_head = 0;
static uint32_t heap_top = HEAP_START; /* next unmapped byte */

static void heap_expand(uint32_t bytes)
{
    /* Round up to page boundary */
    uint32_t pages = (bytes + 4095) / 4096;
    for (uint32_t i = 0; i < pages && heap_top < HEAP_MAX; i++) {
        heap_top += 4096;
    }
}

void heap_init(void)
{
    /* Pre-map 64 KB to start */
    heap_expand(65536);
    heap_head = (block_header_t *)HEAP_START;
    heap_head->magic = HEAP_MAGIC;
    heap_head->size  = heap_top - HEAP_START - sizeof(block_header_t);
    heap_head->free  = 1;
    heap_head->next  = 0;
}

void *kmalloc(uint32_t size)
{
    if (size == 0) return 0;
    /* Align to 8 bytes */
    size = (size + 7) & ~7u;

    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            /* Split block if enough room */
            if (cur->size >= size + sizeof(block_header_t) + 8) {
                block_header_t *split = (block_header_t *)((uint8_t *)cur + sizeof(block_header_t) + size);
                split->magic = HEAP_MAGIC;
                split->size  = cur->size - size - sizeof(block_header_t);
                split->free  = 1;
                split->next  = cur->next;
                cur->next    = split;
                cur->size    = size;
            }
            cur->free = 0;
            return (void *)((uint8_t *)cur + sizeof(block_header_t));
        }
        cur = cur->next;
    }

    /* Need more space */
    heap_expand(size + sizeof(block_header_t));
    /* Extend last block or add new one — simple: try again with a fresh tail */
    block_header_t *tail = heap_head;
    while (tail->next) tail = tail->next;
    if (tail->free) {
        tail->size = heap_top - (uint32_t)((uint8_t *)tail + sizeof(block_header_t));
        return kmalloc(size); /* retry */
    }
    /* Append new block */
    block_header_t *nb = (block_header_t *)((uint8_t *)tail + sizeof(block_header_t) + tail->size);
    nb->magic = HEAP_MAGIC;
    nb->size  = heap_top - (uint32_t)((uint8_t *)nb + sizeof(block_header_t));
    nb->free  = 1;
    nb->next  = 0;
    tail->next = nb;
    return kmalloc(size);
}

void kfree(void *ptr)
{
    if (!ptr) return;
    block_header_t *hdr = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    if (hdr->magic != HEAP_MAGIC) return;
    hdr->free = 1;
    /* Coalesce forward */
    while (hdr->next && hdr->next->free) {
        hdr->size += sizeof(block_header_t) + hdr->next->size;
        hdr->next  = hdr->next->next;
    }
}

uint32_t heap_used(void)
{
    uint32_t used = 0;
    block_header_t *cur = heap_head;
    while (cur) {
        if (!cur->free) used += cur->size;
        cur = cur->next;
    }
    return used;
}