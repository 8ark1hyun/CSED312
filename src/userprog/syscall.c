#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// System Calls - pintos 2
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // 주소 유효성 확인

  int argv[3];
  switch (*(uint32_t *)(f->esp))
  {
    case SYS_HALT:
      halt ();
      break;
    case SYS_EXIT:
      get_argument (f->esp + 4, argv, 1);
      exit ((int)argv[0]);
      break;
    case SYS_EXEC:
      get_argument (f->esp + 4, argv, 1);
      f->eax = exec ((const char *)argv[0]);
      break;
    case SYS_WAIT:
      get_argument (f->esp + 4, argv, 1);
      f->eax = wait ((pid_t)argv[0]);
      break;
    case SYS_CREATE:
      get_argument (f->esp + 4, argv, 2);
      f->eax = create ((const char *)argv[0], (unsigned)argv[1]);
      break;
    case SYS_REMOVE:
      get_argument (f->esp + 4, argv, 1);
      f->eax = remove ((const char *)argv[0]);
      break;
    case SYS_OPEN:
      get_argument (f->esp + 4, argv, 1);
      f->eax = open ((const char *)argv[0]);
      break;
    case SYS_FILESIZE:
      get_argument (f->esp + 4, argv, 1);
      f->eax = filesize ((int)argv[0]);
      break;
    case SYS_READ:
      get_argument (f->esp + 4, argv, 3);
      f->eax = read ((int)argv[0], (void *)argv[1], (unsigned)argv[2]);
      break;
    case SYS_WRITE:
      get_argument (f->esp + 4, argv, 3);
      f->eax = write ((int)argv[0], (const void *)argv[1], (unsigned)argv[2]);
      break;
    case SYS_SEEK:
      get_argument (f->esp + 4, argv, 2);
      seek ((int)argv[0], (unsigned)argv[1]);
      break;
    case SYS_TELL:
      get_argument (f->esp + 4, argv, 1);
      f->eax = tell ((int)argv[0]);
      break;
    case SYS_CLOSE:
      get_argument (f->esp + 4, argv, 1);
      close ((int)argv[0]);
      break;
    default:
      exit (-1);
  }
}

void
get_argument (void *esp, int *arg, int index)
{

}

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
  return process_wait (pid);
}

bool
create (const char *file, unsigned initial_size)
{

}

bool
remove (const char *file)
{
  return filesys_remove (file);
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
