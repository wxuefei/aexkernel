#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/spinlock.h"

#include "aex/sys/cpu.h"

#include "boot/multiboot.h"

#include "frame.h"

#define FRAMES_PER_INT 32

struct memfr_alloc_piece;
typedef struct memfr_alloc_piece memfr_alloc_piece_t;

// Memory stuff
uint64_t frame_current = 0;
uint64_t frame_last    = 0;
uint64_t frames_possible = 0;

uint64_t frames_used = 0;

spinlock_t fr_spinlock = {
    .val = 0,
    .name = "frame op",
};

memfr_alloc_piece_root_t memfr_alloc_piece0;

phys_addr memfr_alloc_internal(uint32_t id) {
    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;
    uint32_t fr = id;

    while (true) {
        if (id < piece->usable) {
            if (!(piece->bitmap[id / FRAMES_PER_INT] & (1UL << (id % FRAMES_PER_INT))))
                ++frames_used;
            else
                printk("False alloc of frame %i\n", fr);

            piece->bitmap[id / FRAMES_PER_INT] |= 1UL << (id % FRAMES_PER_INT);
            return (phys_addr) piece->start + id * CPU_PAGE_SIZE;
        }
        id -= piece->usable;
        piece = piece->next;

        if (piece == NULL || id > frames_possible)
            kpanic("Failed to allocate a memory frame");
    }
    return 0x0000;
}

phys_addr kfalloc(uint32_t id) {
    spinlock_acquire(&fr_spinlock);
    phys_addr ret = memfr_alloc_internal(id);

    spinlock_release(&fr_spinlock);
    return ret;
}

bool kffree(uint32_t id) {
    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;
    uint32_t fr = id;

    spinlock_acquire(&fr_spinlock);

    while (true) {
        if (id < piece->usable) {
            if (piece->bitmap[id / FRAMES_PER_INT] & (1UL << (id % FRAMES_PER_INT)))
                --frames_used;
            else
                printk("False unalloc of frame %i\n", fr);

            piece->bitmap[id / FRAMES_PER_INT] &= ~(1UL << (id % FRAMES_PER_INT));
            spinlock_release(&fr_spinlock);
            
            return true;
        }
        id -= piece->usable;
        piece = piece->next;

        if (piece == NULL || id > frames_possible)
            kpanic("Failed to unallocate a memory frame");
    }
    spinlock_release(&fr_spinlock);
    return false;
}

uint64_t kfamount() {
    return frames_possible;
}

uint64_t kfused() {
    return frames_used;
}

bool kfisfree(uint32_t id) {
    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;

    while (true) {
        if (id < piece->usable)
            return (piece->bitmap[id / FRAMES_PER_INT] & (1UL << (id % FRAMES_PER_INT))) == 0;

        id -= piece->usable;
        piece = piece->next;

        if (piece == NULL || id > frames_possible)
            kpanic("Failed to unallocate a memory frame");
    }
    return false;
}

phys_addr kfpaddrof(uint32_t id) {
    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;

    while (true) {
        if (id < piece->usable)
            return (phys_addr) piece->start + id * CPU_PAGE_SIZE;
        
        id -= piece->usable;
        piece = piece->next;

        if (piece == NULL || id > frames_possible)
            kpanic("Failed to get pointer to a memory frame");
    }
    return 0x0000;
}

uint32_t kfindexof(phys_addr addr) {
    memfr_alloc_piece_t* piece  = (memfr_alloc_piece_t*) &memfr_alloc_piece0;
    memfr_alloc_piece_t* parent = NULL;

    while (piece != NULL) {
        if (piece->start <= addr && (piece->start + (piece->usable * MEM_FRAME_SIZE)) > addr) {
            parent = piece;
            break;
        }
        piece = piece->next;
    }

    if (parent == NULL) {
        printk("addr: 0x%lx\n", addr);
        kpanic("kfindexof() fail");
    }

    return (addr - piece->start) / MEM_FRAME_SIZE;
}

// optimize this later on cuz now am too lazy
uint32_t kfcalloc(uint32_t amount) {
    //printk("kfcalloc(%i)\n", amount);
    if (amount == 0)
        return 0;

    spinlock_acquire(&fr_spinlock);

    uint32_t start_id = 0xFFFFFFFF;
    uint32_t combo = 0;

    for (uint32_t i = 0; i < frames_possible; i++) {
        if (start_id == 0xFFFFFFFF && kfisfree(i))
            start_id = i;

        if (!kfisfree(i)) {
            start_id = 0xFFFFFFFF;
            combo = 0;

            continue;
        }

        if (++combo == amount) {
            for (i = 0; i < amount; i++)
                memfr_alloc_internal(start_id + i);

            //printk("start: %i\n", start_id);

            spinlock_release(&fr_spinlock);
            return start_id;
        }
    }
    spinlock_release(&fr_spinlock);
    kpanic("Failed to allocate contiguous frames");

    return 0;
}