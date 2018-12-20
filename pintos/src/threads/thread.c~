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
#include "threads/malloc.h"
#include "devices/timer.h"
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
/** Properties for mlfqs **/
  /* The minimum current thread */
  static struct thread * thread_min_mlfqs;
  /* The load average needed in the bsd scheduler */
  static fixed_point load_avg;
  /* The first constant coefficient in load average equation. */
  static fixed_point load_coeff_1 = fixed_point_div(
              integer_to_fixed_point(59),
              integer_to_fixed_point(60));
  /* The second constant coefficient in load average equation. */
  static fixed_point load_coeff_2 = fixed_point_div(
              integer_to_fixed_point(1),
              integer_to_fixed_point(60));
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
static void update_priority(struct thread * t, void * aux);
static void update_recent_cpu(struct thread * t, void * aux);
static void list_reorder (struct list_elem *,
                  list_less_func *, void *aux);

static inline bool
is_head (struct list_elem *elem)
{
  return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* Returns true if ELEM is an interior element,
   false otherwise. */
static inline bool
is_interior (struct list_elem *elem)
{
  return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* Returns true if ELEM is a tail, false otherwise. */
static inline bool
is_tail (struct list_elem *elem)
{
  return elem != NULL && elem->prev != NULL && elem->next == NULL;
}
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
  load_avg = 0;
  thread_min_mlfqs = NULL;
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

 thread_ticks++;
 
if (thread_mlfqs) {
    int64_t ticks_timer = timer_ticks ();
    if (t != idle_thread) {
      t->recent_cpu += integer_to_fixed_point(1);
    }
    if (ticks_timer % TIMER_FREQ == 0) {
      load_avg = calculate_load_avg();
      thread_foreach(&update_recent_cpu, NULL);
    }
    if (ticks_timer % 4 == 0) {
      thread_foreach(&update_priority, NULL);
      if (!list_empty(&ready_list)) {
        thread_min_mlfqs = list_entry(list_min(&ready_list, &priority_greater_func, NULL), struct thread, elem);
      }
    }
    if (thread_ticks % TIME_SLICE == 0) {
      intr_yield_on_return ();
    }
  }
  if (!list_empty(&ready_list)) {
    struct thread * top_t;
    if (thread_mlfqs) {
      top_t = thread_min_mlfqs;
    } else {
      top_t = list_entry(list_front(&ready_list), struct thread, elem);
    }
    if (t->priority < top_t->priority) {
      intr_yield_on_return ();
    }
  }
}

void thread_swap_to_highest_pri(void) {
  enum intr_level old_level;
  bool inter_off = true;
  old_level = intr_disable ();
  if (!list_empty(&ready_list)) {
    struct thread * top_t;
    if (thread_mlfqs) {
      top_t = thread_min_mlfqs;
    } else {
      top_t = list_entry(list_front(&ready_list), struct thread, elem);
    }
    if (thread_current ()->priority < top_t->priority) {
      inter_off = false;
      intr_set_level (old_level);
      if (intr_context()) {
        intr_yield_on_return ();
      } else {
        thread_yield ();
      }
    }
  }
  if (inter_off) {
    intr_set_level (old_level);
  }
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
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

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
  intr_set_level (old_level);

  if (thread_mlfqs) {
    if (thread_current () != initial_thread) {
      t->nice = thread_current ()->nice;
      t->recent_cpu = calculate_recent_cpu(thread_current ());
      t->priority = calculate_priority(t);
    }
  }
  #ifdef USERPROG
  list_push_back(&thread_current ()->children, &t->parent_elem);
  #endif
  /* Add to run queue. */
  thread_unblock (t);
  thread_swap_to_highest_pri();
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

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  if (thread_mlfqs) {
    list_push_back(&ready_list, &t->elem);
    if (thread_min_mlfqs == NULL || priority_greater_func(&t->elem, &thread_min_mlfqs->elem, NULL)) {
      thread_min_mlfqs = t;
    }
  } else {
    list_insert_ordered(&ready_list, &t->elem, &priority_greater_func, NULL);
  }
  t->status = THREAD_READY;
  intr_set_level (old_level);
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
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) {
    if (thread_mlfqs) {
      list_push_back(&ready_list, &cur->elem);
      if (thread_min_mlfqs == NULL || priority_greater_func(&cur->elem, &thread_min_mlfqs->elem, NULL)) {
        thread_min_mlfqs = cur;
      }
    } else {
  	 list_insert_ordered(&ready_list, &cur->elem, &priority_greater_func, NULL);
    }
  }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;
  struct thread *t;		
  ASSERT (intr_get_level () == INTR_OFF);
  if (!list_empty(&all_list)) {
    for (e = list_begin (&all_list); e != list_end (&all_list);
         e = list_next (e))
      {
        t = list_entry (e, struct thread, allelem);
        func (t, aux);
      }
  }
}
/*
* Sets the current thread priority according to its base priority or its highest donater priority.
*/
void get_donated_priority(struct thread * t, int donated_pri) {
  if (!thread_mlfqs) {
    if (t == NULL) {
      return;
    }
    enum intr_level old_level;
    old_level = intr_disable ();
  	t->priority = t->base_priority;
    if (donated_pri > t->priority) {
      t->priority = donated_pri;
    }
  	struct list_elem * e;
  	if (!list_empty(&t->locks)) {
  		for (e = list_begin (&t->locks); e != list_end (&t->locks);
  	           e = list_next (e))
          {
  	    	struct lock *l = list_entry(e, struct lock, elem);
  	    	if (!list_empty(&l->semaphore.waiters)) {
  	    		struct thread * lt = list_entry(list_front(&l->semaphore.waiters), struct thread, elem);
  	    		if (lt->priority > t->priority) {
  	    			t->priority = lt->priority;
  	    		}
  	    	}
      	}
  	}
    struct lock * l = t->blocked_on_lock;
    struct thread * lt = t;
    if (t != thread_current ()) {
      list_reorder(&t->elem, &priority_greater_func, NULL);
    }
    while (l != NULL) {
      if (l->holder->priority < lt->priority) {
        l->holder->priority = lt->priority;
        list_reorder(&l->holder->elem, &priority_greater_func, NULL);
        lt = l->holder;
        l = lt->blocked_on_lock;
      } else {
        l = NULL;
      }
    }
    intr_set_level(old_level);
  }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  if (!thread_mlfqs) {
    enum intr_level old_level;
    ASSERT (!intr_context ());
    old_level = intr_disable ();
    thread_current ()->base_priority = new_priority;
    get_donated_priority(thread_current (), new_priority);
    intr_set_level(old_level);
    thread_swap_to_highest_pri();
  }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  if (thread_mlfqs) {
    enum intr_level old_level;
    ASSERT (!intr_context ());
    ASSERT(-20 <= nice && nice <= 20);
    old_level = intr_disable();
    thread_current ()->nice = nice;
    thread_current ()->recent_cpu = calculate_recent_cpu(thread_current ());
    thread_current ()->priority = calculate_priority(thread_current ());
    intr_set_level(old_level);
    thread_swap_to_highest_pri();
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  return round_fixed_point_number_to_integer(100 * load_avg);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return round_fixed_point_number_to_integer( thread_current()->recent_cpu * 100);
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
/*
* Returns true if a is greater than b.
* The comparison function to be given to the list.
*/
bool priority_greater_func (const struct list_elem *a,const struct list_elem *b, void * aux UNUSED) {
  struct thread * t1 = list_entry (a, struct thread, elem);
  struct thread * t2 = list_entry (b, struct thread, elem);
  return t1->priority > t2->priority;
}
/*
  calculate the priority according to recent_cpu and nice values and return the value clamped to a suitable range.
*/
int calculate_priority(struct thread * t) {
  int temp_prio = PRI_MAX -
                fixed_point_to_integer(t->recent_cpu / 4) -
                t->nice * 2;
  if (temp_prio > PRI_MAX) {
    temp_prio = PRI_MAX;
  } else if (temp_prio < PRI_MIN) {
    temp_prio = PRI_MIN;
  }
  return temp_prio;
}
/*
  Calculates recent cpu and returns the calculated value.
*/
fixed_point calculate_recent_cpu(struct thread * t) {
  fixed_point load = 2 * load_avg;
  fixed_point coeff = fixed_point_div(load, load + integer_to_fixed_point(1));
  return fixed_point_mul(coeff, t->recent_cpu) + integer_to_fixed_point(t->nice);
}
/*
  Calculates the load average and returns the calculated value.
*/
fixed_point calculate_load_avg(void) {
  ASSERT (intr_get_level () == INTR_OFF);
  int ready_size = list_size(&ready_list);
  if (thread_current () != idle_thread) {
    ready_size++;
  }
  return fixed_point_mul(
          load_coeff_1,
          load_avg)
        +
            (load_coeff_2
            * ready_size);
}
static void update_priority(struct thread * t, void * aux UNUSED) {
  if (t != idle_thread) {
    t->priority = calculate_priority(t);
  }
}
static void update_recent_cpu(struct thread * t, void * aux UNUSED) {
  if (t != idle_thread) {
    t->recent_cpu = calculate_recent_cpu(t);
  }
}
/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
    if (thread_mlfqs) {
    t->nice = 0;
    t->recent_cpu = 0;
    t->priority = PRI_MAX;
  } else {
    t->priority = priority;
    t->base_priority = priority;
    t->blocked_on_lock = NULL;
  }
  list_init (&t->locks);
  #ifdef USERPROG
    list_init (&t->children);
    sema_init (&t->finished_flag, 0);
    sema_init (&t->allowed_finish, 0);
    t->ret_status = -1;
    t->fd = 2;
    list_init(&t->file_elems);
  #endif
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);
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

/* Reorders ELEM in the proper position in its LIST, which must be
   sorted according to LESS given auxiliary data AUX.
   Runs in O(n) average case in the number of elements in LIST. */
void
list_reorder (struct list_elem *elem,
                     list_less_func *less, void *aux UNUSED)
{
  struct list_elem *e;
  bool back_dir = true;
  ASSERT (elem != NULL);
  ASSERT (less != NULL);
  ASSERT (is_interior(elem));
  e = elem->prev;
  if (is_head(e) || !less(elem, e, aux)) {
    e = elem->next;
    back_dir = false;
    if (is_tail(e)) {
      return;
    }
  }
  list_remove(elem);
  if (back_dir) {
    while (less(elem, e, aux) && !is_head(e)) {
      e = e->prev;
    }
    e = e->next;
  } else {
    while (less(e, elem, aux) && !is_tail(e)) {
      e = e->next;
    }
  }
  list_insert(e, elem);
}

struct thread *get_thread_from_tid (tid_t tid) {
    struct list_elem *e;
    struct thread *t;
    for (e = list_begin (&all_list); e != list_end (&all_list);
         e = list_next (e))
      {
        t = list_entry (e, struct thread, allelem);
        if (t->tid == tid) {
          return t;
        }
      }
      return NULL;
}
/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
   if (list_empty (&ready_list)) {
    return idle_thread;
  } else {
    if (thread_mlfqs) {
      struct thread * top_waiter_thread = thread_min_mlfqs;
      list_remove(&thread_min_mlfqs->elem);
      if (list_empty(&ready_list)) {
        thread_min_mlfqs = NULL;
      } else {
        thread_min_mlfqs = list_entry(list_min(&ready_list, &priority_greater_func, NULL), struct thread, elem);
      }
      return top_waiter_thread;
    } else {
      return list_entry (list_pop_front (&ready_list), struct thread, elem);
    }
  }
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
