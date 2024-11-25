#include "vm/page.h"

static unsigned
vm_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
    struct page *page = hash_entry (e, struct page, elem);

    return hash_int ((int) page->vaddr);
}

static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    return hash_entry (a, struct page, elem)->vaddr < hash_entry (b, struct page, elem)->vaddr;
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
page_allocate (void *vaddr, bool writable, struct file *file)
{
    struct page *page = (struct page*) malloc (sizeof (struct page));
    if (page == NULL)
    {
        return NULL;
    }
    memset (page, 0, sizeof (struct page));

    page->vaddr = vaddr;
    page->frame = NULL;
    page->writable = writable;
    page->file = file;

    page_insert (&thread_current ()->vm, page);
    return page;
}

void
page_deallocate (struct page *page)
{
    lock_acquire (&frame->lock);
    if (page != NULL)
    {
        if (page->frame != NULL)
        {
            frame_deallocate (page->frame);
        }
        page_delete (page);
        free (page);
    }
    lock_acquire (&frame->lock);
}