#pragma once

#include "aex/io.h"

#include "proc/proc.h"

struct dev_char {
    struct dev_file_ops* ops;
    int internal_id;

    thread_t* worker;
    mutex_t   access;
};
typedef struct dev_char dev_char_t;

int  dev_register_char(char* name, struct dev_char* dev_char);
void dev_unregister_char(char* name);

int  dev_char_open(int id);
int  dev_char_read(int id, uint8_t* buffer, int len);
int  dev_char_write(int id, uint8_t* buffer, int len);
int  dev_char_close(int id);
long dev_char_ioctl(int id, long code, void* mem);