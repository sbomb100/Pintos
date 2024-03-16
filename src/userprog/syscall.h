#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include  "threads/vaddr.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "devices/shutdown.h"
void syscall_init (void);
bool parse_arguments (struct intr_frame *f, int* args, int numArgs);
bool validate_pointer(const void * pointer);
tid_t exec(const char *cmd_line);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
int findFdForFile(void);
void lock_file(void);
void unlock_file(void);
extern struct lock file_lock;
#endif /* userprog/syscall.h */
