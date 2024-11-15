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
#include "vm/page.h"

extern struct lock file_lock;
void syscall_init (void);
bool parse_arguments (struct intr_frame *f, int* args, int numArgs);
bool validate_pointer(const void * pointer);
tid_t exec(const char *cmd_line);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size, void*);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
// int findFdForFile(void);
void lock_file(void);
void unlock_file(void);

/* Virtual Memory Functions */
mapid_t mmap(int, void *);
bool munmap(mapid_t);
bool put_mmap_in_list(struct spt_entry *);

/* Filesystem Functions */
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);

#endif /* userprog/syscall.h */
