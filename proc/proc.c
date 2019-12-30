#include "aex/cbuf.h"
#include "aex/hook.h"
#include "aex/io.h"
#include "aex/kernel.h"
#include "aex/klist.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/string.h"
#include "aex/syscall.h"
#include "aex/time.h"

#include "aex/dev/cpu.h"

#include "aex/proc/task.h"
#include "aex/proc/exec.h"

#include "proc/elfload.h"

#include "aex/fs/fs.h"
#include "aex/fs/file.h"

#include "mem/frame.h"
#include "mem/page.h"
#include "mem/pagetrk.h"

#include <stddef.h>

#include "kernel/init.h"
#include "aex/proc/proc.h"

struct process;
struct thread;

struct klist process_klist;
size_t process_counter = 1;

cbuf_t thread_cleanup_queue;

struct thread* thread_create(process_t* process, void* entry, bool kernelmode) {
    struct thread* new_thread = kmalloc(sizeof(struct thread));
    memset(new_thread, 0, sizeof(struct thread));

    new_thread->task = task_create(process, kernelmode, entry, (size_t) process->ptracker->root);
    new_thread->task->thread = new_thread;

    new_thread->id = process->thread_counter++;
    new_thread->process = process;

    new_thread->task->process = process;

    klist_set(&(process->threads), new_thread->id, new_thread);

    return new_thread;
}

void thread_start(struct thread* thread) {
    task_insert(thread->task);
}

void _thread_kill(struct thread* thread) {
    process_t* process = thread->process;
    mutex_acquire_yield(&(process->access));

    bool inton = checkinterrupts();
    if (inton)
        nointerrupts();

    thread->task->status = TASK_STATUS_DEAD;

    if (thread->added_busy) {
        thread->process->busy--;
        thread->added_busy = false;
    }
    task_remove(thread->task);
    task_dispose(thread->task);

    klist_set(&(process->threads), thread->id, NULL);

    if (thread->name != NULL)
        kfree(thread->name);

    kfree(thread);
    mutex_release(&(process->access));

    if (process->threads.count == 0)
        process_kill(process->pid);

    if (inton)
        interrupts();
}

bool thread_kill(struct thread* thread) {
    if (klist_get(&(thread->process->threads), thread->id) == NULL)
        return false;

    size_t pid = thread->process->pid;
    size_t id  = thread->id;

    if (thread == task_current->thread)
        cbuf_write(&thread_cleanup_queue, (uint8_t*) &thread, sizeof(thread_t*));
    else
        _thread_kill(thread);

    while (thread_exists(pid, id))
        yield();
    
    return true;
}

bool thread_kill_preserve_process_noint(struct thread* thread) {
    if (klist_get(&(thread->process->threads), thread->id) == NULL)
        return false;

    thread->task->status = TASK_STATUS_DEAD;

    task_remove(thread->task);
    task_dispose(thread->task);

    klist_set(&(thread->process->threads), thread->id, NULL);
    kfree(thread);
    
    return true;
}

bool thread_pause(struct thread* thread) {
    if (klist_get(&(thread->process->threads), thread->id) == NULL)
        return false;

    thread->pause = true;
    if (!thread->added_busy)
        task_remove(thread->task);

    return true;
}

bool thread_resume(struct thread* thread) {
    if (klist_get(&(thread->process->threads), thread->id) == NULL)
        return false;

    task_insert(thread->task);
    return true;
}

bool thread_exists(size_t pid, size_t id) {
    process_t* process = process_get(pid);
    if (process == NULL)
        return false;

    if (klist_get(&(process->threads), id) == NULL)
        return false;

    return true;
}

size_t process_create(char* name, char* image_path, size_t paging_dir) {
    process_t* new_process = kmalloc(sizeof(struct process));
    memset(new_process, 0, sizeof(struct process));

    io_create_bqueue(&(new_process->wait_list));

    new_process->ptracker = kmalloc(sizeof(page_tracker_t));
    if (paging_dir != 0)
        mempg_init_tracker(new_process->ptracker, (void*) paging_dir);

    new_process->pid        = process_counter++;
    new_process->name       = kmalloc(strlen(name) + 2);
    new_process->image_path = kmalloc(strlen(image_path) + 2);
    new_process->parent_pid = process_current != NULL ? process_current->pid : 0;

    strcpy(new_process->name,       name);
    strcpy(new_process->image_path, image_path);

    klist_init(&(new_process->threads));
    klist_init(&(new_process->fiddies));

    new_process->thread_counter = 0;
    new_process->fiddie_counter = 4;

    klist_set(&process_klist, new_process->pid, new_process);

    return new_process->pid;
}

process_t* process_get(uint64_t pid) {
    return klist_get(&process_klist, pid);
}

