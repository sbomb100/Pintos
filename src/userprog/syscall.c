#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "filesys/filesys.h"
//#include <sys/types.h>
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static void
syscall_handler(struct intr_frame *f UNUSED)
{
  //get pointer (int bc we want sys call number)
  int* p = f->esp;
  //check if its a good pointer
  validate_pointer(p);
  int syscall_num = *p;
  int args[3];
  //PUT RETURNS IN EAX REGISTER ON FRAME, converted to same type as EAX for consistency
  switch (syscall_num){
    case SYS_HALT:
      halt();
    case SYS_EXIT:
      //get arg for exit then exit
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_CREATE:
      parse_arguments(f, &args, 2);
  	  f->eax = (uint32_t) create(args[0], args[1]);
    case SYS_REMOVE:
      parse_arguments(f, &args, 1);
      f->eax = (uint32_t) remove(args[0]);
    case SYS_OPEN:
      parse_arguments(f, &args, 1);
      f->eax = (uint32_t) open(args[0]);
    case SYS_FILESIZE:
      parse_arguments(f, &args, 1);
      f->eax = (uint32_t) filesize(args[0]);
    case SYS_READ:
      parse_arguments(f, &args, 3);
      f->eax = (uint32_t) read(args[0], args[1], args[2]);
    case SYS_WRITE:
      parse_arguments(f, &args, 3);
      f->eax = (uint32_t) write(args[0], args[1], args[2]);
    case SYS_SEEK:
      parse_arguments(f, &args, 2);
      seek(args[0], args[1]);
    case SYS_TELL:
      parse_arguments(f, &args, 1);
      f->eax = (uint32_t) tell(args[0]);
    case SYS_CLOSE:
      parse_arguments(f, &args, 1);
      close(args[0]);
    default: 
      printf("system call!\n");

  }
  
  //thread_exit();

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
//pid_t exec(const char *cmd_line)
//{
  //process_execute(file name)
//}
/*
  if pid alive, wait until terminate
  returns: 0 on success, non 0 on failure.

*/
//int wait(pid_t pid)
//{
  // We suggest that you implement process_wait() according to the comment at the top of the function
  // then implement the wait system call in terms of process_wait().

  //process_wait()
//}
/*
  creates a file, however does not open the file
  returns: true on success, false otherwise
*/
bool create(const char *file, unsigned initial_size)
{
  if (file == NULL)
    return -1;

  lock_acquire(&file_lock);
  int ret = filesys_create(file, initial_size);
  lock_release(&file_lock);

  return ret;
}
/*
  deletes a file, however does not close it
  returns: true on success, false otherwise
*/
bool remove(const char *file)
{
  if (file == NULL)
    return -1;
  lock_acquire(&file_lock);
  bool flag = filesys_remove(file);
  lock_release(&file_lock);

  return flag;
}
/*
  Opens the file called file.
  returns: an fd to openned file, but not 0 or 1
*/
int open(const char *file)
{
  // Each process has an independent set of file descriptors. File descriptors are not inherited by child processes
  // openning same file makes new fds (act as if different files)

  lock_acquire(&file_lock);
  struct file * fp = filesys_open (file);
  lock_release(&file_lock);
  
  if (fp == NULL) 
    return -1;
  
  int fd = findFdForFile(); //does index + 2 to avoid 0 or 1
  if (fd == -1) //need to expand array
    return -1;
  //lock around it? i dont think so
  thread_current()->fdToFile[fd - 2] = fp;
  
  return fd;
}
/*
  Returns: the size, in bytes, of the file open as fd.
*/
int filesize(int fd)
{
  struct file * filePtr = thread_current()->fdToFile[fd - 2];
  lock_acquire(&file_lock);
  int length = file_length(filePtr); 
  lock_release(&file_lock);
  return length;
}
/*
  Reads size bytes from the file open as fd into buffer.
  Returns: the number of bytes actually read (0 at end of file),
     or -1 if the file could not be read (due to a condition other than end of file).
*/
int read(int fd, void *buffer, unsigned size)
{
  // Fd 0 reads from the keyboard using input_getc().
  int bytesRead = 0;
  
  // If fd == 0, reads from keyboard using input_getc()
  if (fd == 0)
  { 
    while (bytesRead < size)
    {
      *((char *)buffer+bytesRead) = input_getc();
      bytesRead++;
    }
    return bytesRead;
  }

  // fd is not 0, so read it
  struct file * filePtr = thread_current()->fdToFile[fd - 2];
  if (filePtr == NULL)
    return -1;
  
  lock_acquire(&file_lock);
  // Read from the file using filesys function
  bytesRead = file_read(filePtr,buffer,size);
  lock_release(&file_lock);
  return bytesRead;
}
/*
  Writes size bytes from buffer to the open file fd.
  Returns: the number of bytes actually written (which may be less than size if some bytes could not be written).
*/
int write(int fd, const void *buffer, unsigned size)
{
  // Fd 1 writes to the console.

  // Your code to write to the console should write all of buffer in one call to putbuf(),
  // at least as long as size is not bigger than a few hundred bytes.
  // if fd == 1, write to standard output
  if (fd == 1)
  {
    putbuf(buffer,size);
    return size;
  }
  
 struct file* fileDes = thread_current()->fdToFile[fd - 2];
  if (fileDes == NULL)
    return -1;
  
  lock_acquire(&file_lock);
  // write to the file using filesys function
  int ret = file_write(fileDes,buffer,size);
  lock_release(&file_lock);
  return ret;
}

/*
  Changes the next byte to be read or written in open file fd to position,
  expressed in bytes from the beginning of the file.
  (Thus, a position of 0 is the file's start.)
*/
void seek(int fd, unsigned position)
{
  struct file_desc * fd_elem = get_file(fd);

  if (fd_elem == NULL)
    return;
  
  lock_acquire(&file_lock);
  file_seek(thread_current()->fdToFile[fd - 2], position);
  lock_release(&file_lock);
}
/*
  Returns: the position of the next byte to be read or written in open file fd
*/
unsigned tell(int fd)
{
  struct file* fileDes = thread_current()->fdToFile[fd - 2];
  if (fileDes == NULL)
    return -1;

  lock_acquire(&file_lock);
  unsigned pos = file_tell (fileDes);
  lock_release(&file_lock);
  return pos;
}
/*
  Closes file descriptor fd.
*/
void close(int fd)
{
  if (fd == 0 || fd == 1)
    return;
  
  struct file* fileDes = thread_current()->fdToFile[fd - 2];
  if (fileDes == NULL)
    return -1;
  
  
  lock_acquire(&file_lock);
  // Closing file using file sys function
  file_close(fileDes);
  lock_release(&file_lock);
}

//index of zero is fd of 2
//TODO make array exapandable
int findFdForFile(){
  struct file** fdArray = thread_current()->fdToFile;

  for (int i = 0; i < 1024; i++){
    if (fdArray[i] == NULL){
      return i + 2;
    }
  }
  return -1;
}


/*
this checks the address of a given pointer to validate it*/
void* validate_pointer(const void * givenPointer)
{ 
    //is not a user virtual address
    if (!is_user_vaddr(givenPointer)) //from threads/vaddr.h
    {
        return 0;
    }
    //from userprog/pagedir.c
    //vaddr < USER_VADDR_BOTTOM?
    void *point = pagedir_get_page(thread_current()->pagedir, givenPointer);
    //is it a null when dereferenced
    if (point == NULL)
    {
        return 0;
    }
    //return the valid pointer
    return point;
}
/* get arguments off of the stack */
void
parse_arguments (struct intr_frame *f, int *args, int numArgs)
{
  int *tempPointer = f->esp + 1; //area of first actual argument
  for (int i = 0; i < numArgs; i++)
  {
    //get arg, check its pointer, then set to an argument
    tempPointer = (int *) tempPointer + i;
    validate_pointer((const void *) tempPointer);
    args[i] = *tempPointer;
  }
}