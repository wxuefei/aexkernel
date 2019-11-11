#pragma once

#include "aex/mutex.h"

#include "proc/proc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum blk_rq_type {
    BLK_INIT    = 0,
    BLK_READ    = 1,
    BLK_WRITE   = 2,
    BLK_RELEASE = 3,
    BLK_IOCTL   = 4,
};
enum block_flags {
    DISK_PARTITIONABLE = 0x0001,
};
enum block_type {
    DISK_TYPE_DISK    = 0x01,
    DISK_TYPE_OPTICAL = 0x02,
};

struct blk_request {
    uint8_t type;

    uint64_t sector;
    uint16_t count;

    uint8_t* buffer;
    long     response;
    thread_t* thread;

    volatile bool done;

    struct blk_request* next;
} __attribute((packed));
typedef struct blk_request blk_request_t;

struct dev_block {
    int internal_id;
    bool initialized;

    char model_name[64];

    uint8_t type;

    uint16_t flags;
    uint16_t max_sectors_at_once;

    uint32_t total_sectors;
    uint32_t sector_size;

    thread_t* worker;
    mutex_t   access;
    blk_request_t* io_queue;
    blk_request_t* last_brq;

    struct dev_block_ops* block_ops;

    struct dev_block* proxy_to;
    uint64_t proxy_offset;
};
typedef struct dev_block dev_block_t;

struct dev_block_ops {
    int (*init) (int drive);
    int (*read) (int drive, uint64_t sector, uint16_t count, uint8_t* buffer);
    int (*write)(int drive, uint64_t sector, uint16_t count, uint8_t* buffer);
    int (*release)(int drive);

    long (*ioctl)(int drive, long, long);
};

int  dev_register_block(char* name, struct dev_block* block_dev);
void dev_unregister_block(char* name);

int  dev_block_init(int dev_id);
int  dev_block_read(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
int  dev_block_dread(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
int  dev_block_write(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
int  dev_block_release(int dev_id);

int  dev_block_set_proxy(struct dev_block* block_dev, struct dev_block* proxy_to);
bool dev_block_is_proxy(int dev_id);
dev_block_t* dev_block_get_data(int dev_id);