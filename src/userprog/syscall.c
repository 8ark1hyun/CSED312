#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
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

  // Stack Growth - pintos 3
  thread_current ()->esp = f->esp;
  // end

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
      f->eax = exec ((const char *)argv[0], f->esp);
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
      f->eax = read ((int)argv[0], (void *)argv[1], (unsigned)argv[2], f->esp);
      break;
    case SYS_WRITE:
      get_argument (f->esp + 4, argv, 3);
      f->eax = write ((int)argv[0], (const void *)argv[1], (unsigned)argv[2], f->esp);
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
    case SYS_MMAP:
      get_argument (f->esp + 4, argv, 2);
      f->eax = mmap ((int)argv[0], (void *)argv[1]);
      break;
    case SYS_MUNMAP:
      get_argument (f->esp + 4, argv, 1);
      munmap ((mapid_t)argv[0]);
      break;
    default:
      exit (-1);
  }
}

void
check_valid_addr (const void *addr)
{
  /* if ((addr == NULL) || (is_user_vaddr (addr) == false) || (pagedir_get_page (thread_current ()->pagedir, addr) == NULL))
  {
    exit (-1); // 유효한 주소가 아닌 경우 exit
  }*/

  // Lazy Loading & Stack Growth - pintos 3
  if ((addr == NULL) || (is_user_vaddr (addr) == false))
  {
    exit (-1); // 유효한 주소가 아닌 경우 exit
  }
  // end
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
exec (const char *cmd_line, void *esp)
{
  struct thread *child;
  pid_t pid;
  void *cmd_addr = (void *)cmd_line;
  unsigned int length = strlen (cmd_line) + 1;
  size_t bytes;
  struct frame *frame;
  struct page *page;

	check_valid_addr (cmd_line); // 주소 유효성 검증

  // 접근하는 모든 주소에 대응되는 page와 매핑된 frame에 대하여 반복
  while (length > 0)
  {
    page = page_find (pg_round_down (cmd_addr)); // page 단위로 주소 내림
    
    if (page != NULL)
    {
      if (page->is_load == false) // page가 load되지 않은 경우
      {
        if (!fault_handle (page)) // fault 처리 (page를 memory에 load)
        {
          exit (-1); // 실패 시 종료
        }
      }
    }
    else
    {
      uint32_t base = 0xc0000000;
      uint32_t max = 0x800000;
      uint32_t stack_top_addr = base - max;

      // 스택 포인터로부터 일정 범위 내에 있으며, 스택 크기가 최대 한도 내인 경우
      if ((cmd_addr >= (esp - 32)) && ((uint32_t)cmd_addr >= stack_top_addr))
      {
        if (!stack_growth (cmd_addr)) // 스택 확장
        {
          exit (-1); // 실패 시 종료
        }
      }
      else
      {
        exit (-1); // 종료
      }
    }

    frame = frame_find (pg_round_down (cmd_addr)); // page 단위로 주소 내림하여 frame 탐색
    frame->pinning = true; // pinning 설정
    bytes = (length > PGSIZE - pg_ofs (cmd_addr)) ? PGSIZE - pg_ofs (cmd_addr) : length;
    length -= bytes;
    cmd_addr += bytes;
  }

  pid = process_execute (cmd_line); // 새로운 process 생성
  if (pid == -1)
  {
    return -1; // process 생성 실패 시 -1 반환
  }  

  child = get_child (pid); // child process
  sema_down (&child->sema_load); // child process가 load를 완료할 때까지 대기

  length = strlen (cmd_line) + 1;
  cmd_addr = (void *) cmd_line;

  // 접근하는 모든 주소에 대응되는 page와 매핑된 frame에 대하여 반복
  while (length > 0)
  {
    frame = frame_find (pg_round_down (cmd_addr)); // page 단위로 주소 내림하여 frame 탐색
    frame->pinning = false; // pinning 해제
    bytes = (length > PGSIZE - pg_ofs (cmd_addr)) ? PGSIZE - pg_ofs (cmd_addr) : length;
    length -= bytes;
    cmd_addr += bytes;
  }

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
  bool success = false;

  check_valid_addr ((void *)file); // 주소 유효성 검증

  if (file == NULL)
  {
    exit (-1); // file이 NULL인 경우 exit
  }

  lock_acquire (&file_lock);
  success = filesys_create (file, initial_size);
  lock_release (&file_lock);

  return success;
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

  if ((fd > 1) && (fd < thread_current ()->fd_max))
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
read (int fd, void *buffer, unsigned size, void *esp)
{
  struct file* f;
  int bytes = 0;
  unsigned int i;
  void *buffer_addr = buffer;
  unsigned int file_size = size;
  size_t read_bytes;
  struct frame *frame;
  struct page *page;

  for (i = 0; i < size; i++)
  {
    check_valid_addr (buffer + i); // 주소 유효성 검증
  }

  while (file_size > 0)
  {
    page = page_find (pg_round_down (buffer_addr)); // page 단위로 주소 내림

    if (page != NULL)
    {
      if (page->is_load == false) // page가 load되지 않은 경우
      {
        if (!fault_handle (page)) // fault 처리 (page를 memory에 load)
        {
          exit (-1); // 실패 시 종료
        }
      }
    }
    else
    {
      uint32_t base = 0xc0000000;
      uint32_t max = 0x800000;
      uint32_t stack_top_addr = base - max;

      // 스택 포인터로부터 일정 범위 내에 있으며, 스택 크기가 최대 한도 내인 경우
      if ((buffer_addr >= (esp - 32)) && ((uint32_t)buffer_addr >= stack_top_addr))
      {
        if (!stack_growth (buffer_addr)) // 스택 확장
        {
          exit (-1); // 실패 시 종료
        }
      }
      else
      {
        exit (-1); // 종료
      }
    }    

    frame = frame_find (pg_round_down (buffer_addr)); // page 단위로 주소 내림하여 frame 탐색
    frame->pinning = true; // pinning 설정
    read_bytes = (file_size > PGSIZE - pg_ofs (buffer_addr)) ? PGSIZE - pg_ofs (buffer_addr) : file_size;
    file_size -= read_bytes;
    buffer_addr += read_bytes;
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

  file_size = size;
  buffer_addr = buffer;

  // 접근하는 모든 주소에 대응되는 page와 매핑된 frame에 대하여 반복
  while (file_size > 0)
  {
    frame = frame_find (pg_round_down (buffer_addr)); // page 단위로 주소 내림하여 frame 탐색
    frame->pinning = false; // pinning 해제
    read_bytes = (file_size > PGSIZE - pg_ofs (buffer_addr)) ? PGSIZE - pg_ofs (buffer_addr) : file_size;
    file_size -= read_bytes;
    buffer_addr += read_bytes;
  }

  return bytes; // 읽은 bytes 수 반환
}

int
write (int fd, const void *buffer, unsigned size, void *esp)
{
  struct file* f;
  int bytes = 0;
  unsigned int i;
  void *buffer_addr = (void *)buffer;
  unsigned int file_size = size;
  size_t write_bytes;
  struct frame *frame;
  struct page *page;

  for (i = 0; i < size; i++)
  {
    check_valid_addr (buffer + i); // 주소 유효성 검증
  }

  // 접근하는 모든 주소에 대응되는 page와 매핑된 frame에 대하여 반복
  while (file_size > 0)
  {
    page = page_find (pg_round_down (buffer_addr)); // page 단위로 주소 내림

    if (page != NULL)
    {
      if(page->is_load == false) // page가 load되지 않은 경우
      { 
        if (!fault_handle (page)) // fault 처리 (page를 memory에 load)
        { 
          exit (-1); // 실패 시 종료
        }
      }
    }
    else
    {
      uint32_t base = 0xc0000000;
      uint32_t max = 0x800000;
      uint32_t stack_top_addr = base - max;

      // 스택 포인터로부터 일정 범위 내에 있으며, 스택 크기가 최대 한도 내인 경우
      if ((buffer_addr >= (esp - 32)) && ((uint32_t)buffer_addr >= stack_top_addr))
      {
        if (!stack_growth (buffer_addr)) // 스택 확장
        {
          exit (-1); // 실패 시 종료
        }
      }
      else
      {
        exit (-1); // 종료
      }
    } 

    frame = frame_find (pg_round_down (buffer_addr)); // page 단위로 주소 내림하여 frame 탐색
    frame->pinning = true; // pinning 설정
    write_bytes = (file_size > PGSIZE - pg_ofs (buffer_addr)) ? PGSIZE - pg_ofs (buffer_addr) : file_size;
    file_size -= write_bytes;
    buffer_addr += write_bytes;   
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

  file_size = size;
  buffer_addr = buffer;

  // 접근하는 모든 주소에 대응되는 page와 매핑된 frame에 대하여 반복
  while (file_size > 0)
  {
    frame = frame_find (pg_round_down (buffer_addr)); // page 단위로 주소 내림하여 frame 탐색
    frame->pinning = false; // pinning 해제
    write_bytes = (file_size > PGSIZE - pg_ofs (buffer_addr)) ? PGSIZE - pg_ofs (buffer_addr) : file_size;
    file_size -= write_bytes;
    buffer_addr += write_bytes;
  }

  return bytes; // 쓴 bytes 수 반환
}

void
seek (int fd, unsigned position)
{
  struct file *f;

  lock_acquire (&file_lock); // lock 획득

  if ((fd > 1) && (fd < thread_current ()->fd_max))
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

  lock_acquire (&file_lock);
  if ((fd > 1) && (fd < thread_current ()->fd_max))
  {
    f = thread_current ()->fd_table[fd];
    lock_release (&file_lock);
    return file_tell (f);
  }
  else
  {
    lock_release (&file_lock);
    return -1;
  }
}

void
close (int fd)
{
  struct file *f;

  if ((fd > 1) && (fd < thread_current ()->fd_max))
  {
    f = thread_current ()->fd_table[fd];
    file_close (f);
    thread_current ()->fd_table[fd] = NULL; // table에서 file 삭제
  }
}
// end

// Memory Mapped Files - pintos 3
mapid_t
mmap (int fd, void *addr)
{
  int file_size;
  int offset = 0;
  struct mmap_file *mmap_file;
  struct file *f = NULL;
  struct page *page;
  size_t page_read_bytes;
  size_t page_zero_bytes;

  if (is_kernel_vaddr (addr)) // 커널 가상 주소인 경우
  {
    exit (-1); // 종료
  }
  // 유효하지 않은 주소인 경우 (NULL이거나, page의 경계에 맞지 않는 경우)
  if ((addr == NULL) || (pg_ofs (addr) != 0) || ((int) addr % PGSIZE != 0))
  {
    return -1; // -1 반환
  }

  mmap_file = (struct mmap_file *) malloc (sizeof (struct mmap_file)); // mmap_file 구조체 할당
  if (mmap_file == NULL)
  {
    return -1;
  }
  memset (mmap_file, 0, sizeof (struct mmap_file)); // 메모리 초기화

  lock_acquire (&file_lock); // lock 획득
  f = file_reopen (thread_current ()->fd_table[fd]); // file 열기
  file_size = file_length (f); // file 길이
  lock_release (&file_lock); // lock 해제
  if (file_size == 0)
  {
    return -1;
  }

  list_init (&mmap_file->page_list); // mmap_file의 page 목록 초기화

  while (file_size > 0) // file과 매핑된 모든 page에 대해 반복
  {
    if (page_find (addr)) // page 탐색
    {
      return -1;
    }

    page_read_bytes = file_size < PGSIZE ? file_size : PGSIZE; // 읽은 bytes
    page_zero_bytes = PGSIZE - page_read_bytes; // 나머지 bytes

    page = page_allocate (FILE, addr, true, false, offset, page_read_bytes, page_zero_bytes, f); // page 할당
    if (page == NULL)
    {
      return false;
    }
    list_push_back (&mmap_file->page_list, &page->mmap_elem); // page 삽입
    
    addr += PGSIZE;
    offset += PGSIZE;
    file_size -= PGSIZE;
  }

  mmap_file->mapid = thread_current ()->mmap_max++; // 매핑 ID 설정
  list_push_back (&thread_current ()->mmap_file_list, &mmap_file->elem); // mmap_file 삽입
  mmap_file->file = f; // file 설정

  return mmap_file->mapid;
}

void
munmap (mapid_t mapping)
{
  struct mmap_file *mmap_file = NULL;
  struct page *page;
  struct list_elem *e;
  void *addr;

  // 매핑된 file 탐색
  for (e = list_begin (&thread_current ()->mmap_file_list); e != list_end (&thread_current ()->mmap_file_list); e = list_next (e))
  {
    mmap_file = list_entry (e, struct mmap_file, elem);
    if (mmap_file->mapid == mapping)
    {
      break;
    }
  }

  if (mmap_file == NULL)
  {
    return;
  }

  // file과 매핑된 모든 page에 대해 반복
  for (e = list_begin (&mmap_file->page_list); e != list_end (&mmap_file->page_list);)
  {
    page = list_entry (e, struct page, mmap_elem);
    if ((page->is_load == true) && (pagedir_is_dirty (thread_current ()->pagedir, page->addr)))
    {
      lock_acquire (&file_lock); // lock 획득
      file_write_at (page->file, page->addr, page->read_byte, page->offset); // write-back
      lock_release (&file_lock); // lock 해제
      addr = pagedir_get_page (thread_current ()->pagedir, page->addr);
      frame_deallocate (addr); // frame 할당 해제
    }
    page->is_load = false;
    e = list_remove (e);
    page_deallocate (page); // page 할당 해제
  }

  list_remove (&mmap_file->elem);
  free (mmap_file); // mmap_file 구조체 할당 해제
}
// end