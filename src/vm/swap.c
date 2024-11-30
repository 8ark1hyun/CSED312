#include "vm/swap.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#define SECTOR_NUM (PGSIZE / BLOCK_SECTOR_SIZE)

struct bitmap *swap_table;
struct block *swap_disk;
struct lock swap_lock;

void
swap_table_init (void)
{
    swap_disk = block_get_role (BLOCK_SWAP);
    swap_table = bitmap_create (block_size (swap_disk) / SECTOR_NUM);
    bitmap_set_all (swap_table, true);
    lock_init (&swap_lock);
}

bool
swap_in (size_t swap_slot, void *addr)
{
    int start = SECTOR_NUM * swap_slot;

    lock_acquire (&swap_lock);
    for (int i = 0; i < SECTOR_NUM; i++)
    {
        block_read (swap_disk, start + i, addr + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_set (swap_table, swap_slot, false);
    lock_release (&swap_lock);

    return true;
}

size_t
swap_out (void *addr)
{
    size_t swap_slot;
    int start;

    lock_acquire (&swap_lock);
    swap_slot = bitmap_scan_and_flip (swap_table, 0, 1, false);
    if (swap_slot == BITMAP_ERROR)
    {
        lock_release (&swap_lock);
        NOT_REACHED ();
        return BITMAP_ERROR;
    }
    start = SECTOR_NUM * swap_slot;
    for (int i = 0; i < SECTOR_NUM; i++)
    {
        block_write (swap_disk, start + i, addr + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock);

    return swap_slot;
}