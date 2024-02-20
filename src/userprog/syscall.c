#include "userprog/syscall.h"
#include "userprog/process.h"

//#include <sys/types.h>
static void syscall_handler (struct intr_frame *);
struct lock file_lock;

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
/* get arguments off of the stack */ 
 bool parse_arguments (struct intr_frame *f, int* args, int numArgs)
 {
   int i;
    int *ptr;
    for (i = 0; i < numArgs; i++)
    {
        ptr = (int *) f->esp + i + 1;
        validate_pointer((const void *) ptr);
        args[i] = *ptr;
    }
    return true;
 } 

// bool parse_arguments (void *ptr, int args)
// {
//   for (int i = 0; i < 4*args; i++) {
//     if (!validate_pointer((int *) ptr+i))
//       return false;
//   }
//   return true;
// } 

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  //get pointer (int bc we want sys call number)
  int* p = f->esp; //esp
  //check if its a good pointer
  validate_pointer(p);
  int syscall_num = *p;
  int args[3];
  //PUT RETURNS IN EAX REGISTER ON FRAME, converted to same type as EAX for consistency
  switch (syscall_num){
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      printf("SYS_EXIT\n");
      if (!parse_arguments(f, &args[0], 1))
        thread_exit(-1);
      // thread_current()->exit_status = *(int *) (p + 4); // TODO: exit_status should be in child struct
      struct thread *cur = thread_current();
      // int arg = *(int *) (p + 4);
      printf ("%s\n", cur->name);
      thread_exit(*(int *) (p + 4));
      break;
    case SYS_EXEC:
      printf("SYS_EXEC\n");
      break;
    case SYS_WAIT:
      printf("SYS_WAIT\n");
      if (!parse_arguments(f, &args[0], 1))
        thread_exit(-1);
      int arg = *(int *) (p + 4);
      f->eax = process_wait(arg);
      break;
    case SYS_CREATE:
      printf("SYS_CREATE\n");
      if (!parse_arguments(f, &args[0], 2)){
        thread_exit(-1);
        return;
      }
  	  f->eax = (uint32_t) create((const char *) (p + 4), (unsigned int) (p + 8));
      break;
    case SYS_REMOVE:
      printf("SYS_REMOVE\n");
      if (!parse_arguments(f, &args[0], 1)){
        thread_exit(-1);
        return;
      }
      f->eax = (uint32_t) remove((const char *) args[0]);
      break;
    case SYS_OPEN:
      printf("SYS_OPEN\n");
      if (!parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
      }
      f->eax = (uint32_t) open((const char *)  args[0]);
      break;
    case SYS_FILESIZE:
      printf("SYS_FILESIZE\n");
      if (!parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
      }
      f->eax = (uint32_t) filesize(args[0]);
      break;
    case SYS_READ:
      printf("SYS_READ\n");
      if (!parse_arguments(f, &args[0], 3)) {
        thread_exit(-1);
        return;
      }
      f->eax = (uint32_t) read(args[0], (void *) args[1], (unsigned int) args[2]);
      break;
    case SYS_WRITE:
      printf("SYS_WRITE\n");
      if (!parse_arguments(f, &args[0], 3)) {
        thread_exit(-1);
        return;
      }
      f->eax = (uint32_t) write(args[0], (const void *) args[1], (unsigned int) args[2]);
      break;
    case SYS_SEEK:
      printf("SYS_SEEK\n");
      if (!parse_arguments(f, &args[0], 2)) {
        thread_exit(-1);
        return;
      }
      seek(args[0], (unsigned int) args[1]);
      break;
    case SYS_TELL:
      printf("SYS_TELL\n");
      if (!parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
      }
      f->eax = (uint32_t) tell(args[0]);
      break;
    case SYS_CLOSE:
      printf("SYS_CLOSE\n");
      if (!parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
      }
      close(args[0]);
      break;
    default: 
      printf("system call!\n");

  }
  
  thread_exit(0);

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
  unsigned bytesRead = 0;
  
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
printf ("fd: %d\n", fd);
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
  struct file* fileDes = thread_current()->fdToFile[fd - 2];

  if (fileDes == NULL)
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
    return;
  
  
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
// returns 0 if it's not valid
bool validate_pointer(const void * givenPointer)
{ 
    //is not a user virtual address
    if (!is_user_vaddr(givenPointer)) //from threads/vaddr.h
    {
        return false;
    }
    //from userprog/pagedir.c
    //vaddr < USER_VADDR_BOTTOM?
    void *point = (void *)pagedir_get_page(thread_current()->pagedir, givenPointer);
    //is it a null when dereferenced
    if (point == NULL)
    {
        return false;
    }
    //return the valid pointer
    return true;
}
