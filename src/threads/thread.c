#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

// Alarm Clock - pintos 1
static struct list sleep_list; // sleep 상태의 threads list

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

// Advanced Scheduler - pintos 1
int load_avg; // 시스템의 평균 부하 

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  // Alarm Clock - pintos 1
  list_init (&sleep_list); // sleep_list에 추가
  // end

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  // Advanced Scheduler - pintos 1
  load_avg = 0; // load_avg 값 초기화
  // end

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  // Priority Scheduling - pintos 1
  check_priority_switch(); // priority switch 확인
  // end

#ifdef USERPROG
  t->exit_status = -1;
  t->is_load = false;

  t->parent = thread_current ();
  list_push_back (&t->parent->child_list, &t->child_elem);

  sema_init (&t->sema_load, 0);
  sema_init (&t->sema_exit, 0);
  sema_init (&t->sema_wait, 0);

  t->fd_table = palloc_get_page (0);
  if (t->fd_table == NULL)
  {
    palloc_free_page (t->fd_table);
    return TID_ERROR;
  }
  t->fd_max = 2;
#endif

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t)); // 유효한 thread인지 확인

  old_level = intr_disable (); // 현재 interrupt를 저장하고 비활성화
  ASSERT (t->status == THREAD_BLOCKED); // block 상태의 thread인지 확인

  // original code
  // list_push_back (&ready_list, &t->elem);

  // Priority Scheduling - pintos 1
  list_insert_ordered (&ready_list, &t->elem, compare_priority, NULL); // ready_list에 넣어줄 때 정렬
  // end

  t->status = THREAD_READY; // thread 상태를 ready로 전환
  intr_set_level (old_level); // interrupt 복원
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  // System Calls - pintos 2
  process_exit (); // process 종료

  struct thread *current_thread = thread_current ();
  struct thread *t;
  struct list_elem *elem;
  
  sema_up (&current_thread->sema_wait); // 종료한 thread(process)의 sema_wait를 up하여 exit 완료 알림

  for (elem = list_begin (&current_thread->child_list); elem != list_end (&current_thread->child_list); elem = list_next (elem))
  {
    t = list_entry (elem, struct thread, child_elem);
    sema_up (&t->sema_exit); // 종료한 thread(process)의 child의 sema_exit을 up하여 orphan이 되지 않도록 보장
  }

  sema_down (&current_thread->sema_exit); // parent가 exit status를 저장할 때까지 대기
  // end
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current ()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current (); // 현재 실행 중인 thread
  enum intr_level old_level;
  
  ASSERT (!intr_context ()); // interrupt 처리 중인지 확인

  old_level = intr_disable (); // 현재 interrupt를 저장하고 비활성화
  if (cur != idle_thread) // 현재 실행 중인 thread가 idle thread가 아닌 경우
  {
    // original code
    // list_push_back (&ready_list, &cur->elem);

    // Priority Scheduling - pintos 1
    list_insert_ordered (&ready_list, &cur->elem, compare_priority, NULL); // ready_list에 넣어줄 때 정렬
    // end
  }
  cur->status = THREAD_READY; // thread의 상태를 ready로 전환(현재 실행 중인 thread가 CPU 양보)
  schedule (); // scheduler를 실행하여 다른 thread 실행
  intr_set_level (old_level); // interrupt 복원
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  // Advanced Scheduler - pintos 1
  if (thread_mlfqs) {
    return;
  }
  // end

  thread_current ()->original_priority = new_priority; // 현재 실행 중인 thread의 원래 priority를 새 priority로 변경

  // Priority Scheduling - pintos 1
  recalculate_priority (); // priority 재계산
  check_priority_switch (); // priority switch 확인
  // end
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
  enum intr_level old_level = intr_disable (); // 현재 interrupt를 저장하고 비활성화
  struct thread *current_thread = thread_current (); // 현재 실행 중인 thread
  current_thread->nice = nice; // 현재 실행 중인 thread의 nice 값 설정
  mlfqs_update_priority (current_thread); // 현재 실행 중인 thread의 priority 설정
  if (current_thread != idle_thread) // 현재 실행 중인 thread가 idle thread가 아닌 경우
  {
    check_priority_switch (); // priority switch 확인
  }
  intr_set_level (old_level); // interrupt 복원
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
    enum intr_level old_level = intr_disable (); // 현재 interrupt를 저장하고 비활성화
    int current_nice_value = thread_current ()->nice; // 현재 실행 중인 thread의 nice 값
    intr_set_level (old_level); // interrupt 복원
    return current_nice_value;       
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{ 
  enum intr_level old_level = intr_disable (); // 현재 interrupt를 저장하고 비활성화
  int load_avg_value = convert_to_int_nearest (multiply_int_fixed_point (load_avg, 100)); // load_avg 값
  intr_set_level (old_level); // interrupt 복원
  return load_avg_value;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  enum intr_level old_level = intr_disable (); // 현재 interrupt를 저장하고 비활성화
  int recent_cpu_value = convert_to_int_nearest (multiply_int_fixed_point (thread_current ()->recent_cpu, 100)); // 현재 실행 중인 thread의 recent_cpu 값
  intr_set_level (old_level); // interrupt 복원
  return recent_cpu_value;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;

  // Priority Scheduling - pintos 1
  t->original_priority = priority; // thread의 원래 priority 변경
  t->waiting_lock = NULL; // thread가 기다리고 있는 lock 초기화
  list_init (&t->donations); // donation_list 초기화
  // end

  // Advanced Scheduler - pintos 1
  t->nice = 0; // nice 값 초기화
  t->recent_cpu = 0; // recent_cpu 값 초기화
  // end

#ifdef USERPROG
  list_init (&t->child_list);
#endif
#ifdef VM
  list_init (&t->mmap_file_list);
  t->mmap_max = 0;
#endif

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);



