Design Document for Project 1: Threads
======================================

## Group Members

* Derek Feng <kianex@berkeley.edu>
* Jerry Huang <jerryhuang@berkeley.edu>
* Jaehyun Sim <simjay@berkeley.edu>
* Pei Jie Sim <peijiesim@berkeley.edu>

## Task 1: Efficient Alarm Clock

### Data Structures and Functions
In `threads/thread.h` we are 
* adding `int64_t wake_time` in `struct thread`  to keep track of the wake up time (in ticks) of a thread.
* adding `struct semaphore sema` in `struct thread` to indicate which state (blocked or unblocked) a thread is in.

In `devices/timer.c` we are 
* adding a global variable `struct list sleeping_threads` to store all the threads that are asleep.
* adding a function `list_less_func less_than(struct list_elem *t1, struct list_elem *t2, void *aux)` that compares wake times of two threads and returns true if the first thread has an earlier wake time than the second thread and false otherwise.
```
list_less_func less_than(struct list_elem *t1, struct list_elem *t2, void *aux) {
	thread_1 = list_entry(t1, struct thread, elem);
	thread_2 = list_entry(t2, struct thread, elem);
	return thread_1->wake_time < thread_2->wake_time;	
}
```	
### Algorithms
1. Initialize an empty linked list (call it `sleeping_threads`) in `timer.c` to store the threads that are blocked.
2. In `timer_sleep()`, first check if the argument passed in is valid (positive).
3. The time to wake each thread up is kept track of using the variable `wake_time` which is the result of calling `timer_ticks()` and then adding ticks to it.
4. Insert the current thread into `sleeping_threads` in ascending order of wake times by calling `list_insert_ordered()`, passing in the function `less_than()` that was declared.
5. Interrupts have to be disabled before doing so and re-enabled after the insertion is done.
6. Instead of looping until number of ticks has passed, we block the thread by calling `thread_block()`.
7. To wake threads up, in `timer_interrupt()`, iterate through the list of sleeping threads and call `thread_unblock()` on each of them if `ticks == wake_time`.
8. Any thread that gets unblocked are removed and this step is repeated until `sleeping_threads` is empty.

### Synchronization 
The linked list `sleeping_threads` can potentially be accessed by multiple threads at the same time which may disrupt the order in which threads are inserted based on their wake times. Hence, by disabling interrupts before inserting a thread into the linked list, we can ensure that no two threads access the linked list at once. Interrupts shall be enabled thereafter. When attempting to put a thread to sleep, interrupts cannot be permitted because allowing interrupts may prevent the thread from actually going to sleep. The same has to be done when calling `thread_block()` and `thread_unblock()` so that threads can safely be blocked. On the other hand, the `less_than()` function created has no problem being called by two threads at the same time since no global variables are accessed within the function.

### Rationale
Initially, we thought about creating a priority queue to store the sleeping threads but we realized that inserting threads in non-decreasing order of wake times does the job and although there are advantages and disadvantages in choosing one over the other, we opted for the linked list implementation since it simplifies coding. Inserting threads in non-descending order of wake times makes for efficient accesses because we can avoid searching the entire list for a given blocked thread when trying to unblock it. In terms of complexity, inserting a thread into the linked list takes at most O(n) time, where n is the size of the linked list. Searching for threads to unblock and remove also takes at most O(n) time if all threads have the same wake times.   

## Task 2: Priority Scheduler
### Data Structures and Functions
In `threads/thread.c` we are 
* modifying `static struct thread * next_thread_to_run (void)` so that it handles priority; right now it ignores priority.
* adding a recursive function `thread * findFirstUnblocked (thread* t)` that finds the first thread that can unblock thread t. 
* adding the function `list_less_func greater_priority(struct list_elem t1, struct list_elem t2, void *aux)` for ease of sorting.

In `threads/thread.h` we are modifying the thread struct* modifying `struct thread` 
* to have a base and a effective priority
* to have a list if locks `locksheld` it holds and a variable `lockwanted` for the lock, if any, that thread is waiting on

In `threads/synch.c` we are
* modifying `lock_acquire (lock)` so that when a thread calls the function it gets its `lockWanted` and `lockHeld` set appropriately.