void process_debug_list() {
    klist_entry_t* klist_entry = NULL;
    process_t* process = NULL;

    printk("Running processes:\n", klist_first(&process_klist));

    while (true) {
        process = klist_iter(&process_klist, &klist_entry);
        if (process == NULL)
            break;

        printk(PRINTK_BARE "[%i] %s\n", process->pid, process->name);
            
        klist_entry_t* klist_entry2 = NULL;
        struct thread* thread = NULL;
            
        while (true) {
            thread = klist_iter(&(process->threads), &klist_entry2);
            if (thread == NULL)
                break;

            if (thread->name == NULL)
                printk(PRINTK_BARE " - *anonymous*");
            else
                printk(PRINTK_BARE " - %s", thread->name);

            switch (thread->task->status) {
                case TASK_STATUS_RUNNABLE:
                    printk("; runnable");
                    break;
                case TASK_STATUS_SLEEPING:
                    printk("; sleeping");
                    break;
                case TASK_STATUS_BLOCKED:
                    printk("; blocked");
                    break;
                default:
                    printk("; unknown");
                    break;
            }
            printk("; prio: %i\n", thread->task->priority);
        }
    }
}

bool process_kill(uint64_t pid) {
    process_t* process = process_get(pid);
    if (process == NULL)
        return false;

    hook_proc_data_t pkill_data = {
        .pid = pid,
    };
    hook_invoke(HOOK_PKILL, &pkill_data);

    thread_t* thread;
    mutex_acquire_yield(&(process->access));
    if (process->busy > 0) {
        mutex_release(&(process->access));

        klist_entry_t* klist_entry = NULL;

        printk("th_pause %i, %i\n", process->busy, process->threads.count);
        while (true) {
            thread = (thread_t*) klist_iter(&(process->threads), &klist_entry);
            if (thread == NULL)
                break;

            thread_pause(thread);
        }
        printk("th_nx\n");
        while (true) {
            mutex_acquire_yield(&(process->access));
            if (process->busy > 0) {
                mutex_release(&(process->access));
                yield();

                continue;
            }
            break;
        }
        printk("th_done\n");
    }
    mutex_release(&(process->access));

    bool inton = checkinterrupts();
    if (inton)
        nointerrupts();

    kfree(process->name);
    kfree(process->image_path);
    kfree(process->working_dir);

    bool self = process->pid == process_current->pid;
    if (self)
        mempg_set_pagedir(NULL);

    page_frame_ptrs_t* ptrs;
    ptrs = &(process->ptracker->first);
    while (ptrs != NULL) {
        for (size_t i = 0; i < PG_FRAME_POINTERS_PER_PIECE; i++) {
            if (ptrs->pointers[i] == 0 || ptrs->pointers[i] == 0xFFFFFFFF)
                continue;

            kffree(ptrs->pointers[i]);
        }
        ptrs = ptrs->next;
    }

    ptrs = &(process->ptracker->dir_first);
    while (ptrs != NULL) {
        for (size_t i = 0; i < PG_FRAME_POINTERS_PER_PIECE; i++) {
            if (ptrs->pointers[i] == 0 || ptrs->pointers[i] == 0xFFFFFFFF)
                continue;

            kffree(ptrs->pointers[i]);
        }
        ptrs = ptrs->next;
    }

    // TODO: copy over the stack later on to prevent issues
    while (process->threads.count) {
        thread = klist_get(&(process->threads), klist_first(&process->threads));
        thread_kill_preserve_process_noint(thread);
    }
    mempg_dispose_user_root(process->ptracker->root_virt);
    io_defunct(&(process->wait_list));

    kfree(process->ptracker);
    kfree(process);
    klist_set(&process_klist, pid, NULL);

    //printk("Killed process '%s'\n", process->name);
    if (inton)
        interrupts();

    if (self)
        yield();

    return true;
}

int process_icreate(char* image_path, char* args[]) {
    int ret;

    if (!fs_exists(image_path))
        return ERR_NOT_FOUND;

    char* name = image_path + strlen(image_path);
    while (*name != '/')
        --name;

    name++;

    char* max = image_path + strlen(image_path);
    char* end = name;
    while (*end != '.' && end <= max)
        end++;

    struct exec_data exec;
    CLEANUP page_tracker_t* tracker = kmalloc(sizeof(page_tracker_t));

    ret = elf_load(image_path, args, &exec, tracker);
    if (ret < 0)
        return ret;
        
    char before = *end;
    if (*end == '.')
        *end = '\0';

    ret  = process_create(name, image_path, 0);
    *end = before;

    process_t* process = process_get(ret);

    memcpy(process->ptracker, tracker, sizeof(page_tracker_t));
    thread_t* main_thread = thread_create(process, exec.pentry, false);
    main_thread->name = kmalloc(6);
    strcpy(main_thread->name, "Entry");

    init_entry_caller(main_thread->task, exec.entry, exec.ker_proc_addr, args_count(args));

    return ret;
}

int process_start(process_t* process) {
    klist_entry_t* klist_entry = NULL;
    struct thread* thread = NULL;

    hook_proc_data_t pkill_data = {
        .pid = process->pid,
    };
    hook_invoke(HOOK_PSTART, &pkill_data);

    while (true) {
        thread = klist_iter(&(process->threads), &klist_entry);
        if (thread == NULL)
            break;

        thread_start(thread);
    }
    return 0;
}

