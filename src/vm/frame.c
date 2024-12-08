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

extern struct lock file_lock;

struct list frame_table;       // frame을 관리하기 위한 frame table
struct lock frame_lock;        // synchronization을 위한 frame lock
struct list_elem *frame_clock; // page replacement policy(clock algorithm)를 위한 clock hand

void
frame_table_init (void)
{
    list_init (&frame_table); // frame_table 초기화
    lock_init (&frame_lock);  // frame_lock 초기화
    frame_clock = NULL;       // frame_clock 초기화
}

void
frame_insert (struct frame *frame)
{
    list_push_back (&frame_table, &frame->elem); // frame 삽입
}

void
frame_delete (struct frame *frame)
{
    if (frame_clock != &frame->elem)
        list_remove (&frame->elem); // frame 제거
    else if (frame_clock == &frame->elem)
        // 제거하는 frame의 다음 frame으로 clock hand 설정
        frame_clock = list_remove (frame_clock); // frame 제거
}

struct frame *
frame_allocate (enum palloc_flags flags)
{
    struct frame *frame;

    lock_acquire (&frame_lock); // lock 획득
    frame = (struct frame *) malloc (sizeof (struct frame)); // frame 구조체 할당
    if (frame == NULL)
        return NULL;
    memset (frame, 0, sizeof (struct frame)); // 메모리 초기화

    // frame 할당
    frame->page_addr = palloc_get_page (flags);
    while (frame->page_addr == NULL) // frame를 할당할 공간이 없으면 eviction을 통해 할당할 때까지 반복
    {   
        evict (); // eviction
        frame->page_addr = palloc_get_page (flags); // eviction 후 다시 frame 할당
    }

    frame->thread = thread_current (); // frame을 할당한 현재 thread
    frame->pinning = false; // swap 가능 상태 표시
    frame_insert (frame); // frame_table에 frame 삽입
    lock_release (&frame_lock); // lock 해제

    return frame;
}

void
frame_deallocate (void *addr)
{
    struct frame *frame;
    struct list_elem *e;

    // addr에 대응되는 frame 탐색
    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
    {
        frame = list_entry (e, struct frame, elem);
        if (frame->page_addr == addr)
        {
            break;
        }
        else
        {
            frame = NULL;
        }
    }

    lock_acquire (&frame_lock); // lock 획득
    if (frame != NULL) // addr에 대응되는 frame이 존재하면
    {
        frame->page->is_load = false; // page load 상태 표시
        pagedir_clear_page (frame->thread->pagedir, frame->page->addr); // page-frame 매핑 해제
        palloc_free_page (frame->page_addr); // frame 할당 해제
        frame_delete (frame); // frame 제거
        free (frame); // frame 구조체 할당 해제
    }
    lock_release (&frame_lock); // lock 해제
}

struct frame *
frame_find (void *addr)
{
    struct frame *frame;
    struct list_elem *e;

    // addr에 대응되는 page와 매핑된 frame 탐색
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
        // frame_clock(== clock hand)이 NULL이거나, frame_table의 마지막을 가리키고 있는 경우
        if ((frame_clock == NULL) || (frame_clock == list_end (&frame_table)))
        {
            if (!list_empty (&frame_table)) // frame_table이 비어있지 않으면
            {
                // frame_clock이 frame_table의 첫 요소(첫 frame)를 가리키도록 설정
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
            frame_clock = list_next (frame_clock); // frame_clock이 다음 frame을 가리키도록 설정
            if (frame_clock == list_end (&frame_table)) // frame_table의 마지막을 가리키면 한 번 더 반복
            {
                continue;
            }
        }

        frame = list_entry (frame_clock, struct frame, elem); // frame_clock이 가리키는 frame

        if (frame->pinning == false) // pinning이 false이면, 즉 swap이 가능하면
        {
            // access bit 확인
            // access bit가 0이면, 즉 최근에 참조되지 않은 경우 eviction 대상으로 설정
            if (!pagedir_is_accessed (frame->thread->pagedir, frame->page->addr))
            {
                break;
            }
            // access bit가 1이면, 즉 최근에 참조된 경우 access bit를 0으로 설정 후 다음 frame으로 이동
            else
            {
                pagedir_set_accessed (frame->thread->pagedir, frame->page->addr, false);
            }
        }
    }

    // dirty bit 확인
    // dirty bit가 0이면 수정된 적이 없음을,
    // dirty bit가 1이면 수정되어 write-back이 필요함을 의미
    dirty = pagedir_is_dirty (frame->thread->pagedir, frame->page->addr);

    // 실행 파일 페이지인 경우
    // 즉, 프로그램의 실행 코드와 관련된 페이지인 경우
    if (frame->page->type == BINARY)
    {
        if (dirty) // 수정된 적이 있으면
        {
            // swap out (swap 영역에 저장)
            frame->page->swap_slot = swap_out (frame->page_addr);
            frame->page->type = ANONYMOUS;
        }
    }
    // 메모리 매핑된 파일 페이지인 경우
    // 즉, 파일과 매핑된 페이지인 경우
    else if (frame->page->type == FILE)
    {
        if (dirty) // 수정된 적이 있으면
        {
            lock_acquire (&file_lock); // lock 획득
            // write-back
            file_write_at (frame->page->file, frame->page_addr, frame->page->read_byte, frame->page->offset);
            lock_release (&file_lock); // lock 해제
        }
    }
    // 익명 페이지인 경우
    // 즉, 파일과 연결되지 않은 페이지인 경우
    else if (frame->page->type == ANONYMOUS)
    {
        // swap out (swap 영역에 저장)
        frame->page->swap_slot = swap_out (frame->page_addr);
    }
    
    frame->page->is_load = false; // page load 상태 표시
    pagedir_clear_page (frame->thread->pagedir, frame->page->addr); // page-frame 매핑 해제
	palloc_free_page (frame->page_addr); // frame 할당 해제
    frame_delete (frame); // frame 제거
    free (frame); // frame 구조체 할당 해제
}