### Algorithms
#### `next_thread_to_run (void)` 
In this function, we first find the thread with the highest priority, and then we check to see if that thread is being blocked by any others. If not, we simply just run the thread and if so, we want to do recursive priority donation. We can find the thread to donate to by calling `findFirstUnblocked` on the highest priority thread. 
#### `bool blocked (thread * t)`
In this function we check `t`'s `lockWanted` to see if `t` `lockWanted` is currently being held by a lock with lower priority than `t`. If so we return true; else we return false. 
#### `thread * blockingThread(thread * t)`
This is a trivial function, implemented with various pointers.
#### `thread * findFirstUnblocked (thread* t)`
This is a recursive function. The base case is if blocked(`t`) is false we return `t`. Else we find `t2 = blockingThread(t)` and return `findFirstUnblocked(t2)`.
#### `list_less_func greater_priority(struct list_elem t1, struct list_elem t2, void *aux)`
In this function we simply return true if the priority of thread 1 is greater than the priority of thread 2, and false otherwise. 

### Synchronization 
Our synchronization policy is to make sure interrupts are turned off until the current thread has finished accessing any type of shared data. 
#### Interrupts coming in while the thread is trying to access a list. 
We simply disable interrupts until the thread is done accessing the list. Since no interrupts can come in, the thread cannot be switched to another one that might modify the data it was trying to access. 
#### A lock trying to acquire the lock, but an interrupt comes in and switches the thread to another one. 
In the implementation of a semaphore (a lock is a type of semaphore) we turn off interrupts until the lock is assigned. 

### Rationale
We thought the recursive solution to priority donation was neat, compact, and logically tight. We made extra helper functions to increase the readibility of the code and for easier debugging in case we need to. Our synchronization scheme is rather simple, but that also makes it easier to see that it is comprehensive, and, again, easy to debug.  

## Task 3: Multi-level Feedback Queue Scheduler
### Data Structures and Functions
In `threads/thread.h`, we are adding to the thread struct
* `int nice` to represent the thread's current nice value.
* `fixed_point_t recent_cpu` to represent the thread's most recent recent_cpu value.
* `void set_calculated_load_avg(thread *curr_thread, fixed_point_t old_value, int num_ready_thread);` Uses given formula to calculate global variable `load_avg`
 : 
* `void calculate_recent_cpu(thread *curr_thread);` uses given formula to calculate a thread’s new `recent_cpu`
* `void calculate_priority(thread *curr_thread);` uses given formula to calculate a thread’s new priority.

In `threads/thread.c`, we are adding this global variable to the code
* `fixed_point_t load_avg` to represent the system's most recent load average value.

### Algorithms

Every four ticks, we calculate every thread’s priority in an interrupt handler using the formula given. If the new priority is different from the original, we add the thread to the queue with the new priority. We leave the old thread in the queue since we compensate for the duplicate as stated below. We leave it in the queue because it is inefficient to find where in the queue the “old” thread is located. Thus, we can ignore it until it comes up as a thread we want to run. Now, whenever we “switch a thread” to a new queue, we make sure that the thread is not already in the queue, so we don’t end up with multiple copies of the same thread in a queue.

Now, every second, or when `timer_ticks() % TIMER_FREQ == 0`, we want to update the global variable `load_avg` according to the given formula and each thread’s `recent_cpu` according to that given formula as well.

So we will modify timer_interrupt, to call a function to recalculate these values when `timer_ticks() % TIMER_FREQ == 0` or `timer_ticks() % 4 == 0`, depending on which value we want to update.

We will put a function for each formula calculating: `priority`, `recent_cpu`, and `load_avg` in `threads/thread.c` and then call them according to when the ticks are correct.

If there are multiple items within a priority queue, we implement the Round Robin style. We will pull the first thread in the queue, let it run for 4 ticks, then have the thread yield and place it in the back of the queue and pull the next item in the queue. This is so that multiple threads with the same priority will have an equal amount of cpu time.

Finally, in `timer_interrupt`, we will change this function so that it increments the current thread’s `recent_cpu` by 1.

1. Next thread to run we go through every single one of the queues, and we pick the first non empty queue starting from the queue corresponding to priority 63.
2. We pop the first item and check if priority of item == priority of queue.
3. If it is equal, we pick this thread to run.
4. Else, we “throw it away” and pick the next highest priority thread since the thread we throw away is a duplicate that we didn’t discard, as stated above.

### Synchronization

The things being shared across all the threads are

