#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <stdbool.h>
#include "threads/thread.h"
#include "vm/page.h"

// System Calls - pintos 2
typedef int pid_t; // Process identifier type
// end

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

void pass_argument (char *file_name, void **esp);
struct thread *get_child (pid_t pid);

bool fault_handler (struct page *page);
bool stack_growth (void *addr);

#endif /* userprog/process.h */
