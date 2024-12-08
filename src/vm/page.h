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

// Supplemental Page Table - pintos 3
enum page_type
{
    BINARY = 0,
    FILE = 1,
    ANONYMOUS = 2
};

struct page
{
    enum page_type type;         // Page Type
    void *addr;                  // Page Address
    struct frame *frame;         // Frame
    bool writable;               // Writeable or not
    bool is_load;                // Load or not to Physical Memory
    uint32_t offset;             // Offset
    uint32_t read_byte;          // Read bytes
    uint32_t zero_byte;          // Zero bytes
    struct file *file;           // File
    struct hash_elem elem;       // Page Table List element
    struct list_elem mmap_elem;  // Memory Mapped File List element
    size_t swap_slot;            // Swap Slot
};
// end

// File Memory Mapping - pintos 3
struct mmap_file
{
    mapid_t mapid;               // Mapping ID
    void *addr;                  // Address
    struct file *file;           // File
    struct list page_list;       // Page List
    struct list_elem elem;       // Memory Mapped File List element
};
// end

void vm_init (struct hash *vm);
void vm_destroy (struct hash *vm);
bool page_insert (struct hash *vm, struct page *page);
bool page_delete (struct page *page);
struct page *page_allocate (enum page_type type, void *addr, bool writable, bool is_load, uint32_t offset, uint32_t read_byte, uint32_t zero_byte, struct file *file);
void page_deallocate (struct page *page);
struct page *page_find (void *addr);
bool load_file (void *addr, struct page *page);

#endif