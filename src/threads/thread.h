#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "lib/kernel/hash.h"
#include "threads/synch.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* 현재의 Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    // Alarm Clock - pintos 1
    int64_t alarmTicks; // 깨어나야 하는 ticks 값

    // Priority Scheduling - pintos 1
    int original_priority; // 원래의 priority 값
    struct lock *waiting_lock; // thread가 현재 받으려고 대기하고 있는 lock
    struct list donations; // 자신에게 priority를 준 thread list
    struct list_elem donation_elem; // donation_list의 요소

    // Advanced Scheduler - pintos 1
    int nice; // thread의 nice 값
    int recent_cpu; // thread의 최근 CPU 사용량

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                   /* Page directory. */

    int exit_status;                     // exit 상태
    bool is_load;                        // load 상태를 나타내는 flag
    
    struct thread *parent;               // parent thread에 대한 포인터
    struct list child_list;              // child thread 목록
    struct list_elem child_elem;         // child 목록의 요소로서의 thread

    struct semaphore sema_load;          // load 완료 시 사용되는 semaphore
    struct semaphore sema_exit;          // child 종료 시 사용되는 semaphore
    struct semaphore sema_wait;          // parent가 대기할 때 사용되는 semaphore

    struct file **fd_table;              // file descriptor table
    struct file *current_file;           // 현재 실행 중인 file
    int fd_max;                          // 최대 file descriptor 수
#endif
#ifdef VM
    struct hash vm;
    void *esp;

    struct list mmap_file_list;
    int map_max;
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int nice UNUSED);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

// Alarm Clock - pintos 1
void thread_sleep (int64_t ticks);
void thread_awake (int64_t ticks);

// Priority Scheduling - pintos 1
bool compare_priority (const struct list_elem *temp_1, const struct list_elem *temp_2, void *aux);
bool compare_donate_priority (const struct list_elem *temp_1, const struct list_elem *temp_2, void *aux);
void check_priority_switch (void);
void apply_priority_donation (void);
void clear_donations_for_lock (struct lock *lock);
void recalculate_priority (void);

// Advanced Scheduler - pintos 1
void mlfqs_update_priority (struct thread *current_thread);
void mlfqs_update_cpu_time (struct thread *current_thread);
void mlfqs_update_load_average (void);
void mlfqs_increment_cpu_time (void);
void mlfqs_update_all_cpu_usages (void);
void mlfqs_update_all_priorities (void);



#define FIXED_POINT_SHIFT (1 << 14) // 소수 부분을 위한 고정 소수점 변환값
#define MAX_INT ((1 << 31) - 1)
#define MIN_INT (-(1 << 31))

/* 고정 소수점 연산 함수 선언 */
int convert_to_fixed_point (int n);          // 정수를 고정 소수점으로 변환
int convert_to_int (int x);                  // 고정 소수점을 정수로 변환 (내림)
int convert_to_int_nearest (int x);          // 고정 소수점을 정수로 변환 (반올림)
int add_fixed_point (int x, int y);          // 두 고정 소수점을 더함
int subtract_fixed_point (int x, int y);     // 두 고정 소수점을 뺌
int add_int_fixed_point (int x, int n);      // 고정 소수점에 정수를 더함
int subtract_int_fixed_point (int x, int n); // 고정 소수점에서 정수를 뺌
int multiply_fixed_point (int x, int y);     // 두 고정 소수점을 곱함
int multiply_int_fixed_point (int x, int n); // 고정 소수점에 정수를 곱함
int divide_fixed_point (int x, int y);       // 두 고정 소수점을 나눔
int divide_int_fixed_point (int x, int n);   // 고정 소수점을 정수로 나눔

#endif /* threads/thread.h */