// Alarm Clock - pintos 1
void
thread_sleep (int64_t ticks)
{
  enum intr_level old_level = intr_disable (); // 현재 interrupt를 저장하고 비활성화
  struct thread *current_thread = thread_current (); // 현재 실행 중인 thread

  ASSERT (current_thread != idle_thread) // 현재 실행 중인 thread가 idle thread인지 확인(idle thread는 sleep 상태가 되어서는 안 됨)
  
  current_thread->alarmTicks = ticks; // thread가 깨어나야 할 ticks을 저장

  list_push_back (&sleep_list, &current_thread->elem); // sleep_list에 추가
  thread_block (); // thread를 block 상태로 전환
  intr_set_level (old_level); // interrupt 복원
}

void
thread_awake (int64_t ticks)
{
  struct list_elem *next_elem = list_begin (&sleep_list); // sleep_list의 첫 번째 요소

  while (next_elem != list_end (&sleep_list)) // sleep_list의 끝까지 순회
  { 
    struct thread *current_thread = list_entry (next_elem, struct thread, elem); // next_elem이 가리키는 sleep_list 요소에 해당하는 thread의 주소를 current_thread에 저장
    
    if (current_thread->alarmTicks <= ticks) // current_thread가 깨어날 시간이 되었는지 확인하고, 깨어날 시간이 된 경우
    {
      next_elem = list_remove (next_elem); // list에서 해당 thread를 제거하고 다음 요소로 이동
      thread_unblock (current_thread); // thread 상태를 block에서 ready로 전환
    }
    else 
    {
      next_elem = list_next (next_elem); // 아직 깨울 시간 아닌 경우 다음 요소로 이동
    }
  }
}
// end


// Priority Scheduling - pintos 1
// thread 간의 priority 비교
bool
compare_priority (const struct list_elem *temp_1, const struct list_elem *temp_2, void *aux UNUSED)
{
  return list_entry (temp_1, struct thread, elem)->priority > list_entry (temp_2, struct thread, elem)->priority;
}

// donation_list priority 비교
bool
compare_donate_priority (const struct list_elem *temp_1, const struct list_elem *temp_2, void *aux UNUSED) 
{
  return list_entry (temp_1, struct thread, donation_elem)->priority > list_entry (temp_2, struct thread, donation_elem)->priority;
}

