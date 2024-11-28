#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/page.h"

void swap_table_init (void);
void swap_in (struct page *page, void *addr);
int swap_out (void *addr);

#endif