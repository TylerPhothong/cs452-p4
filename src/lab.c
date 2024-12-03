#include "lab.h"
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

size_t btok(size_t bytes) {
    size_t k = 0;
    while ((UINT64_C(1) << k) < bytes) {
        k++;
    }
    return k;
}

struct avail *buddy_calc(struct buddy_pool *pool, struct avail *buddy) {
    size_t offset = (uintptr_t)buddy - (uintptr_t)pool->base;
    size_t buddy_offset = offset ^ (UINT64_C(1) << buddy->kval);
    return (struct avail *)((uintptr_t)pool->base + buddy_offset);
}

void *buddy_malloc(struct buddy_pool *pool, size_t size) {
    if (!pool || size == 0) return NULL;

    size_t k = btok(size + sizeof(struct avail));
    if (k < SMALLEST_K) k = SMALLEST_K;

    for (size_t i = k; i <= pool->kval_m; i++) {
        if (pool->avail[i].next != &pool->avail[i]) {
            struct avail *block = pool->avail[i].next;
            block->tag = BLOCK_RESERVED;

            // Split blocks until we reach the desired size
            while (block->kval > k) {
                block->kval--;
                struct avail *buddy = buddy_calc(pool, block);
                buddy->kval = block->kval;
                buddy->tag = BLOCK_AVAIL;
                buddy->next = pool->avail[block->kval].next;
                buddy->prev = &pool->avail[block->kval];
                pool->avail[block->kval].next->prev = buddy;
                pool->avail[block->kval].next = buddy;
            }

            // Remove block from avail list
            block->next->prev = block->prev;
            block->prev->next = block->next;

            return (void *)(block + 1);
        }
    }

    errno = ENOMEM;
    return NULL;
}

void buddy_free(struct buddy_pool *pool, void *ptr) {
    if (!pool || !ptr) return; // Ignore NULL pointers or NULL pool

    struct avail *block = (struct avail *)ptr - 1;

    // Check if the block is within the memory pool bounds
    if ((uintptr_t)block < (uintptr_t)pool->base || 
        (uintptr_t)block >= (uintptr_t)pool->base + pool->numbytes) {
        fprintf(stderr, "Error: Attempted to free an invalid pointer %p\n", ptr);
        return;
    }

    // Check if the block was already free or is invalid
    if (block->tag != BLOCK_RESERVED) {
        fprintf(stderr, "Error: Double free or invalid free detected for pointer %p\n", ptr);
        return;
    }

    block->tag = BLOCK_AVAIL;

    // Merge buddy blocks
    while (block->kval < pool->kval_m) {
        struct avail *buddy = buddy_calc(pool, block);
        if (buddy->tag != BLOCK_AVAIL || buddy->kval != block->kval) break;

        // Remove buddy from avail list
        buddy->next->prev = buddy->prev;
        buddy->prev->next = buddy->next;

        // Merge with buddy
        if (buddy < block) block = buddy;
        block->kval++;
    }

    // Add the merged block back to the avail list
    block->next = pool->avail[block->kval].next;
    block->prev = &pool->avail[block->kval];
    pool->avail[block->kval].next->prev = block;
    pool->avail[block->kval].next = block;
}


void *buddy_realloc(struct buddy_pool *pool, void *ptr, size_t size) {
    if (!ptr) return buddy_malloc(pool, size);
    if (size == 0) {
        buddy_free(pool, ptr);
        return NULL;
    }

    struct avail *block = (struct avail *)ptr - 1;
    size_t current_size = UINT64_C(1) << block->kval;
    if (current_size >= size) return ptr;

    void *new_ptr = buddy_malloc(pool, size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, current_size - sizeof(struct avail));
        buddy_free(pool, ptr);
    }
    return new_ptr;
}

void buddy_init(struct buddy_pool *pool, size_t size) {
    if (!pool) return;
    if (size == 0) size = UINT64_C(1) << DEFAULT_K;

    pool->kval_m = btok(size);
    pool->numbytes = UINT64_C(1) << pool->kval_m;
    pool->base = mmap(NULL, pool->numbytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (pool->base == MAP_FAILED) {
        perror("mmap failed");
        return;
    }

    for (size_t i = 0; i <= pool->kval_m; i++) {
        pool->avail[i].next = &pool->avail[i];
        pool->avail[i].prev = &pool->avail[i];
        pool->avail[i].tag = BLOCK_UNUSED;
        pool->avail[i].kval = i;
    }

    struct avail *base_block = pool->base;
    base_block->kval = pool->kval_m;
    base_block->tag = BLOCK_AVAIL;
    base_block->next = &pool->avail[pool->kval_m];
    base_block->prev = &pool->avail[pool->kval_m];

    pool->avail[pool->kval_m].next = base_block;
    pool->avail[pool->kval_m].prev = base_block;
}

void buddy_destroy(struct buddy_pool *pool) {
    if (!pool) return;
    if (munmap(pool->base, pool->numbytes) == -1) {
        perror("munmap failed");
    }
}