// 현재 실행 중인 thread와 ready_list의 가장 앞에 있는 thread를 비교하여 후자가 더 높은 priority를 가지면 현재 실행 중인 thread가 CPU 양보
void
check_priority_switch (void) 
{
  if (!list_empty (&ready_list)) // ready_list가 비어있지 않은 경우
  {
      struct thread *current = thread_current (); // 현재 실행 중인 thread
      struct thread *highest_priority_thread = list_entry (list_front (&ready_list), struct thread, elem); // ready_list의 가장 앞에 있는 thread, 즉 ready 상태의 thread 중 priority가 가장 높은 thread

      if ((!intr_context ()) && (current->priority < highest_priority_thread->priority)) // 현재 thread의 priority가 ready_list의 가장 앞에 있는 thread보다 낮은 경우
      {
        thread_yield (); // CPU 양보
      }
  }
}

// priority 기부
void
apply_priority_donation (void) 
{  
  struct thread *current_thread = thread_current (); // 현재 실행 중인 thread
  struct thread *lock_holder;

  // 최대 8번의 priority 기부 시도
  for (int i = 0; i < 8; i++)
  {
    // thread가 더 이상 기다리고 있는 lock이 없는 경우 종료
    if (current_thread->waiting_lock == NULL) 
    {
      break;
    }

    // lock을 소유한 thread에게 priority 기부
    lock_holder = current_thread->waiting_lock->holder;
    if (lock_holder->priority < current_thread->priority)
    {
      lock_holder->priority = current_thread->priority;
    }
        
    // priority를 기부 받은 thread, 즉 lock을 소유한 thread를 현재 thread로 업데이트
    current_thread = lock_holder;
  }
}

// donation_list에서 thread 제거
void
clear_donations_for_lock (struct lock *lock) 
{
  struct list_elem *elem; // donation_list를 순회할 때 사용할 요소
  struct thread *current_thread = thread_current (); // 현재 실행 중인 thread
  struct thread *donating_thread;

  // donation_list의 첫 번째부터 마지막까지 순회
  for (elem = list_begin (&current_thread->donations); elem != list_end (&current_thread->donations); elem = list_next (elem)) 
  {
    donating_thread = list_entry (elem, struct thread, donation_elem); // donation_list에 있는 thread
    
    if (donating_thread->waiting_lock == lock) // 해당 thread가 현재 해제하려는 lock을 기다리고 있는 경우
    { 
      list_remove (elem); // donation_list에서 해당 thread 제거
    }
  }
}

// priority 재계산
void
recalculate_priority (void)
{
  struct thread *current_thread = thread_current (); // 현재 실행 중인 thread
  struct thread *highest_priority_thread;

  current_thread->priority = current_thread->original_priority; // thread의 priority를 원래 priority로 초기화

  if (!list_empty (&current_thread->donations)) // donation_list가 비어있지 않은 경우
  {
    list_sort (&current_thread->donations, compare_donate_priority, NULL); // donation_list를 priority에 따라 정렬

    highest_priority_thread = list_entry (list_front (&current_thread->donations), struct thread, donation_elem); // donation_list에서 가장 높은 priority를 가진 thread
    
    if (highest_priority_thread->priority > current_thread->priority) // 현재 thread의 priority가 donation_list의 가장 높은 priority보다 높은 경우
    {
      current_thread->priority = highest_priority_thread->priority; // priority 조정
    }
  }
}
// end


// Advanced Scheduler - pintos 1
// thread의 priority 계산
void
mlfqs_update_priority (struct thread *current_thread) 
{
    int temp;
    int priority;

    if (current_thread == idle_thread) // idle_thread 제외
    {
      return;
    }

    temp = subtract_fixed_point (PRI_MAX, convert_to_int_nearest (divide_int_fixed_point (current_thread->recent_cpu, 4)));
    priority = subtract_fixed_point (temp, multiply_int_fixed_point (current_thread->nice, 2));

    if (priority > PRI_MAX)
    {
      current_thread->priority = PRI_MAX;
    }
    else if (priority < PRI_MIN)
    {
      current_thread->priority = PRI_MIN;
    }
    else
    {
      current_thread->priority = priority;
    }

    return;
}

