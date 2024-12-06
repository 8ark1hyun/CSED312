#include "vm/swap.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <bitmap.h>

#define SECTOR_NUM (PGSIZE / BLOCK_SECTOR_SIZE)

static struct bitmap *swap_table;
static struct block *swap_disk;
static struct lock swap_lock;

void
swap_table_init (void)
{
    swap_disk = block_get_role (BLOCK_SWAP);
    swap_table = bitmap_create (block_size (swap_disk) / SECTOR_NUM);
    lock_init (&swap_lock);
}

bool
swap_in (size_t swap_slot, void *addr)
{
    int i;
    int start = SECTOR_NUM * swap_slot;

    lock_acquire (&swap_lock);
    if ((swap_slot >= bitmap_size (swap_table)) || (bitmap_test (swap_table, swap_slot) == false))
    {
        lock_release (&swap_lock);
        return false;
    }

    for (i = 0; i < SECTOR_NUM; i++)
    {
        if (start + i >= block_size (swap_disk))
        {
            lock_release (&swap_lock);
            PANIC ("Swap access out of bounds during swap_in!");
        }
        block_read (swap_disk, start + i, addr + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_flip (swap_table, swap_slot);
    lock_release (&swap_lock);

    return true;
}

size_t
swap_out (void *addr)
{
    int i;
    size_t swap_slot;
    int start;

    lock_acquire (&swap_lock);
    swap_slot = bitmap_scan_and_flip (swap_table, 0, 1, false);
    if (swap_slot == BITMAP_ERROR)
    {
        lock_release (&swap_lock);
        PANIC ("No available swap slots!");
    }
    start = SECTOR_NUM * swap_slot;
    for (i = 0; i < SECTOR_NUM; i++)
    {
        if (start + i >= block_size (swap_disk))
        {
            lock_release (&swap_lock);
            PANIC ("Swap access out of bounds during swap_out!");
        }
        block_write (swap_disk, start + i, addr + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock);

    return swap_slot;
}