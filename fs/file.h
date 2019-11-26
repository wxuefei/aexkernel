#pragma once

struct file;
typedef struct file file_t;

struct finfo;
typedef struct finfo finfo_t;

#include "fs/inode.h"
#include "fs/pipe.h"

enum {
    FILE_TYPE_NORMAL = 0,
    FILE_TYPE_PIPE   = 1,
};
enum {
    FILE_FLAG_READ  = 0x0001,
    FILE_FLAG_WRITE = 0x0002,
};

struct file {
    uint64_t position;
    uint8_t  type;
    
    uint16_t flags;
    uint32_t ref_count;

    void* private_data;

    union {
        inode_t* inode;
        pipe_t*  pipe;
    };
};
typedef struct file file_t;

struct finfo {
    uint64_t inode;
    uint8_t  type;
};
typedef struct finfo finfo_t;