#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "vm/frame.h"

struct page
{
    void *vaddr;
    struct frame *frame;
    bool writable;
    struct file *file;
    struct hash_elem elem;
    size_t swap_slot;
};

void vm_init (struct hash *vm);
void page_insert (struct hash *vm, struct page *page);
void page_delete (struct page *page);
struct page *page_allocate (void *vaddr, bool writable, struct file *file);
void page_deallocate (struct page *page);
void vm_destroy (struct hash *vm);

#endif