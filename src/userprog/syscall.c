#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <sys/types.h>
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

//whats difference from user/lib/syscall.c
/*
  tells OS to STOP process
*/
void halt(void)
{
  //does it need more?
  //free palloc pages?
  //close fds?
  shutdown_power_off();
}
/*
  process_exit() and free pallocs, printf?

  terminates current user program.
  returning status to kernel
*/
void exit(int status)
{
  //Exiting or terminating a process implicitly closes all its open file descriptors, 
  //as if by calling close() for each one

  //process_exit()
}
/*
  Runs the executable whose name is given in cmd_line, passing any given arguments.
  returns: the new process's program id (pid) or -1 on failure
*/
pid_t exec(const char *cmd_line)
{
  //process_execute(file name)
}
/*
  if pid alive, wait until terminate
  returns: 0 on success, non 0 on failure.

*/
int wait(pid_t pid)
{
  // We suggest that you implement process_wait() according to the comment at the top of the function
  // then implement the wait system call in terms of process_wait().

  //process_wait()
}
/*
  creates a file, however does not open the file
  returns: true on success, false otherwise
*/
bool create(const char *file, unsigned initial_size)
{
  //just use syscall()?
}
/*
  deletes a file, however does not close it
  returns: true on success, false otherwise
*/
bool remove(const char *file)
{
  //just use syscall()?
}
/*
  Opens the file called file.
  returns: an fd to openned file, but not 0 or 1
*/
int open(const char *file)
{
  //Each process has an independent set of file descriptors. File descriptors are not inherited by child processes
  //openning same file makes new fds (act as if different files)

  //just use syscall()?
}
/*
  Returns: the size, in bytes, of the file open as fd.
*/
int filesize(int fd)
{
   //syscall(SYS_fstat, fileDescriptor, &fileStat) ?
}
/*
  Reads size bytes from the file open as fd into buffer.
  Returns: the number of bytes actually read (0 at end of file), 
     or -1 if the file could not be read (due to a condition other than end of file). 
*/
int read(int fd, void *buffer, unsigned size)
{
  //Fd 0 reads from the keyboard using input_getc().
}
/*
  Writes size bytes from buffer to the open file fd. 
  Returns: the number of bytes actually written (which may be less than size if some bytes could not be written).
*/
int write(int fd, const void *buffer, unsigned size)
{
  //Fd 1 writes to the console. 

  //Your code to write to the console should write all of buffer in one call to putbuf(), 
  //at least as long as size is not bigger than a few hundred bytes. 
}

/*
  Changes the next byte to be read or written in open file fd to position, 
  expressed in bytes from the beginning of the file. 
  (Thus, a position of 0 is the file's start.)
*/
void seek(int fd, unsigned position)
{
  
}
/*
  Returns: the position of the next byte to be read or written in open file fd
*/
unsigned tell(int fd)
{

}
/*
  Closes file descriptor fd. 
*/
void close(int fd)
{
}