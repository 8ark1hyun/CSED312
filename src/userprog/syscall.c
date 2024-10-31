#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
struct lock file_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// System Calls - pintos 2
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_valid_addr ((void *)(f->esp));

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
check_valid_addr (void *addr)
{
  if ((addr == NULL) || (is_user_vaddr (addr) == false) || (pagedir_get_page (thread_current ()->pagedir, addr) == NULL))
  {
    exit (-1);
  }
}

void
get_argument (void *esp, int *argv, int num)
{
  int i;
  for (i = 0; i < num; i++)
  {
    check_valid_addr (esp + 4 * i);
    argv[i] = *(int *)(esp + 4 * i);
  }
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
  struct thread *child;
  pid_t pid;

  pid = process_execute (cmd_line);
  if (pid == -1)
  {
    return -1;
  }  

  child = get_child (pid);
  sema_down (&(child->sema_load));

  if (child->is_load)
  {
    return pid;
  }
  else
  {
    return -1;
  }
}

int
wait (pid_t pid)
{
  return process_wait (pid);
}

bool
create (const char *file, unsigned initial_size)
{
  if (file == NULL)
  {
    exit (-1);
  }

  return filesys_create (file, initial_size);
}

bool
remove (const char *file)
{
  return filesys_remove (file);
}

int
open (const char *file)
{
  int fd;
  struct file *f;
  struct thread *t;

  lock_acquire (&file_lock);
  f = filesys_open (file);

  if (f == NULL)
  {
    lock_release (&file_lock);
    return -1;
  }

  // Denying Writes to Executables - pintos 2
  if (!strcmp (thread_current ()->name, file))
  {
    file_deny_write (f);
  }
  // end

  t = thread_current ();
  fd = t->fd_max;

  t->fd_table[fd] = f;
  t->fd_max++;

  lock_release (&file_lock);
  return fd;
}

int
filesize (int fd)
{
  struct file *f;
  
  lock_acquire (&file_lock);

  if (fd < thread_current ()->fd_max)
  {
    f = thread_current ()->fd_table[fd];
    lock_release (&file_lock);
    return file_length (f);
  }
  else
  {
    lock_release (&file_lock);
    return -1;
  }
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
  struct file *f;

  lock_acquire (&file_lock);

  if (fd < thread_current ()->fd_max)
  {
    f = thread_current ()->fd_table[fd];
    file_seek (f, position);
    lock_release (&file_lock);
  }
  else
  {
    lock_release (&file_lock);
  }
}

unsigned
tell (int fd)
{
  struct file *f;

  if (fd < thread_current ()->fd_max)
  {
    f = thread_current ()->fd_table[fd];
    return file_tell (f);
  }
  else
  {
    return -1;
  }
}

void
close (int fd)
{
  struct file *f;

  if (fd < thread_current ()->fd_max)
  {
    f = thread_current ()->fd_table[fd];
    file_close (f);
    thread_current ()->fd_table[fd] == NULL;
  }
}
// end
