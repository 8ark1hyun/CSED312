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
#include "devices/input.h"
#include <string.h>

static void syscall_handler (struct intr_frame *);
struct lock file_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

// System Calls - pintos 2
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_valid_addr ((void *)(f->esp)); // 주소 유효성 검증

  int argv[3];
  switch (*(uint32_t *)(f->esp)) // syscall number에 따라
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
check_valid_addr (const void *addr)
{
  if ((addr == NULL) || (is_user_vaddr (addr) == false) || (pagedir_get_page (thread_current ()->pagedir, addr) == NULL))
  {
    exit (-1); // 유효한 주소가 아닌 경우 exit
  }
}

void
get_argument (void *esp, int *argv, int num)
{
  int i;
  for (i = 0; i < num; i++) // 필요한 argument 개수만큼 가져오기
  {
    check_valid_addr (esp + 4 * i); // 주소 유효성 검증
    argv[i] = *(int *)(esp + 4 * i); // argument 저장
  }
}

void
halt (void)
{
  shutdown_power_off (); // Pintos 종료
}

void
exit (int status)
{
  // Process Termination Messages - pintos 2
  const char *process_name = thread_name (); // process's name
  int exit_code = status; // exit code
  thread_current ()->exit_status = status; // exit status 저장

  printf ("%s: exit(%d)\n", process_name, exit_code); // message 출력
  thread_exit (); // thread 종료
  // end
}

pid_t
exec (const char *cmd_line)
{
  struct thread *child;
  pid_t pid;

	check_valid_addr (cmd_line); // 주소 유효성 검증

  pid = process_execute (cmd_line); // 새로운 process 생성
  if (pid == -1)
  {
    return -1; // process 생성 실패 시 -1 반환
  }  

  child = get_child (pid); // child process
  sema_down (&child->sema_load); // child process가 load를 완료할 때까지 대기

  if (child->is_load)
  {
    return pid; // load 성공 시 pid 반환
  }
  else
  {
    return -1; // load 실패 시 -1 반환
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
  check_valid_addr ((void *)file); // 주소 유효성 검증

  if (file == NULL)
  {
    exit (-1); // file이 NULL인 경우 exit
  }

  return filesys_create (file, initial_size);
}

bool
remove (const char *file)
{
  check_valid_addr ((void *)file); // 주소 유효성 검증

  return filesys_remove (file);
}

int
open (const char *file)
{
  int fd;
  struct file *f;
  struct thread *t;

  check_valid_addr ((void *)file); // 주소 유효성 검증

  lock_acquire (&file_lock); // lock 획득
  f = filesys_open (file);

  if (f == NULL)
  {
    lock_release (&file_lock); // lock 해제
    return -1; // 열고자 하는 file이 없는 경우 -1 반환
  }

  // Denying Writes to Executables - pintos 2
  if (!strcmp (thread_current ()->name, file))
  {
    file_deny_write (f);
  }
  // end

  t = thread_current ();
  fd = t->fd_max;

  t->fd_table[fd] = f; // file descriptor 값에 해당하는 table에 file 저장
  t->fd_max++; // 최대 file descriptor 수 1 증가

  lock_release (&file_lock); // lock 해제
  return fd; // file descriptor 값 반환
}

int
filesize (int fd)
{
  struct file *f;
  
  lock_acquire (&file_lock); // lock 획득

  if (fd < thread_current ()->fd_max)
  {
    f = thread_current ()->fd_table[fd];
    lock_release (&file_lock); // lock 해제
    return file_length (f); // file 길이 반환
  }
  else
  {
    lock_release (&file_lock); // lock 해제
    return -1;
  }
}

int
read (int fd, void *buffer, unsigned size)
{
  struct file* f;
  int bytes = 0;
  unsigned int i;

  for (i = 0; i < size; i++)
  {
    check_valid_addr (buffer + i); // 주소 유효성 검증
  }

  if (fd == 0) // stdin인 경우
  {
    for (i = 0; i < size; i++)
    {
      ((char *)buffer)[i] = input_getc (); // keyboard 입력
      if (((char *)buffer)[i] == '\0')
      {
        break;
      }
      bytes++; // 입력한 크기만큼 bytes 수 증가
    }
  }
  else if ((fd > 1) && (fd < thread_current ()->fd_max))
  {
    f = thread_current ()->fd_table[fd];
    lock_acquire (&file_lock); // lock 획득
    bytes = file_read (f, buffer, size); // file의 buffer를 read
    lock_release (&file_lock); // lock 해제
  }
  else // stdout이거나 존재하지 않는 file인 경우
  {
    return -1; // -1 반환
  }

  return bytes; // 읽은 bytes 수 반환
}

int
write (int fd, const void *buffer, unsigned size)
{
  struct file* f;
  int bytes = 0;
  unsigned i;

  for (i = 0; i < size; i++)
  {
    check_valid_addr (buffer + i); // 주소 유효성 검증
  }

  if (fd == 1) // stdout인 경우
  {
    lock_acquire (&file_lock); // lock 획득
    putbuf (buffer, size); // console의 buffer에 write
    lock_release (&file_lock); // lock 해제

    return bytes; // 쓴 bytes 수 반환
  }
  else if ((fd > 1) && (fd < thread_current ()->fd_max))
  {
    f = thread_current ()->fd_table[fd];
    lock_acquire (&file_lock); // lock 획득
    bytes = file_write (f, buffer, size); // file의 buffer에 write
    lock_release (&file_lock); // lock 해제
  }
  else // stdin이거나 존재하지 않는 file인 경우
  {
    return -1; // -1 반환
  }

  return bytes; // 쓴 bytes 수 반환
}

void
seek (int fd, unsigned position)
{
  struct file *f;

  lock_acquire (&file_lock); // lock 획득

  if (fd < thread_current ()->fd_max)
  {
    f = thread_current ()->fd_table[fd];
    file_seek (f, position);
    lock_release (&file_lock); // lock 해제
  }
  else
  {
    lock_release (&file_lock); // lock 해제
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
    thread_current ()->fd_table[fd] = NULL; // table에서 file 삭제
  }
}
// end
