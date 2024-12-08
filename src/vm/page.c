#include <stdlib.h>
#include <stdio.h>
#include "vm/page.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "lib/kernel/hash.h"

static unsigned vm_hash_func (const struct hash_elem *e, void *aux);
static bool vm_less_func (const struct hash_elem *e, const struct hash_elem *b, void *aux);
static void vm_destroy_func (struct hash_elem *e, void *aux);

extern struct lock file_lock;

// hash 값 계산 함수
static unsigned
vm_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
    struct page *page = hash_entry (e, struct page, elem);

    return hash_int ((int) page->addr);
}

// 두 hash 요소의 page 주소 비교 함수
static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    return hash_entry (a, struct page, elem)->addr < hash_entry (b, struct page, elem)->addr;
}

// vm(suppelmental page table) 제거 함수
static void
vm_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
    struct page *page = hash_entry (e, struct page, elem);
    void *addr;
    
    if (page != NULL)
    {
        if (page->is_load == true)
        {
            addr = pagedir_get_page (thread_current ()->pagedir, page->addr);
            frame_deallocate (addr);
        }
        free (page);
    }
}

void
vm_init (struct hash *vm)
{
    hash_init (vm, vm_hash_func, vm_less_func, NULL); // vm(supplemental page table) 초기화
}

void
vm_destroy (struct hash *vm)
{
    hash_destroy (vm, vm_destroy_func); // vm(supplemental page table) 제거
}

bool
page_insert (struct hash *vm, struct page *page)
{   
    if (hash_insert (vm, &page->elem) == NULL) // page 삽입
        return false;
    else
        return true;
}

bool
page_delete (struct page *page)
{
    if (hash_delete (&thread_current ()->vm, &page->elem) == NULL) // page 제거
        return false;
    else       
        return true;
}

struct page *
page_allocate (enum page_type type, void *addr, bool writable, bool is_load, uint32_t offset, uint32_t read_byte, uint32_t zero_byte, struct file *file)
{
    struct page *page = (struct page*) malloc (sizeof (struct page)); // page 구조체 할당
    if (page == NULL)
    {
        return NULL;
    } 
    memset (page, 0, sizeof (struct page)); // 메모리 초기화

    // page 각 필드 초기화
    page->type = type;
    page->addr = addr;
    page->frame = NULL;
    page->writable = writable;
    page->is_load = is_load;
    page->offset = offset;
    page->read_byte = read_byte;
    page->zero_byte = zero_byte;
    page->file = file;
    page->swap_slot = 0;

    page_insert (&thread_current ()->vm, page); // page 삽입

    return page;
}

void
page_deallocate (struct page *page)
{
    void *addr;

    if (page_delete (page) == true) // page 제거
    {
        addr = pagedir_get_page (thread_current ()->pagedir, page->addr);
        frame_deallocate (addr); // frame 할당 해제
        free (page); // page 구조체 할당 해제
    }
}

struct page *
page_find (void *addr)
{
    struct hash *vm = &thread_current ()->vm;
    struct page page;
    struct hash_elem *e;

    page.addr = pg_round_down (addr); // page 단위로 주소 내림

    e = hash_find (vm, &page.elem); // page 탐색

    if (e != NULL)
    {
        return hash_entry (e, struct page, elem); // page 반환
    }
    else
    {
        return NULL;
    }
}

bool
load_file (void *addr, struct page *page)
{
    lock_acquire (&file_lock); // lock 획득
    int read_byte = file_read_at (page->file, addr, page->read_byte, page->offset); // file 읽기
    lock_release (&file_lock); // lock 해제

    // 읽은 bytes 수와 요청한 bytes 수가 다르면
    if (read_byte != (int) page->read_byte)
    {
        return false;
    }

    memset (addr + page->read_byte, 0, page->zero_byte); // 남은 메모리 초기화
    return true;
}