#include "vm/frame.h"
#include "threads/palloc.h"

void
frame_table_init (void)
{
    list_init (&frame_table);
    lock_init (&frame_lock);
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
    lock_acquire (&frame_lock);
    list_remove (&frame->elem);
    lock_release (&frame_lock);
}

struct frame *
frame_allocate (void *page_addr)
{
    struct frame *frame;
    
    if (page_addr == NULL)
    {
        lock_acquire (&frame_lock);
        frame = evict ();
        lock_release (&frame_lock);
    }
    else
    {
        frame = (struct frame *) malloc (sizeof (struct frame));
        if (frame == NULL)
        {
            return NULL;
        }
        frame_insert (frame);
    }

    frame->page_addr = page_addr;
    frame->thread = thread_current ();
    frame->pinning = false;

    return frame;
}

void
frame_deallocate (struct frame *frame)
{
    palloc_free_page (frame->page_addr);
    frame_delete (frame);
    free (frame);
}
