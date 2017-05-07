#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdio.h>

typedef int pid_t;

void syscall_init (void);

/* Process control syscalls */
int practice (int);
void halt (void);
void exit (int);
pid_t exec (const char *);
int wait (pid_t);

/* File operation syscalls */
bool create (const char *, unsigned);
bool remove (const char *);
int open (const char *);
int filesize (int);
int read (int, void *, unsigned);
int write (int, const void *, unsigned);
void seek (int, unsigned);
unsigned tell (int);
void close (int);

/* File Systems, Project 3 Task 3. */
bool chdir (const char *filename);
bool mkdir (const char *filename);
bool readdir (int fd, char *filename);
bool isdir (int fd);
int inumber (int fd);
int hit (void);
int miss (void);
unsigned long long read_cnt (void);
unsigned long long write_cnt (void);
void reset_read (void);

#endif /* userprog/syscall.h */