1. the 64 priority queues, whenever we add to the queue, we put this process in the interrupt handler to prevent race conditions.
2. the `load_avg`, but it gets calculated inside an interrupt handler since we need to calculate it on a specific tick. Thus, it should be synchronized, since none of the threads directly affect `load_avg` as well.

### Rationale

Because of the need to calculate `recent_cpu` and `load_avg` on a specific tick, most of the work is done from inside an interrupt handler. We need to calculate these in an interrupt handler, since if context is switched and more ticks pass, then the calculations may not be done at the desired time. Thus, before we do any thread running, we make sure that all the new values have been recalculated and updated, so that every thread runs with the new scheduler data values.

The only thing left under our control is how we swap a thread around if its priority changes. We first thought that we should delete the old thread pointer when a thread’s priority changes. However, this is inefficient because then we need to find the thread’s original position and remove it from the queue. So in changing a thread’s priority, we just add the thread to the respective queue and leave the thread’s old position there. This is because unless it comes up, we don’t need to worry about us running it. We save a lot of time not worrying about the old thread since we can just check every thread that is a potential “running thread” and make sure that it is in the correct queue. If the priority of the thread does not equal the priority of the queue, that means it was the position of the old thread.

In terms of complexity, we have a runtime of O(n) since every four ticks, we have to recalculate each thread’s priority.

## Additional Questions
#### Q1.
Start by creating a thread A with priority 1 and holding a lock X. Then have thread A create thread B, with thread B being given priority 3 and holding a lock Y. Next, have thread B create another thread C that has priority 5 and waiting for lock Y. Then have thread A create thread D with priority 4 and wanting to acquire lock X. 

In order for thread B to finish running, thread C has to donate its priority to B, which makes thread B’s effective priority 5. Suppose that after B finishes executing, it tries to acquire lock X. Even though thread D is also waiting for lock X, thread B gets it ahead of D since its (effective) priority is 5. However, if lock donation is not implemented, thread D would’ve gotten lock X before thread B given that its base priority is higher compared to that of thread B. 

To detect the bug, each creation of thread should be accompanied by a call to a function. In each function corresponding to a thread creation, call `lock_acquire()` with a thread’s desired lock and `lock_release()` when a thread is done using the lock. In particular, the functions should be set up in such a way that a conflict arises between thread B and D. Include print statements in both functions for correct implementation (using effective priorities) and buggy implementation (using base priorities).

Example:
```
tid_t thread_create(name, func, priority, lock) {
	// initialization
	func()
}

func_A() {
	lock_acquire(lock_X)
	thread_create(B, func_B, 3, lock_Y)
	thread_create(D, func_D, 4, lock_X)
	lock_release(lock_X)
}

func_B() {
	lock_acquire(lock_Y)
	thread_create(C, func_C, 5, lock_Y)
	lock_acquire(lock_X)
	// if thread B is holding lock X, then we have the right output.
	if lock_held_by_thread(lock_X)
		print "Correct implementation."
}

func_C() {
	// thread B's priority is increased to 5
	lock_acquire(lock_Y)
}

func_D() {
	lock_acquire(lock_X)
	// if thread D is holding lock X, then the output is wrong.
	if lock_held_by_thread(lock_X)
		print "Buggy implementation."
}

init(lock_X)
init(lock_Y)
thread_create(A, func_A, 1, lock_X)
```

#### Q2.
timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
 0          |  0   |  0   |  0   |  63  |  61  |  59  | 	 A
 4          |  4   |  0   |  0   |  62  |  61  |  59  |      A 
 8          |  8   |  0   |  0   |  60  |  61  |  59  |      B
12          |  8   |  4   |  0   |  61  |  60  |  59  |      A
16          |  12  |  4   |  0   |  60  |  60  |  59  |      B
20          |  12  |  8   |  0   |  60  |  59  |  59  |      A
24          |  16  |  8   |  0   |  59  |  59  |  59  |      C
28          |  16  |  8   |  4   |  59  |  59  |  58  |      B
32          |  16  |  12  |  4   |  59  |  58  |  58  |      A
36          |  20  |  12  |  4   |  58  |  58  |  58  |      C

#### Q3.
There are multiple occasions in which the priorities of the threads are the same. We resolve the ambiguities by comparing the values of recent_cpu and giving the thread with lower recent_cpu value the right to run. For the case in which all threads have the same priority, we use round robin to ensure that all threads get an equal amount of runtime. 