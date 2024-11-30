#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include "vm/page.h"

void swap_table_init (void);
bool swap_in (size_t swap_slot, void *addr);
size_t swap_out (void *addr);

#endif