#include "vm/swap.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#define SECTOR_NUM (PGSIZE / BLOCK_SECTOR_SIZE)

static struct bitmap *swap_table;
static struct block *swap_disk;
static struct lock swap_lock;

void
swap_table_init (void)
{
    swap_disk = block_get_role (BLOCK_SWAP);
    swap_table = bitmap_create (block_size (swap_disk) / SECTOR_NUM);
    bitmap_set_all (swap_table, true);
    lock_init (&swap_lock);
}

void
swap_in (struct page *page, void *addr)
{

}

int
swap_out (void *addr)
{
    
}