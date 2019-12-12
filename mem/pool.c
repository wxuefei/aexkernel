#include "frame.h"
#include "page.h"

#include "aex/time.h"
#include "aex/mutex.h"

#include <stdio.h>
#include <string.h>

#include "aex/mem.h"
#include "pool.h"

#define DEFAULT_POOL_SIZE 0x1000 * 256

struct mem_pool {
    //uint32_t frame_start;
    uint32_t frame_amount;

    struct mem_pool* next;
    mutex_t mutex;

    uint32_t size;
    uint32_t free;

    struct mem_block* first_block;
};
typedef struct mem_pool mem_pool_t;

struct mem_block {
    uint32_t size;
    char free : 1;
    uint32_t idk : 15;

    struct mem_pool* parent;
    struct mem_block* next;
} __attribute((packed));
typedef struct mem_block mem_block_t;

mem_pool_t* mem_pool0;

void mempo_cleanup(mem_pool_t* pool);

mem_pool_t* mempo_create(uint32_t size) {
    size += (size % 2);

    uint32_t real_size     = size + sizeof(mem_pool_t) + sizeof(mem_block_t);
    uint32_t needed_frames = kptopg(real_size);
    void* ptr = kpalloc(needed_frames, NULL, 0x03);

    mem_pool_t* pool = (mem_pool_t*) ptr;
    mem_block_t* block = (mem_block_t*) (((size_t) ptr) + sizeof(mem_pool_t));

    block->size = size;
    block->free = true;
    block->parent = pool;
    block->next   = NULL;

    pool->frame_amount = needed_frames;
    pool->next = NULL;
    pool->size = size + sizeof(mem_block_t);
    pool->free = size;
    pool->first_block = block;
    pool->mutex = 0;

    return pool;
}

void mempo_enum(mem_pool_t* pool) {
    mem_block_t* block = pool->first_block;

    while (block != NULL) {
        printf("Addr: %x Size: %i ", (size_t) block & 0xFFFFFFFFFFFF, block->size);
        printf(block->free ? "Free\n" : "Busy\n");

        block = block->next;
    }
}

void mempo_enum_root() {
    mem_pool_t* pool   = mem_pool0;
    mem_block_t* block = pool->first_block;

    while (block != NULL) {
        printf("Addr: %x Size: %i ", (size_t) block & 0xFFFFFFFFFFFF, block->size);
        printf(block->free ? "Free" : "Busy");
        printf(", Next: %s\n", block->next != NULL ? "Next" : "Last");

        sleep(100);

        block = block->next;
        if (block == NULL) {
            pool = pool->next;
            if (pool == NULL)
                return;

            block = pool->first_block;
            printf("Next\n");
        }
    }
}

void mempo_cleanup(mem_pool_t* pool) {
    mem_block_t* block = pool->first_block;
    mem_block_t* next_block;

    while (block != NULL) {
        next_block = block->next;

        if (!(block->free)) {
            block = next_block;
            continue;
        }
        if (next_block == NULL)
            break;

        if (next_block->free) {
            block->size += sizeof(mem_block_t) + next_block->size;
            block->next = next_block->next;

            pool->size += sizeof(mem_block_t);
            continue;
        }
        block = next_block;
    }
}

void mempo_init() {
	mem_pool0 = mempo_create(DEFAULT_POOL_SIZE);
	printf("Created initial %i KB kernel memory pool\n\n", DEFAULT_POOL_SIZE / 1000);
}

void* kmalloc(uint32_t size) {
    mem_pool_t* pool = mem_pool0;
    mem_block_t* block = mem_pool0->first_block;

    size += (size % 2);

    uint32_t real_size = size + sizeof(mem_block_t);

    while (true) {
        if (block == NULL) {
            if (pool->next == NULL) {
                mempo_enum_root();

                pool->next = mempo_create(size > DEFAULT_POOL_SIZE ? size * 2 : DEFAULT_POOL_SIZE);
                printf("                new pool %i\n", pool->free);
                for (volatile uint64_t i = 0; i < 333232333; i++) ;
            }
            pool  = pool->next;
            block = pool->first_block;
            continue;
        }
        if (!(block->free)) {
            block = block->next;
            continue;
        }
        if (block->size == size) {
            block->free = false;
            pool->free -= size;
            break;
        }
        if (pool->free < real_size) {
            if (pool->next == NULL)
                pool->next = mempo_create(size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE);

            pool  = pool->next;
            block = pool->first_block;
            continue;
        }
        if (block->size < real_size) {
            block = block->next;
            continue;
        }
        pool->free -= real_size;

        mem_block_t* new_block = (mem_block_t*)(((size_t) block) + real_size);

        new_block->size   = block->size - real_size;
        new_block->free   = true;
        new_block->parent = block->parent;
        new_block->next   = block->next;

        block->size = size;
        block->free = false;
        block->next = new_block;

        break;
    }
    //printf("alloced size: %i\n", size);
    //for (volatile uint64_t i = 0; i < 33232333; i++) ;

    return (void*) (((size_t) block) + sizeof(mem_block_t));
}

void* krealloc(void* space, uint32_t size) {
    if (space == NULL) {
        if (size == 0)
            return NULL;

        return kmalloc(size);
    }

    mem_block_t* block = (mem_block_t*)(((size_t) space) - sizeof(mem_block_t));
    if (block->free)
        return NULL;

    if (size == 0) {
        kfree(space);
        return NULL;
    }
    uint64_t oldsize = block->size;

    void* new = kmalloc(size);

    memcpy(new, space, oldsize);
    kfree(space);

    return new;
}

void kfree(void* space) {
    mem_block_t* block = (mem_block_t*)(((size_t) space) - sizeof(mem_block_t));
    block->free = true;

    block->parent->free += block->size + sizeof(mem_block_t);

    mutex_acquire(&(block->parent->mutex));
    mempo_cleanup(block->parent);
    mutex_release(&(block->parent->mutex));
}