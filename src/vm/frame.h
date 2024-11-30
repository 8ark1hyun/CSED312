#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/thread.h"
#include "vm/page.h"

struct frame
{
    void *page_addr;
    struct page *page;
    struct thread *thread;
    struct list_elem elem;
    bool pinning;
};

void frame_table_init (void);
void frame_insert (struct frame *frame);
void frame_delete (struct frame *frame);
struct frame *frame_allocate (enum palloc_flags flags);
void frame_deallocate (struct frame *frame);
struct frame *frame_find (void *addr);
void evict (void);

#endif