#include <stdlib.h>
#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "filesys/file.h"

static struct list frame_table;
static struct lock frame_lock;
static struct list_elem *frame_clock;

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
    list_push_back (&frame_table, &frame->elem);
}

void
frame_delete (struct frame *frame)
{
    list_remove (&frame->elem);
}

struct frame *
frame_allocate (enum palloc_flags flags)
{
    struct frame *frame;

    frame = (struct frame *) malloc (sizeof (struct frame));
    memset (frame, 0, sizeof (struct frame));

    if (frame == NULL)
    {
        return NULL;
    }
    else
    {
        lock_acquire (&frame_lock);
        frame->page_addr = palloc_get_page (flags);
        if (frame->page_addr == NULL)
        {
            evict ();
            frame->page_addr = palloc_get_page (flags);
            if (frame->page_addr == NULL)
            {
                lock_release (&frame_lock);
                return NULL;
            }
        }
        frame->thread = thread_current ();
        frame->pinning = false;
        frame_insert (frame);
        lock_release (&frame_lock);
        return frame;
    }
}

void
frame_deallocate (struct frame *frame)
{
    lock_acquire (&frame_lock);
    if (frame == NULL)
    {
        exit (-1);
    }
    pagedir_clear_page (frame->thread->pagedir, frame->page->addr);
    palloc_free_page (frame->page_addr);
    frame_delete (frame);
    free (frame);
    lock_release (&frame_lock);
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
            //frame->page->swap_slot = swap_out (frame->page_addr);
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
        //frame->page->swap_slot = swap_out (frame->page_addr);
    }

    frame_deallocate (frame);

}