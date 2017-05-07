Design Document for Project 2: User Programs
============================================

## Group Members

* Derek Feng <kianex@berkeley.edu>
* Jerry Huang <jerryhuang@berkeley.edu>
* Jaehyun Sim <simjay@berkeley.edu>
* Pei Jie Sim <peijiesim@berkeley.edu>

# Task 1
## Data Structures and Functions
In `process.c`, we add
`void handle_argument_passing(const char *file_name, void **esp);`

## Algorithms
* In `load` method in `process.c`, call `handle_argument_passing` after `setup_stack` method call.
* In `handle_argument_passing` method, split filename string(`*char`) by space and store the results into string(`*char`) array.
* Take the contents of the array and add `\0` at the end. Then push all of them onto the stack with `*esp`. While doing this, keep track of the size of the arguments. Also, keep the addresses(`esp` values) of the pushed arguments.
* Based on the sum of the sizes of all the arguments, perform word-align by subtracting the smallest positive integer from `esp` which makes it a multiple of 4.
* Push a null pointer onto the stack.
* Push the addresses of arguments onto the stack in reverse order.
* Push the address of `argv` and then push the int value of `argc` onto the stack.
* Finally, add a fake return address onto the stack.

## Synchronization
The job of method `handle_argument_passing` should be atomic to avoid synchronization issues. If another process runs while we are pushing arguments on stack, the whole program will not work correctly.

## Rationale

The order of things that we push onto the stack is the one in the project spec. We push a fake return value to keep the value format of address space even though we will not return to anything from starting-up process.

Here, we can notice size of the data of the arguments are not necessarily in words(multiples of 4 bytes). We then perform word-aligning so that all data added afterwards is word-aligned. Rather than pushing some n bytes of data to perform word-align, we just decrement esp value. This is so that we don't end up overwriting any other data and because the stack grows downwards, we are essentially creating more space in the stack by subtracting.

# Task 2

## Data Structures and Functions
In `threads/thread.h`, we add to the `struct thread`:
* `struct list children_processes` to store all the children processes for `exec ()` and `wait ()`.
* `struct semaphore wait_child` so that we can `sema_down` when the thread waits for a child process and the respective child can `sema_up` when it exits.
* `pid_t parent` to hold the pid of the parent of the process, if it has one.
* `bool waiting` to signify if the process is waiting on a child process.
* `pid_t wait_pid` to hold the pid of the process of the child it is waiting on.
* `struct child_process` to execute programs. The new struct has the following variables:
```
struct child_process {
	pid_t process_id; //to identify the child process
	int exit_status; //to signify the status of the process when it exited
	bool load_success; //checks whether an executable is successfully loaded
	bool exit_success; // checks whether a process exits successfully
	struct list_elem elem; //to store each child process in the list of child processes of a parent thread
}
```
In `userprog/syscall.h`, we add:
* `int practice (int i)` that takes increments the integer argument passed in by 1.
* `void halt (void)` to terminate Pintos.
* `void exit (int status)` to terminate the current user program and return the status to kernel.
* `pid_t exec (const char *cmd_line)` to run the executable whose name is given in `cmd_line`.
* `int wait (pid_t pid)` to wait for a child process pid and retrieves the child’s exit status.
* `void check_address (const void *address)` to check address to make sure it is not in the kernel’s virtual address, since the user should not be able to access it. Also will check to make sure the page stored at the address is valid.

## Algorithms
The general idea of this task is to add the necessary system call functions, i.e. practice, halt, exit, wait and exec in syscall.h. Then modify syscall_handler to handle the various cases of syscalls. 

We would have to modify `init_thread` in `threads/thread.c` so that it initializes all the new stuff we added to the struct thread.

* For `check_address ()`, in the functions in `userprog/syscall.h` we have to make sure the arguments to the practice, halt, exit, exec, and wait syscalls are valid before we pass them into the actual syscall functions. We can do this by checking for invalid pointers, i.e. pointers that point to unmapped memory locations, by calling `pagedir_get_page (USERPROG, f->esp)` and pointers to kernel virtual memory by calling `is_kernel_vaddr (f)` from `threads/vaddr.c`. A return value of true means the user does not have access to the address and we should not continue. Otherwise, we will return a pointer to the page returned from `pagedir_get_page`.

* Once we check that the address is valid and the page at the address is also valid, we now want to handle all the various syscalls. The first item stored in the page we pulled from the address would be which syscall the user called and we would then use a switch statement to handle the various cases.