// thread의 최근 CPU 사용량 계산
void
mlfqs_update_cpu_time (struct thread *current_thread) 
{
    if (current_thread == idle_thread) // idle_thread 제외
    {
      return;
    }

    current_thread->recent_cpu = add_int_fixed_point (multiply_fixed_point (divide_fixed_point (multiply_int_fixed_point (load_avg, 2), add_int_fixed_point (multiply_int_fixed_point (load_avg, 2), 1)), current_thread->recent_cpu), current_thread->nice);
}

// 시스템의 평균 부하 계산
void
mlfqs_update_load_average (void)
{
    int ready_threads;

    if (thread_current () == idle_thread)
    {
        ready_threads = list_size (&ready_list);
    }
    else
    {
        ready_threads = list_size (&ready_list) + 1;
    }

    load_avg = add_fixed_point (
        multiply_fixed_point (
            divide_fixed_point (convert_to_fixed_point (59), convert_to_fixed_point (60)), load_avg), 
        multiply_int_fixed_point (
            divide_fixed_point (convert_to_fixed_point (1), convert_to_fixed_point (60)), ready_threads));
}

// 현재 실행 중인 thread의 recent_cpu를 tick마다 1씩 증가
void
mlfqs_increment_cpu_time (void)
{
    if (thread_current () != idle_thread) // idle_thread 제외
    { 
        thread_current ()->recent_cpu = add_int_fixed_point (thread_current ()->recent_cpu, 1); // 1 증가
    }
}

// 모든 thread의 recent_cpu 값 재계산
void
mlfqs_update_all_cpu_usages (void) 
{
    for (struct list_elem *element = list_begin (&all_list); element != list_end (&all_list); element = list_next (element)) // all_list에 있는 모든 thread를 순회하면서 recent_cpu 값 업데이트
    {
        mlfqs_update_cpu_time (list_entry (element, struct thread, allelem)); // 각 thread의 recent_cpu 값 계산
    }
}

// 모든 thread의 priority 값 재계산
void
mlfqs_update_all_priorities (void)
{
    for (struct list_elem *element = list_begin (&all_list); element != list_end (&all_list); element = list_next (element)) // all_list에 있는 모든 thread를 순회하면서 priority 값 업데이트
    {
        mlfqs_update_priority (list_entry(element, struct thread, allelem)); // 각 thread의 priority 값 계산
    }
}


// Advanced Scheduler - pintos 1
// 정수를 고정 소수점으로 변환
int
convert_to_fixed_point (int n)
{
    return n * FIXED_POINT_SHIFT;
}

// 고정 소수점을 정수로 변환 (소수점 이하 내림)
int
convert_to_int (int x)
{
    return x / FIXED_POINT_SHIFT;
}

// 고정 소수점을 정수로 변환 (반올림)
int
convert_to_int_nearest (int x)
{
    if (x >= 0) 
        return (x + FIXED_POINT_SHIFT / 2) / FIXED_POINT_SHIFT;
    else 
        return (x - FIXED_POINT_SHIFT / 2) / FIXED_POINT_SHIFT;
}

// 두 고정 소수점 값을 더함
int
add_fixed_point (int x, int y)
{
    return x + y;
}

// 두 고정 소수점 값을 뺌
int
subtract_fixed_point (int x, int y)
{
    return x - y;
}

// 고정 소수점에 정수를 더함
int
add_int_fixed_point (int x, int n)
{
    return x + n * FIXED_POINT_SHIFT;
}

// 고정 소수점에서 정수를 뺌
int
subtract_int_fixed_point (int x, int n)
{
    return x - n * FIXED_POINT_SHIFT;
}

// 두 고정 소수점 값을 곱함
int
multiply_fixed_point (int x, int y)
{
    return ((int64_t) x) * y / FIXED_POINT_SHIFT;
}

// 고정 소수점에 정수를 곱함
int
multiply_int_fixed_point (int x, int n)
{
    return x * n;
}

// 두 고정 소수점 값을 나눔
int
divide_fixed_point (int x, int y)
{
    return ((int64_t) x) * FIXED_POINT_SHIFT / y;
}

// 고정 소수점을 정수로 나눔
int
divide_int_fixed_point (int x, int n)
{
    return x / n;
}
// end