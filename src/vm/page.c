#include <stdlib.h>
#include "vm/page.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"

extern struct lock file_lock;

static unsigned
vm_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
    struct page *page = hash_entry (e, struct page, elem);

    return hash_int ((int) page->addr);
}

static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    return hash_entry (a, struct page, elem)->addr < hash_entry (b, struct page, elem)->addr;
}

void
vm_init (struct hash *vm)
{
    hash_init (vm, vm_hash_func, vm_less_func, NULL);   
}

void
page_insert (struct hash *vm, struct page *page)
{
    hash_insert (vm, &page->elem);
}

void
page_delete (struct page *page)
{
    hash_delete (&thread_current ()->vm, &page->elem);
}

struct page *
page_allocate (enum page_type type, void *addr, bool writable, uint32_t offset, uint32_t read_byte, uint32_t zero_byte, struct file *file)
{
    struct page *page = (struct page*) malloc (sizeof (struct page));
    if (page == NULL)
    {
        return NULL;
    }
    memset (page, 0, sizeof (struct page));

    page->type = type;
    page->addr = addr;
    page->frame = NULL;
    page->writable = writable;
    page->offset = offset;
    page->read_byte = read_byte;
    page->zero_byte = zero_byte;
    page->file = file;
    page->swap_slot = 0;

    page_insert (&thread_current ()->vm, page);
    return page;
}

void
page_deallocate (struct page *page)
{
    if (page != NULL)
    {
        if (page->frame != NULL)
        {
            frame_deallocate (page->frame);
        }
        page_delete (page);
        free (page);
    }
}

struct page *
page_find (void *addr)
{
    struct hash *vm = &thread_current ()->vm;
    struct page page;
    struct hash_elem *e;

    page.addr = pg_round_down (addr);

    e = hash_find (vm, &page.elem);

    if (e != NULL)
    {
        return hash_entry (e, struct page, elem);
    }
    else
    {
        return NULL;
    }
}

bool
load_file (void *addr, struct page *page)
{
    lock_acquire (&file_lock);
    int read_byte = file_read_at (page->file, addr, page->read_byte, page->offset);
    lock_release (&file_lock);

    if (read_byte != (int) page->read_byte)
    {
        return false;
    }

    memset (addr + page->read_byte, 0, page->zero_byte);
    return true;
}