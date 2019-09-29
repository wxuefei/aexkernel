#pragma once

#include "frame.h"
#include "page.h"

#define DEFAULT_POOL_SIZE 0x1000 * 256

typedef struct mem_pool {
    //uint32_t frame_start;
    uint32_t frame_amount;
    
    struct mem_pool* next;

    uint32_t size;
    uint32_t free;

    struct mem_block* first_block;
} mem_pool;

typedef struct mem_block {
    size_t size;
    char free : 1;

    struct mem_pool* parent;
    struct mem_block* next;
} mem_block;

mem_pool* mem_pool0;
//size_t isr_stack;

mem_pool* mempo_create(uint32_t size) {

    uint32_t real_size = size + sizeof(mem_pool) + sizeof(mem_block);
    uint32_t needed_frames = mempg_to_pages(real_size);

    void* ptr = mempg_nextc(needed_frames, NULL, NULL, 0x03);

    //write_debug("Pool ptr: 0x%s", (size_t)ptr, 16);

    mem_pool* pool = (mem_pool*) ptr;
    mem_block* block = (mem_block*) (((size_t)ptr) + sizeof(mem_pool));

    block->size = size;
    block->free = true;
    block->parent = pool;
    block->next = NULL;

    //pool->frame_start = start_id;
    pool->frame_amount = needed_frames;
    pool->next = NULL;
    pool->size = size + sizeof(mem_block);
    pool->free = size;
    pool->first_block = block;

    return pool;
}
void* mempo_alloc(uint32_t size) {

    mem_pool* pool = mem_pool0;
    mem_block* block = mem_pool0->first_block;

    uint32_t real_size = size + sizeof(mem_block);

    while (true) {

        if ((!(block->free) && block->next == NULL) || block == NULL) {
            //printf("Reached end\n");

            if (pool->next == NULL)
                pool->next = mempo_create(size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE);

            pool = pool->next;
            block = pool->first_block;

            continue;
        }
        if (!(block->free)) {
            //printf("Block not free\n");
            
            block = block->next;
            continue;
        }

        if (block->size == size) {
            //printf("Perfect fit\n");

            block->free = false;
            pool->free -= size;

            break;
        }

        if (pool->free < real_size) {
            //printf("Pool cannot fit me at all\n");

            if (pool->next == NULL)
                pool->next = mempo_create(size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE);

            pool = pool->next;
            block = pool->first_block;
            
            continue;
        }

        if (block->size < real_size) {
            //printf("Block cannot fit me\n");

            block = block->next;
            continue;
        }
        
        //printf("I had to split a block\n");
        pool->free -= real_size;

	    //char stringbuffer[32];

        mem_block* new_block = (mem_block*)(((size_t)block) + real_size);

        //printf("Real Size: %s\n", itoa(real_size, stringbuffer, 10));

        new_block->size = block->size - real_size;
        new_block->free = true;
        new_block->parent = block->parent;
        new_block->next = block->next;
        
        block->size = size;
        block->free = false;
        block->next = new_block;

        break;
    }
    return (void*)(((size_t)block) + sizeof(mem_block));
}

void mempo_enum(mem_pool* pool) {

    mem_block* block = pool->first_block;

    while (block != NULL) {
        printf("Addr: %x ", (size_t)block & 0xFFFFFFFFFFFF);
        printf("Size: %i ", block->size);
        printf(block->free ? "Free" : "Busy");
        printf("\n");

        block = block->next;
    }
}

void mempo_cleanup(mem_pool* pool) {

    mem_block* block = pool->first_block;
    mem_block* next_block;

    while (block != NULL) {

        next_block = block->next;

        if (!(block->free)) {
            block = next_block;
            continue;
        }

        if (next_block == NULL)
            break;

        if (next_block->free) {
            block->size += sizeof(mem_block) + next_block->size;
            block->next = next_block->next;

            pool->size += sizeof(mem_block);

            continue;
        }
        block = next_block;
    }
}

void mempo_unalloc(void* space) {
    mem_block* block = (mem_block*)(((size_t)space) - sizeof(mem_block));
    block->free = true;

    block->parent->free += block->size;

    mempo_cleanup(block->parent);
}

char mempo_buffer[32];

void mempo_init() {

	mem_pool0 = mempo_create(DEFAULT_POOL_SIZE);
	printf("Created initial 1 MB kernel memory pool\n\n");

    //isr_stack = (size_t)mempo_alloc(8192);
	//printf("bong 0x%s\n", itoa(isr_stack, mempo_buffer, 16));
}