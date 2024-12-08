#include "vm/swap.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <bitmap.h>

#define SECTOR_NUM (PGSIZE / BLOCK_SECTOR_SIZE)

static struct bitmap *swap_table; // swap slot을 추적하기 위한 swap table
static struct block *swap_disk;   // swap disk
static struct lock swap_lock;     // synchronization을 위한 swap lock

void
swap_table_init (void)
{
    swap_disk = block_get_role (BLOCK_SWAP); // swap_disk 초기화
    swap_table = bitmap_create (block_size (swap_disk) / SECTOR_NUM); // swap_table 초기화
    lock_init (&swap_lock); // swap_lock 초기화
}

bool
swap_in (size_t swap_slot, void *addr)
{
    int i;
    int start = SECTOR_NUM * swap_slot; // disk를 읽을 시작 위치

    lock_acquire (&swap_lock); // lock 획득
    // swap_slot이 swap_table 크기 밖이거나, swap_slot이 사용 중이 아닐 때
    if ((swap_slot >= bitmap_size (swap_table)) || (bitmap_test (swap_table, swap_slot) == false))
    {
        lock_release (&swap_lock); // lock 해제
        return false;
    }

    for (i = 0; i < SECTOR_NUM; i++)
    {
        if (start + i >= block_size (swap_disk))
        {
            lock_release (&swap_lock); // lock 해제
            PANIC ("Swap access out of bounds during swap_in!");
        }
        block_read (swap_disk, start + i, addr + i * BLOCK_SECTOR_SIZE); // swap_disk에서 메모리로 데이터 읽어오기
    }
    bitmap_flip (swap_table, swap_slot); // bit를 반전하여 swap_slot 해제
    lock_release (&swap_lock); // lock 해제

    return true;
}

size_t
swap_out (void *addr)
{
    int i;
    size_t swap_slot;
    int start;

    lock_acquire (&swap_lock); // lock 획득
    swap_slot = bitmap_scan_and_flip (swap_table, 0, 1, false); // 해제된 swap_slot을 찾아 사용 중으로 표시
    if (swap_slot == BITMAP_ERROR)
    {
        lock_release (&swap_lock); // lock 해제
        PANIC ("No available swap slots!");
    }
    start = SECTOR_NUM * swap_slot;
    for (i = 0; i < SECTOR_NUM; i++)
    {
        if (start + i >= block_size (swap_disk))
        {
            lock_release (&swap_lock); // lock 해제
            PANIC ("Swap access out of bounds during swap_out!");
        }
        // 메모리에서 swap_disk로 데이터 쓰기
        block_write (swap_disk, start + i, addr + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock); // lock 해제

    return swap_slot;
}