* For task 2, there are 5 cases we have to implement in the `syscall_handler` function:
	1. If the syscall is SYS_PRACTICE, we would call `practice ()` which would:
		* However, we first need to check that the address of the first argument (pointer + 1) is valid, by passing in (pointer + 1) into `check_address ()` before we pass anything into `practice ()`
		* Now, we pass the data stored at (pointer + 1) into `practice ()`.
		* The function would then increment the value passed in and return it.
	2. If the syscall is SYS_HALT, we would call `halt ()`, which would:
		* Shutdown System by calling shutdown_power_off ()
	3. If the syscall is SYS_EXIT, we would call `exit ()` and pass in the next item in the page, which is \*(pointer + 1):
		* However, we first need to check that the address of the first argument (pointer + 1) is valid, so we would call `check_address ()` before we pass anything into `exit ()`.
		* Then, we pass in \*(pointer + 1) as the argument to `exit ()`
		* Then, we set the `exit_status` of the current process to `status`, which was passed in.
		* If the parent of the process has `waiting` as `True`, then we check:
			* If the id of the current thread is the same as the id of the child the parent is waiting on (`wait_pid`), we `sema_up ()` the semaphore - `wait_child` of the parent.
		* Finally, we call `thread_exit ()` and it will call `process_exit ()`.
		* `process_exit ()` will make sure the current process isn't the kernel thread and if it isn't, then we run the print statement given.
	4. If the syscall is SYS_EXEC, we would call `exec ()`:
		* However, we first need to check that the address (pointer + 1) is valid, so we would call `check_address ()` before we pass anything into `exec ()`.
		* Then, we pass in \*(pointer + 1) as the argument to `exec ()`
		* In `exec ()`, we call `process_execute ()` to ‘fork’ a new child process. 
		* Since `process_execute ()` returns the tid of the newly created thread, we cast that into pid and proceed. 
		* If the child process cannot load the executable (which can be checked by examining `load_success` of that process,) we return -1. Otherwise, we return `process_id` of the child.
	5. If the syscall is SYS_WAIT, we would call `wait ()`:
		* However, we first need to check that the address (pointer + 1) is valid, so we would call `check_address ()` before we pass anything into `wait ()`.
		* Then, we call `process_wait ()` after making the following changes to it.
			* To check if TID is the child of the current process, we iterate through the list `child_processes` to find TID.
			* If TID is not found, we return -1. Otherwise, we check the current process' to ensure that the process is not already waiting on another child thread.
			* If `exit_success` is false, we also return -1 since this indicates that a process did not exit successfully due to an exception by the kernel.
			* Lastly, we will wait on a child process by attempting to `sema_down ()` on `wait_child`. As was done in step 2, the child process will up the parent's semaphore when the child process exits.

## Synchronization
By having a semaphore for each process, we are guaranteed that a process waits on only one child thread and the child thread can signal the parent thread when it has exited. In other words, the communication between a parent and its children is done with proper synchronization and hence the parent cannot wait on any other child threads while it is already waiting on one.

## Rationale
The addition of a `struct child_process` in each thread is so that we can make the distinction between a kernel thread and a child process. This makes sense because the only way to create a new process is by creating a thread. That said, we will set the process_id of the process to be the thread_id of the thread that process is running on. 

At first, we thought that we should have the parent down the child's semaphore, but then we realized that we want to make sure a parent thread can wait on only one child thread and a semaphore in the parent is easier to keep track of since we call `wait` on a parent. Additionally, we only have to modify the parent semaphore rather than a bunch of children semaphores in the case that the parent has to end up waiting over multiple children over time.


# Task 3
## Data Structures and Functions
In `userprog/syscall.c` we want to include the `file.h` and `directory.h` so we can use the already implemented file system. We would also like to modify the `syscall_handler` in the same file to include functionalities for the following syscalls: `create` (SYS_CREATE), `remove` (SYS_REMOVE), `open` (SYS_OPEN), `filesize` (SYS_FILESIZE), `read` (SYS_READ), `write` (SYS_WRITE), `seek` (SYS_SEEK), `tell` (SYS_TELL), `close` (SYS_CLOSE).

For each of the syscalls, we are calling the approriate functions from `filesys/file.h` and `filesys/filesys.h`. 

Since there are no file descriptors in this implementation, we want to add an array `file * file_descriptors` of size 128 in which the *i*th element has a pointer to the corresponding file struct. Any untaken file descriptor *j* will have a `NULL` pointer in the *j*th element in `file_descriptors`. We will add this array in `syscall.c`. 
In `userprog/syscall.h`, we add:
* `bool create (const char *file, unsigned initial size)` which calls `filesys_create ()` to create a file.
* `bool remove (const char *file)` which calls `filesys_remove ()` to remove a file from a directory.
* `int open (const char *file)` which calls `filesys_open ()` to open a file. We'll assign this file the smallest available file descriptor and return this number.
* `int filesize (int fd)` which calls `file_length ()` to get the length of a file.
* `int read (int fd, void *buffer, unsigned size)` which calls `file_read ()` to read a file.
* `int write (int fd, const void *buffer, unsigned size)`which calls `file_write ()` to write to a file.
* `void seek (int fd, unsigned position)`which calls `file_seek ()` to seek.
* `unsigned tell (int fd)` which calls `file_tell ()` to tell.
* `void close (int fd)` which calls `file_close ()` to close a file.

