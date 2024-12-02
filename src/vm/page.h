#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lib/kernel/hash.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

enum page_type
{
    BINARY = 0,
    FILE = 1,
    ANONYMOUS = 2
};

struct page
{
    enum page_type type;
    void *addr;
    struct frame *frame;
    bool writable;
    bool is_load; // physical memory의 load 여부 flag 
    uint32_t offset;
    uint32_t read_byte;
    uint32_t zero_byte;
    struct file *file;
    struct hash_elem elem;
    struct list_elem mmap_elem;
    size_t swap_slot;
};

struct mmap_file
{
    mapid_t mapid;
    void *addr;
    struct file *file;
    struct list page_list;
    struct list_elem elem;
};

void vm_init (struct hash *vm);
void vm_destroy (struct hash *vm);
bool page_insert (struct hash *vm, struct page *page);
bool page_delete (struct page *page);
struct page *page_allocate (enum page_type type, void *addr, bool writable, bool is_load, uint32_t offset, uint32_t read_byte, uint32_t zero_byte, struct file *file);
void page_deallocate (struct page *page);
struct page *page_find (void *addr);
bool load_file (void *addr, struct page *page);

#endif