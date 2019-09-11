#pragma once

#include "proc/proc.c"

struct process;
struct thread;

struct process* process_current;
struct klist* process_klist;

void proc_init();

struct thread* thread_create(struct process* process, void* entry);
void thread_kill(struct thread* thread);

size_t process_create(char* name, char* image_path, size_t paging_dir);
struct process* process_get(size_t pid);
void process_kill(size_t pid);