## Algorithms
Similar to task 2, before calling the syscall functions defined above, we need to call `check_address ()` to ensure that the stack pointer as well as the addresses of the arguments are valid. 

* To handle the various file system calls:
	1. If the syscall is SYS_CREATE, we would call `create ()`:
		* We will use simply pass the two arguments received into `filesys_open ()`.
	2. If the syscall is SYS_REMOVE, we would call `remove ()`:
		* We simply pass the file name into `filesys_remove ()`.
	3. If the syscall is SYS_OPEN, we would call `open ()`:
		* We first pass the file name into `file_open`. Upon a successful file open, we add the file struct pointer returned to the first available (i.e non-`NULL`) index in `file_descriptors`. We find this index simply by iterating down the list. We then return this index. 
	4. If the syscall is SYS_FILESIZE, we would call `filesize ()`:
		* We take the `int fd` passed in and we look up this file descriptor by going to the *fd*th element our `file_descriptors` array. Then we take the `file` pointer and we pass it into `file_length` and we return this. 
	5. If the syscall is SYS_READ, we would call `read ()`:
		* Again, we get the file pointer from our `file_descriptors` array and we pass this, along with the other 2 given arguments, into `file_read`. We return the number returned by this function.
	6. If the syscall is SYS_WRITE, we would call `write ()`:
		* We get the file pointer from our `file_descriptors` array and we pass this, along with the other 2 given arguments, into `file_write`. We return the number returned by this function.
	7. If the syscall is SYS_SEEK, we would call `seek ()`:
		* We get the file pointer from our `file_descriptors` array and we pass this into `file_seek`. 
	8. If the syscall is SYS_TELL, we would call `tell ()`:
		* We get the file pointer from our `file_descriptors` array and we pass this into `file_tell`. We return the number returned by this function. 
	9. If the syscall is SYS_CLOSE, we would call `close ()`:
		* We get the file pointer from our `file_descriptors` array and we pass this into `file_close`. We then set the element of `file_descriptors` at index `fd` to `NULL`.

## Synchronization
We'll add a file system lock `filesys_lock` as a global variable in `syscall.c`. Before any of the above functions does anything it must acquire this lock and after it does what it has to do it releases the lock. This prevents multiple threads from modifying the file system at once. 

## Rationale
We chose an array for its simplicity; iterating through 128 indices takes negligible time. 


# Additional Questions
1. In `sc-bad-arg.c`, the stack pointer (`$esp`) is being set to 0xbffffffc on line 14, which is at the very top of the user virtual address space. The program then invokes the exit system call with that stack pointer and since the argument to that syscall is in the 32-bit word at the next higher address, we are venturing into the kernel address space. 

2. In `sc-boundary.c`, the stack pointer (`$esp`) is valid. However, after getting the page boundary on line 14, the program decrements it, thus the pointer now points to the last memory location of the previous page. It then fills in the arguments of the syscall to the memory pointed to by p, i.e. the syscall number SYS_EXIT on the previous page and the argument to exit, 42 on the next page. 

3. We can add tests regarding thread safety in which more than one thread tries to write to a file at the same time. Since we are implementing a global lock on filesystem operations for this project, a second thread trying to write to the same file one thread is currently writing to should wait until the first thread is done writing.

4. Questions on GDB
*
	1. Name of running thread: main 
	   Address: `0xc000ee0c`
	   Other threads present: 
	```
		{tid = 2, status = THREAD_BLOCKED, name = "idle",
		'\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, 
		allelem = {prev = 0xc000e020, next = 0xc0034b58 <all_list+8>}, 
		elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, 
		pagedir = 0x0, magic = 3446325067}
	```
	2. Backtrace: 
	```
		#0  process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32
		#1  0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288
		#2  0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340
		#3  main () at ../../threads/init.c:133
	```
	3. Name of running thread: args-none
	   Address: `0xc010afd4`
	   Other threads present: 
	```
		{tid = 1, status = THREAD_BLOCKED, name = "main", 
		'\000' <repeats 11 times>, stack = 0xc000eebc "\001", priority = 31, 
		allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, 
		elem = {prev = 0xc0036554 <temporary+4>, next = 0xc003655c <temporary+12>}, 
		pagedir = 0x0, magic = 3446325067}

		{tid = 2, status = THREAD_BLOCKED, name = "idle",
		'\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, 
		allelem = {prev = 0xc0 00e020, next = 0xc010a020}, 
		elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, 
		pagedir = 0x0, magic = 3446325067}
	```
	4. `tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);`

	5. 0x0804870c

	6. `_start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9`

	7. The page fault occurs because we haven't implemented argument passing.
