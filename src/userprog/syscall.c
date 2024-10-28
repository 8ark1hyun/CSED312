#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

// System Calls - pintos 2
void
halt (void)
{
  shutdown_power_off ();
}

void
exit (int status)
{
  // Process Termination Messages - pintos 2
  const char *process_name = thread_name (); // process's name
  int exit_code = status; // exit code
  thread_current ()->exit_status = status; // exit status 저장

  printf ("%s: exit(%d)\n", process_name, exit_code); // message 출력
  thread_exit ();
  // end
}

pid_t
exec (const char *cmd_line)
{

}

int
wait (pid_t pid)
{

}

bool
create (const char *file, unsigned initial_size)
{

}

bool
remove (const char *file)
{

}

int
open (const char *file)
{

}

int
filesize (int fd)
{

}

int
read (int fd, void *buffer, unsigned size)
{

}

int
write (int fd, const void *buffer, unsigned size)
{

}

void
seek (int fd, unsigned position)
{

}

unsigned
tell (int fd)
{

}

void
close (int fd)
{

}

void
check_valid_addr (void *addr)
{
  //if ((addr == NULL) || (is_user_vaddr (addr) == false) || ())
}
// end
