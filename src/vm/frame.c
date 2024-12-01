#include <stdlib.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"


struct list frame_table;
struct lock frame_lock;
struct list_elem *frame_clock;

extern struct lock file_lock;

void
frame_table_init (void)
{
    list_init (&frame_table);
    lock_init (&frame_lock);
    frame_clock = NULL;
}

void
frame_insert (struct frame *frame)
{
    lock_acquire (&frame_lock);
    list_push_back (&frame_table, &frame->elem);
    lock_release (&frame_lock);
}

void
frame_delete (struct frame *frame)
{
    if (frame_clock != &frame->elem)
        list_remove (&frame->elem);
    else
        frame_clock = list_remove (frame_clock);
}

struct frame *
frame_allocate (enum palloc_flags flags)
{
    struct frame *frame;

    frame = (struct frame *) malloc (sizeof (struct frame));
    if (frame == NULL)
        return NULL;
    memset (frame, 0, sizeof (struct frame));

    frame->page_addr = palloc_get_page (flags);
    while (frame->page_addr == NULL)
    {
        evict ();
        frame->page_addr = palloc_get_page (flags);
    }

    frame->thread = thread_current ();
    frame->pinning = false;
    frame_insert (frame);
    
    // printf("frame_allocate (enum palloc_flags flags)##########################################\n");
    // printf("[DEBUG] Allocating frame at: %p\n", frame);
    // printf("[DEBUG] Frame physical address: %p\n", frame->page_addr);

    return frame;
}

void
frame_deallocate (struct frame *frame)
{
    printf("111111111");
    printf("[DEBUG] frame address: %p\n", frame);
    printf("[DEBUG] frame->page address: %p\n", frame->page);
    printf("[DEBUG] frame->page->is_loaded: %d\n", frame->page->is_loaded);
    printf("[DEBUG] frame->page->vaddr: %p\n", frame->page->addr); // 페이지의 가상 주소
    if (frame != NULL)
    {  printf("22222222222");
        frame->page->is_loaded = false;
        printf("22222222222");
        pagedir_clear_page (frame->thread->pagedir, frame->page->addr);
        printf("22222222222");
        palloc_free_page (frame->page_addr);
        printf("22222222222");
        frame_delete (frame);
        printf("22222222222");
        free (frame);
        printf("22222222222");
    }
    printf("33333333");
}

struct frame *
frame_find (void *addr)
{
    struct frame *frame;
    struct list_elem *e;

    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
    {
        frame = list_entry (e, struct frame, elem);
        if (frame->page->addr == addr)
        {
            return frame;
        }
    }

    return NULL;
}

void
evict (void)
{
    struct frame *frame;
    bool dirty;

    while (true)
    {
        if ((frame_clock == NULL) || (frame_clock == list_end (&frame_table)))
        {
            if (!list_empty (&frame_table))
            {
                frame_clock = list_begin (&frame_table);
            }
            else
            {
                frame = NULL;
                break;
            }
        }
        else
        {
            frame_clock = list_next (frame_clock);
            if (frame_clock == list_end (&frame_table))
            {
                continue;
            }
        }

        frame = list_entry (frame_clock, struct frame, elem);

        if (frame->pinning == false)
        {
            if (!pagedir_is_accessed (frame->thread->pagedir, frame->page->addr))
            {
                break;
            }
            else
            {
                pagedir_set_accessed (frame->thread->pagedir, frame->page->addr, false);
            }
        }
    }

    dirty = pagedir_is_dirty (frame->thread->pagedir, frame->page->addr);

    if (frame->page->type == BINARY)
    {
        if (dirty)
        {
            frame->page->swap_slot = swap_out (frame->page_addr);
            frame->page->type = ANONYMOUS;
        }
    }
    else if (frame->page->type == FILE)
    {
        if (dirty)
        {
            lock_acquire (&file_lock);
            file_write_at (frame->page->file, frame->page_addr, frame->page->read_byte, frame->page->offset);
            lock_release (&file_lock);
        }
    }
    else if (frame->page->type == ANONYMOUS)
    {
        frame->page->swap_slot = swap_out (frame->page_addr);
    }
    frame->page->is_loaded = false;
    frame_deallocate (frame);
}