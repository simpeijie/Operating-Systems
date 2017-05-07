#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/inode.h"

static void syscall_handler (struct intr_frame *);
/*
  Checks ADDRESS to make sure it is valid (not in kernel address space.)
*/
void *get_kernel_address (const void *address);

/*
  Checks ADDRESS to make sure it is mapped, and if so returns
  the kernel virtual address corresponding to
  the physical memory mapped by ADDRESS.
*/
void check_pointer (const void *address);

/*
  Checks BUFFER to make sure both BUFFER all the way to BUFFER + SIZE are valid.
*/
void check_buffer (const void *buffer, unsigned size);

int first_null (void);
struct file* get_check_file (int);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);

  /* checks the validity of the stack pointer to the syscall number */
  get_kernel_address ((const void*) args);

  void *cmd_line;

  /* Start of Task 2. */
  switch (args[0])
  {
    case SYS_HALT:
    {
      halt ();
      break;
    }
    case SYS_PRACTICE:
    {
      check_pointer (&args[1]);
      f->eax = practice (args[1]);
      break;
    }
    case SYS_EXEC:
    {
      cmd_line = get_kernel_address ((void *) args[1]);
      f->eax = exec ((const char *) cmd_line);
      break;
    }
    case SYS_WAIT:
    {
      check_pointer (&args[1]);
      f->eax = wait (args[1]);
      break;
    }
    case SYS_EXIT:
    {
      check_pointer (&args[1]);
      f->eax = args[1];
      exit (args[1]);
      break;
    }
    /* End of Task 2. */

    /* Start of Task 3 */
    void *file_name;
    void *buf;

    case SYS_CREATE:
    {
      file_name = get_kernel_address ((void *) args[1]);
      check_pointer (&args[2]);

      f->eax = create (file_name, args[2]);
      break;
    }
    case SYS_REMOVE:
    {
      file_name = get_kernel_address ((void *) args[1]);

      f->eax = remove (file_name);
      break;
    }
    case SYS_OPEN:
    {
      file_name = get_kernel_address ((void *) args[1]);

      f->eax = open (file_name);
      break;
    }
    case SYS_FILESIZE:
    {
      check_pointer (&args[1]);

      f->eax = filesize (args[1]);
      break;
    }
    case SYS_READ:
    {
      check_pointer (&args[1]);
      check_buffer ((void *) args[2], (unsigned) args[3]);
      buf = get_kernel_address ((void *) args[2]);
      check_pointer (&args[3]);

      f->eax = read (args[1], buf, args[3]);
      break;
    }
    case SYS_WRITE:
    {
      check_pointer (&args[1]);
      check_buffer ((void *) args[2], (unsigned) args[3]);
      buf = get_kernel_address ((void *) args[2]);
      check_pointer (&args[3]);

      f->eax = write (args[1], buf, args[3]);
      break;
    }
    case SYS_SEEK:
    {
      check_pointer (&args[1]);
      check_pointer (&args[2]);

      seek (args[1], args[2]);
      break;
    }
    case SYS_TELL:
    {
      check_pointer (&args[1]);

      f->eax = tell (args[1]);
      break;
    }
    case SYS_CLOSE:
    {
      check_pointer (&args[1]);

      close (args[1]);
      break;
    }
    /* End of Task 3 */

    void *path;
    /* Added for project 3 */
    case SYS_CHDIR:
    {
      path = get_kernel_address ((void *) args[1]);

      f->eax = chdir ((const char *) path);
      break;
    }
    case SYS_MKDIR:
    {
      path = get_kernel_address ((void *) args[1]);

      f->eax = mkdir ((const char *) path);
      break;
    }
    case SYS_READDIR:
    {
      check_pointer (&args[1]);
      path = get_kernel_address ((void *) args[2]);
      f->eax = readdir (args[1], path);
      break;
    }
    case SYS_ISDIR:
    {
      check_pointer (&args[1]);

      f->eax = isdir (args[1]);
      break;
    }
    case SYS_INUMBER:
    {
      check_pointer (&args[1]);

      f->eax = inumber (args[1]);
      break;
    }
    case SYS_HIT:
    {
      f->eax = hit ();
      break;
    }
    case SYS_MISS:
    {
      f->eax = miss ();
      break;
    } 
    case SYS_READ_CNT:
    {
      f->eax = read_cnt ();
      break;
    }
    case SYS_WRITE_CNT:
    {
      f->eax = write_cnt ();
      break;
    }
    case SYS_RESET_READ_CNT:
    {
      reset_read ();
      break;
    } 
  }
}


/* Practice simply returns the argument + 1. */
int
practice (int i)
{
  return i + 1;
}

/* Halt will stop everything. */
void
halt (void)
{
  shutdown_power_off ();
}

/* Exit cleans up anything it owns that is no longer needed. */
void
exit (int status)
{
  struct thread *current = thread_current ();
  struct process *proc = current->proc;
  if (proc)
  {
    /* Decrements proc's counter and frees if parent already exited. */
    if ((--proc->counter) == 0)
      free (proc);
    else
    {
      /* Otherwise we set the exit conditions for the proc so the parent can access. */
      proc->exit_success = true;
      if (proc->waited_on == true)
        sema_up (&proc->wait_child);
      proc->exit_status = status;
    }
  }

  printf ("%s: exit(%d)\n", current->name, status);
  thread_exit ();
}

