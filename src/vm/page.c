#include <stdlib.h>
#include "vm/page.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "lib/kernel/hash.h" // 헤더 포함
#define list_elem_to_hash_elem(LIST_ELEM) \
        list_entry(LIST_ELEM, struct hash_elem, list_elem)



static unsigned vm_hash_func (const struct hash_elem *e, void *aux);
static bool vm_less_func (const struct hash_elem *e, const struct hash_elem *b, void *aux);
static void vm_destroy_func (struct hash_elem *e, void *aux);

extern struct lock frame_lock;
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

static void
vm_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
    struct page *page = hash_entry (e, struct page, elem);
    struct frame *frame;
    
    if (page != NULL)
    {
        frame = pagedir_get_page (thread_current ()->pagedir, page->addr);

        if (page -> is_loaded)
        {  
            frame_deallocate (frame);
        }
        free (page);
    }
}

void
vm_init (struct hash *vm)
{
    hash_init (vm, vm_hash_func, vm_less_func, NULL);   
}

void
vm_destroy (struct hash *vm)
{
    hash_destroy (vm, vm_destroy_func);
}

bool
page_insert (struct hash *vm, struct page *page)
{   
    if (hash_insert (vm, &page->elem) == NULL) {
        printf("real falese?FD?FD??"); return false;}
    else
    {
        //         // 삽입 성공: 디버깅 출력 추가
        // printf("[DEBUG] Successfully inserted page: addr=%p, is_loaded=%d, writable=%d\n",
        //        page->addr, page->is_loaded, page->writable);

        // // 현재 해시 테이블 상태 출력
        // printf("[DEBUG] VM contents after insertion:\n");
        // for (size_t i = 0; i < vm->bucket_cnt; i++) {
        //     struct list *bucket = &vm->buckets[i];
        //     printf("  Bucket %zu:\n", i);

        //     for (struct list_elem *e = list_begin(bucket); e != list_end(bucket); e = list_next(e)) {
        //         struct hash_elem *he = list_elem_to_hash_elem(e);
        //         struct page *p = hash_entry(he, struct page, elem);

        //         // 각 페이지 정보 출력
        //         printf("    Page addr: %p, is_loaded: %d, writable: %d\n",
        //                p->addr, p->is_loaded, p->writable);
        //     }
        // }
        return true;
    }
}

bool
page_delete (struct page *page)
{
    if (hash_delete (&thread_current ()->vm, &page->elem) == NULL)
        return false;
    else       
        return true;
}

struct page *
page_allocate (enum page_type type, void *addr, bool writable, uint32_t offset, uint32_t read_byte, uint32_t zero_byte, struct file *file)
{
    lock_acquire (&frame_lock);
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
    lock_release (&frame_lock);

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