uint64_t process_used_phys_memory(uint64_t pid) {
    process_t* proc = process_get(pid);
    return (proc->ptracker->dir_frames_used + proc->ptracker->frames_used - proc->ptracker->map_frames_used) * CPU_PAGE_SIZE;
}

uint64_t process_mapped_memory(uint64_t pid) {
    process_t* proc = process_get(pid);
    return (proc->ptracker->map_frames_used) * CPU_PAGE_SIZE;
}

void proc_set_stdin(process_t* process, file_t* fd) {
    klist_set(&(process->fiddies), 0, fd);
}

void proc_set_stdout(process_t* process, file_t* fd) {
    klist_set(&(process->fiddies), 1, fd);
}

void proc_set_stderr(process_t* process, file_t* fd) {
    klist_set(&(process->fiddies), 2, fd);
}

void proc_set_dir(struct process* process, char* path) {
    if (process->working_dir != NULL)
        kfree(process->working_dir);

    process->working_dir = kmalloc(strlen(path) + 1);
    strcpy(process->working_dir, path);
}

struct spawn_options {
    int stdin;
    int stdout;
    int stderr;
};
typedef struct spawn_options spawn_options_t;

int syscall_spawn(char* image_path, spawn_options_t* options, char* args[]) {
    long fret;
    if ((fret = check_user_file_access(image_path, FS_MODE_EXECUTE)) != 0)
        return fret;

    int ret = process_icreate(image_path, args);
    if (ret < 0)
        return ret;

    process_t* proc = process_get(ret);
    file_t* file;

    for (int i = 0; i <= 2; i++) {
        file = klist_get(&(process_current->fiddies), i);
        file->ref_count++;
        klist_set(&(proc->fiddies), i, file);
    }
    proc->working_dir = kmalloc(strlen(process_current->working_dir) + 1);
    strcpy(proc->working_dir, process_current->working_dir);

    proc->parent_pid = process_current->pid;

    if (options != NULL) {

    }
    process_start(proc);
    return ret;
}

int syscall_wait(int pid) {
    io_block(&(process_get(pid)->wait_list));
    return 0;
}

int syscall_getcwd(char* buffer, size_t size) {
    if (strlen(process_current->working_dir) + 1 > size)
        return ERR_NO_SPACE;

    mutex_acquire(&(process_current->access));
    strcpy(buffer, process_current->working_dir);
    mutex_release(&(process_current->access));

    return 0;
}

int syscall_setcwd(char* path) {
    finfo_t info;

    int len = strlen(path);

    char* buffer = kmalloc(len + 4);
    translate_path(buffer, NULL, path);
    if (path[len - 1] != '/') {
        buffer[len] = '/';
        buffer[len + 1] = '\0';
    }
    
    int ret = fs_info(buffer, &info);
    if (ret < 0) {
        kfree(buffer);
        return ret;
    }
    
    long fret;
    if ((fret = check_user_file_access(buffer, FS_MODE_EXECUTE)) != 0)
        return fret;
    
    if (info.type != FS_TYPE_DIR) {
        kfree(buffer);
        return FS_ERR_NOT_DIR;
    }
    mutex_acquire(&(process_current->access));

    kfree(process_current->working_dir);
    process_current->working_dir = buffer;

    mutex_release(&(process_current->access));

    return 0;
}

void proc_init() {
    syscalls[SYSCALL_SPAWN]  = syscall_spawn;
    syscalls[SYSCALL_WAIT]   = syscall_wait;
    syscalls[SYSCALL_GETCWD] = syscall_getcwd;
    syscalls[SYSCALL_SETCWD] = syscall_setcwd;

    klist_init(&process_klist);

    uint64_t pid = process_create("aexkrnl", "/sys/aexkrnl.elf", 0);
    process_current = process_get(pid);

    //kfree(process_current->ptracker); // Look into why this breaks the process klist later
    process_current->ptracker = &kernel_pgtrk;
}

void thread_cleaner() {
    thread_t* th;
    while (true) {
        cbuf_read(&thread_cleanup_queue, (uint8_t*) &th, sizeof(thread_t*));
        _thread_kill(th);
    }
}

void proc_initsys() {
    struct thread* new_thread = kmalloc(sizeof(struct thread));
    memset(new_thread, 0, sizeof(struct thread));

    new_thread->id      = process_current->thread_counter++;
    new_thread->process = process_current;
    new_thread->name    = "Main Kernel Thread";
    new_thread->task    = task_current;
    
    task_current->thread = new_thread;

    klist_set(&(process_current->threads), new_thread->id, new_thread);
    new_thread->task = task_current;

    cbuf_create(&thread_cleanup_queue, 4096);

    thread_t* th = thread_create(process_current, thread_cleaner, true);
    th->name = "Thread Cleaner";
    thread_start(th);
}