pid_t
exec (const char *cmd_line)
{
  pid_t pid = process_execute (cmd_line);
  struct thread *current = thread_current ();
  bool found = false;
  struct process *p = NULL;

  /*  Iterate through the child processes of the current (parent) thread
      and find the child to execute.
  */
  struct list_elem *e;
  for (e = list_begin (&current->children);
       e != list_end (&current->children) && found == false;
       e = list_next (e))
  {
    struct process *temp = list_entry (e, struct process, elem);
    if (temp->child_pid == pid)
    {
      p = temp;
      found = true;
    }
  }

  /* Wait till the child process has finally loaded. */
  while (p->load_success == 2)
    thread_yield ();
  /* If child failed to load successfully. */
  if (!p || p->load_success == 0)
    return -1;

  return pid;
}

int
wait (pid_t pid)
{
  return process_wait (pid);
}

bool
create (const char *file, unsigned initial_size)
{
  bool ret_val = filesys_create (file, initial_size, false);
  return ret_val;
}

bool
remove (const char *file)
{
  bool ret_val = filesys_remove (file);
  return ret_val;
}

int
open (const char *file)
{
  struct file *fptr = filesys_open (file);
  if (fptr == NULL) {
    return -1;
  } 
  else {
    int first = first_null ();
    if (inode_is_dir (file_get_inode (fptr))) {
      thread_current ()->cwd = (struct dir *) fptr;
    }
    thread_current ()->fd_array[first] = fptr;
    return first;
  }
}

int
filesize (int fd)
{
  struct file *f;
  if ((f = get_check_file (fd)) != NULL)
    return file_length (f);
  else
    return -1;
}

int
read (int fd, void *buffer, unsigned size)
{
  if (fd == STDIN_FILENO)
  {
    unsigned i = 0;
    char *ret = (char *) buffer;
    for (; i < size; i++) {
      ret[i] = input_getc ();
    }
    return size;
  }
  else if (fd == STDOUT_FILENO)
    return 0;
  else
  {
    struct file *f = get_check_file (fd);
    if (!f)
      return -1;
    /* Don't read from directory, so error */
    if (inode_is_dir (file_get_inode (f)))
      return -1;
    return file_read (f, buffer, size);
  }
}

int
write (int fd, const void *buffer, unsigned size)
{
  if (fd == STDOUT_FILENO)
  {
    putbuf (buffer, size);
    return size;
  }
  else if (fd == STDIN_FILENO)
    return 0;
  else
  {
    struct file *f = get_check_file (fd);
    if (!f)
      return -1;
    /* Don't write to directory, so error */
    if (inode_is_dir (file_get_inode (f)))
      return -1;
    return file_write (f, buffer, size);
  }
}

void
seek (int fd, unsigned position)
{
  file_seek (get_check_file (fd), position);
}

unsigned
tell (int fd)
{
  return file_tell (get_check_file (fd));
}

void
close (int fd)
{
  file_close (get_check_file (fd));
  thread_current ()->fd_array[fd] = NULL;
}

void *
get_kernel_address (const void *address)
{
  /* checks if ptr is null or if ptr is in kernel address space */
  check_pointer (address);

  /* validates ptr to make sure it maps to a location in physical memory */
  int *ptr;
  if ((ptr = pagedir_get_page (thread_current ()->pagedir, address)) == NULL)
    exit (-1);
  return ptr;
}

void
check_pointer (const void *address)
{
  if (!address || is_kernel_vaddr (address))
    exit (-1);
}

void
check_buffer (const void *buffer, unsigned size)
{
  if (is_kernel_vaddr (buffer) || is_kernel_vaddr (buffer + size)) {
    exit (-1);
  }
}

/*
 * This function returns the index of the first null element in
 * the fd_array.
*/
int
first_null()
{
  struct thread *t = thread_current ();
  int i = 2;
  for (; i < FD_LENGTH; i++) {
   if (t->fd_array[i] == NULL) {
     return i;
   }
  }
  return FD_LENGTH;
}

/*
  This function returns the file associated with FD.
*/
struct file *
get_check_file (int fd)
{
  struct thread *t = thread_current ();
  if (fd < 0 || fd >= FD_LENGTH)
    exit(-1);
  if (t->fd_array[fd] == NULL) {
    return NULL;
  }
  return t->fd_array[fd];
}

/* Project 3-3 syscalls. */
// TODO, need to support directories for file descriptors
bool
chdir (const char *filename)
{
  bool ret;
  /* TODO: Acquire lock. */
  ret = filesys_chdir (filename);
  /* TODO: Release lock. */
  return ret;
}

bool
mkdir (const char *filename)
{
  bool ret;
  /* TODO: Acquire lock. */
  ret = filesys_create (filename, 0, true);
  /* TODO: Release lock. */
  return ret;
}

bool
readdir (int fd, char *filename)
{
  struct file *f = get_check_file (fd);
  if (!f) {
    return false;
  }
  if (!inode_is_dir (file_get_inode (f))) {
    return false;
  }
  return dir_readdir (thread_current ()->cwd, filename);
}

bool
isdir (int fd)
{
  bool ret;
  /* TODO: Acquire lock. */
  ret = inode_is_dir (file_get_inode (get_check_file (fd)));
  /* TODO: Release lock. */
  return ret;
}

int
inumber (int fd)
{
  int ret;
  /* TODO: Acquire lock. */
  struct file *f = get_check_file (fd);
  if (!inode_is_dir (file_get_inode (f))) {
    ret = (int) inode_get_inumber (file_get_inode (f));
  }
  else {
    ret = (int) inode_get_inumber (dir_get_inode ((struct dir *) f));
  /* TODO: Release lock. */
  }
  return ret;
}

int 
hit (void)
{ 
  return cache_get_hit_cnt ();
}

int 
miss (void)
{
  return cache_get_miss_cnt ();
}

unsigned long long
read_cnt (void) 
{
  return filesys_get_read_cnt ();
}

unsigned long long
write_cnt (void)
{
  return filesys_get_write_cnt ();
}

void 
reset_read (void)
{
  return filesys_reset_read_cnt ();
}
