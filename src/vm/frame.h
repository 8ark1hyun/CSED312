#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/page.h"

struct frame
{
    void *page_addr;
    struct page *vpage;
    struct thread *thread;
    struct list_elem elem;
    bool pinning;
};

struct list frame_table;
struct lock frame_lock;

void frame_table_init (void);
void frame_insert (struct frame *frame);
void frame_delete (struct frame *frame);
struct frame *frame_allocate (void *page_addr);
void frame_deallocate (struct frame *frame);

#endif