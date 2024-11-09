#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

// System Calls - pintos 2
typedef int pid_t; // Process identifier type
// end

void syscall_init (void);

// System Calls - pintos 2
void check_valid_addr (const void *addr);
void get_argument (void *esp, int *argv, int num);
void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
// end

#endif /* userprog/syscall.h */
