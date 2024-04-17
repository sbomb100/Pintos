#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "userprog/exception.h"
#include "threads/vaddr.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "userprog/process.h"
#include "threads/cpu.h"
struct lock file_lock;

static void syscall_handler(struct intr_frame *);
static struct file_descriptor *find_fd(int fd);

void lock_file()
{
  // ASSERT(!lock_held_by_current_thread(&file_lock));
  if (!lock_held_by_current_thread(&file_lock))
  {
    lock_acquire(&file_lock);
  }
}
void unlock_file()
{
  ASSERT(lock_held_by_current_thread(&file_lock));
  lock_release(&file_lock);
}
void syscall_init(void)
{
  lock_init(&file_lock);

  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* get arguments off of the stack */
bool parse_arguments(struct intr_frame *f, int *args, int numArgs)
{
  int i;
  int *ptr;
  for (i = 0; i < numArgs; i++)
  {
    ptr = (int *)f->esp + i + 1;
    if (!validate_pointer((const void *)ptr))
      return false;
    args[i] = *ptr;
  }
  return true;
}

static void
syscall_handler(struct intr_frame *f)
{
  int *p = f->esp;
  if (!validate_pointer(p))
  {
    thread_exit(-1);
    return;
  }
  int syscall_num = *p;
  int args[3];
  switch (syscall_num)
  {
  case SYS_HALT:
    shutdown_power_off();
    break;
  case SYS_EXIT:
    if (!parse_arguments(f, &args[0], 1))
      thread_exit(-1);
    thread_exit(*(int *)(p + 1));
    break;
  case SYS_EXEC:
    if (!parse_arguments(f, &args[0], 1))
      thread_exit(-1);
    f->eax = exec((const char *)args[0]);
    break;
  case SYS_WAIT:
    if (!parse_arguments(f, &args[0], 1))
      thread_exit(-1);
    int arg = *(int *)(p + 4);
    f->eax = process_wait(arg);
    break;
  case SYS_CREATE:
    if (!parse_arguments(f, &args[0], 2))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)create((const char *)(args[0]), (unsigned int)(args[1]));
    break;
  case SYS_REMOVE:
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)remove((const char *)args[0]);
    break;
  case SYS_OPEN:
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)open((const char *)args[0]);
    break;
  case SYS_FILESIZE:
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)filesize(args[0]);
    break;
  case SYS_READ:
    if (!parse_arguments(f, &args[0], 3))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)read(args[0], (void *)args[1], (unsigned int)args[2], p);
    break;
  case SYS_WRITE:
    if (!parse_arguments(f, &args[0], 3))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)write(args[0], (const void *)args[1], (unsigned int)args[2]);
    break;
  case SYS_SEEK:
    if (!parse_arguments(f, &args[0], 2))
    {
      thread_exit(-1);
      return;
    }
    seek(args[0], (unsigned int)args[1]);
    break;
  case SYS_TELL:
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)tell(args[0]);
    break;
  case SYS_CLOSE:
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    close(args[0]);
    break;
  case SYS_MMAP:
  {
    if (!parse_arguments(f, &args[0], 2))
    {
      thread_exit(-1);
      return;
    }
    mapid_t return_val = mmap(args[0], (void *)args[1]);
    f->eax = return_val;
    break;
  }
  case SYS_MUNMAP:
  {
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    if (!munmap(args[0]))
    {
      thread_exit(-1);
      return;
    }
    break;
  }
  case SYS_CHDIR:
  // printf("chdir\n");
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)chdir((const char *)args[0]);
    break;
  case SYS_MKDIR:
  // printf("mkdir\n");
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)mkdir((const char *)args[0]);
    break;
  case SYS_READDIR:
  // printf("readdir\n");
    if (!parse_arguments(f, &args[0], 2))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)readdir(args[0], (char *)args[1]);
    break;
  case SYS_ISDIR:
  // printf("isdir\n");
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)isdir(args[0]);
    break;
  case SYS_INUMBER:
    if (!parse_arguments(f, &args[0], 1))
    {
      thread_exit(-1);
      return;
    }
    f->eax = (uint32_t)inumber(args[0]);
    break;
  default:
    thread_exit(-1);
  }
}

/*
 * Runs the executable whose name is given in cmd_line, passing any given arguments.
 * Returns the new process's program id (pid) or -1 on failure
 */
tid_t exec(const char *cmd_line)
{
  if (cmd_line == NULL || !validate_pointer(cmd_line))
    return -1;

  return process_execute(cmd_line);
}

/*
 * Creates a file, however does not open the file
 * Returns true on success, false otherwise
 */
bool create(const char *file, unsigned initial_size)
{

  if (file == NULL || !validate_pointer(file))
    thread_exit(-1);

  lock_file();
  int ret = filesys_create(file, initial_size, false);
  unlock_file();

  return ret;
}

/*
 * Deletes a file, however does not close it
 * Returns true on success, false otherwise
*/
bool remove(const char *file)
{
  if (file == NULL || !validate_pointer(file))
    thread_exit(-1);

  lock_file();
  bool flag = filesys_remove(file);
  unlock_file();

  return flag;
}

/*
 * Opens the file called file.
 * returns: an fd to openned file, but not 0 or 1
 * Each process has an independent set of file descriptors. File descriptors are not inherited by child processes
 * openning same file makes new fds (act as if different files)
 */
int open(const char *file)
{
  // printf("trying to open %s\n", file);
  if (file == NULL || !validate_pointer(file))
  {
    thread_exit(-1);
  }

  lock_file();
  struct file *fp = filesys_open(file);
  if (fp == NULL)
  {
    // inode_close(file_get_inode(fp));
    unlock_file();
    return -1;
  }
  struct inode *inode = file_get_inode(fp);
  unlock_file();

  if (fp == NULL)
    thread_exit(0);

  
  struct file_descriptor *fd = malloc(sizeof(struct file_descriptor));
  if (fd != NULL) {
    if (inode_is_directory(inode))
    {
      // printf("is dir in open\n");
      fd->dir = dir_open(inode);
      fd->file = NULL;
    }
    else
    {
      // printf("is file in open\n");
      fd->file = fp;
      fd->dir = NULL;
    }
    if (fd->file != NULL || fd->dir != NULL)
    {
      fd->fd = thread_current()->fd;
      thread_current()->fd++;
      list_push_back(&thread_current()->fdToFile, &fd->elem);
      return fd->fd;
    }
    else
    {
      free(fd);
      inode_close(inode);
    }
  }

  return -1;
  
}

/*
 * Returns: the size, in bytes, of the file open as fd.
 */
int filesize(int fd)
{
  lock_acquire(&file_lock);
  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    lock_release(&file_lock);
    return -1;
  }
  struct file *filePtr = fd2->file;
  if (filePtr == NULL)
  {
    lock_release(&file_lock);
    return -1;
  }
  int length = file_length(filePtr);
  // struct file *filePtr = thread_current()->fdToFile[fd - 2];
  // int length = file_length(filePtr);
  lock_release(&file_lock);
  return length;
}
/*
 * Reads size bytes from the file open as fd into buffer.
 * Returns: the number of bytes actually read (0 at end of file) or -1 if the file could not be read (due to a condition other than end of file).
 */
int read(int fd, void *buffer, unsigned size, void* esp)
{
  if (fd < 0 || fd == 1 || fd > 1025)
  {
    return -1;
  }

  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    return -1;
  }

  if (buffer == NULL || !is_user_vaddr(buffer))
	{
		thread_exit(-1);
	}

	if((void*)get_page_from_hash(buffer) == NULL) 
	{
		if ( buffer < esp) {
		  thread_exit(-1);
		}    
	}
  lock_acquire(&file_lock);

  int byteCount = 0;
  int success = 0;

  void *buffer_start = pg_round_down(buffer);
  void *buffer_page;

  unsigned readsize = (unsigned)(buffer_start + PGSIZE - buffer);
  unsigned bytes_read = 0;
  
  for (buffer_page = buffer_start; buffer_page <= buffer + size; buffer_page += PGSIZE)
  {
    struct spt_entry *page = get_page_from_hash(buffer_page);
    if (page == NULL) /* Page not found */
    {
      load_extra_stack_page(buffer_page);
      byteCount++;
    }
    else if (page->page_status == 2) /* Page in filesys */
    {
      load_file_to_spt(page);
      byteCount++;
    }
    else if (page->page_status == 1 ) {
        load_swap_to_spt(page);
        byteCount++;
    }
    else if (page->page_status == 0 ) {
      load_mmap_to_spt(page);
      byteCount++;
    }
  }
  /* If fd == 0, reads from keyboard using input_getc() */
  if (fd == 0)
  {
    while ((unsigned int)byteCount < size)
    {
      *((char *)buffer + byteCount) = input_getc();
      byteCount++;
    }
    return size;
  }

  /* fd is not 0, so read it */
  // struct file *filePtr = thread_current()->fdToFile[fd - 2];
  struct file *filePtr = fd2->file;
  if (filePtr == NULL)
    return -1;


  if (buffer_start == (void *)0x08048000)
  {
    thread_exit(-1);
  }
  else if (size <= readsize)
  {

    success = file_read(filePtr, buffer, size);
  }
  else
  {
    bool stillReading = true;
    while (stillReading)
    {
      bytes_read = file_read(filePtr, buffer, readsize);

      if (bytes_read != readsize)
      {
        stillReading = false;
      }

      size -= bytes_read;
      if (size == 0)
      {
        stillReading = false;
      }
      else
      {
        buffer += bytes_read;
        if (size >= PGSIZE)
        {
          readsize = PGSIZE;
        }
        else
        {
          readsize = size;
        }
      }

      success += bytes_read;
    }
  }
  lock_release(&file_lock);
  return success;
}
/*
 * Writes size bytes from buffer to the open file fd.
 * Returns: the number of bytes actually written (which may be less than size if some bytes could not be written).
 */
int write(int fd, const void *buffer, unsigned size)
{
  if (!lock_held_by_current_thread(&file_lock))
  {
    lock_acquire(&file_lock);
  }

  if (buffer == NULL || !validate_pointer(buffer))
    thread_exit(-1);
  if (fd < 1 || fd > 1025)
    return -1; 
  int ret = -1;
  if (fd == 1)
  {
    putbuf(buffer, size);
    ret = size;
  }
  else
  {
    struct file_descriptor *fd2 = find_fd(fd);
    if (fd2 == NULL)
    {
      lock_release(&file_lock);
      return -1;
    }
    struct file *file = fd2->file;
    if (file == NULL)
    {
      lock_release(&file_lock);
      return -1;
    }
    ret = file_write(file, buffer, size);

    
    
  }
  lock_release(&file_lock);
  return ret;
}

/*
 * Changes the next byte to be read or written in open file fd to position,
 * expressed in bytes from the beginning of the file.
 * (Thus, a position of 0 is the file's start.)
 */
void seek(int fd, unsigned position)
{
  lock_file();
  // struct file *fileDes = thread_current()->fdToFile[fd - 2];
  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    unlock_file();
    return;
  }
  struct file *fileDes = fd2->file;

  if (fileDes == NULL)
    return;

  file_seek(fileDes, position);
  // file_seek(thread_current()->fdToFile[fd - 2], position);
  unlock_file();
}
/*
 * Returns: the position of the next byte to be read or written in open file fd
 */
unsigned tell(int fd)
{
  lock_acquire(&file_lock);
  // struct file *fileDes = thread_current()->fdToFile[fd - 2];
  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    lock_release(&file_lock);
    return -1;
  }
  struct file *fileDes = fd2->file;
  if (fileDes == NULL)
    return -1;

  unsigned pos = file_tell(fileDes);
  lock_release(&file_lock);
  return pos;
}

/*
 * Closes file descriptor fd.
 */
void close(int fd)
{
  lock_acquire(&file_lock);
  if (fd < 2 || fd > 1025)
    return;

  // struct file *fileDes = thread_current()->fdToFile[fd - 2];
  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    lock_release(&file_lock);
    return;
  }
  struct file *fileDes = fd2->file;
  if (fileDes == NULL)
    return;

  /* Closing file using file sys function */
  file_close(fileDes);
  // thread_current()->fdToFile[fd - 2] = NULL;
  // remove from list and free
  struct list_elem *e;
  for (e = list_begin(&thread_current()->fdToFile); e != list_end(&thread_current()->fdToFile); e = list_next(e))
  {
    struct file_descriptor *fd3 = list_entry(e, struct file_descriptor, elem);
    if (fd3 == fd2)
    {
      list_remove(e);
      free(fd2);
      break;
    }
  }
  lock_release(&file_lock);
}

/*
 * This checks the address of a given pointer to validate it.
 * Returns 0 if not valid
 */
bool validate_pointer(const void *givenPointer)
{
  if (!is_user_vaddr(givenPointer))
    return false;

  void *point = (void *)pagedir_get_page(thread_current()->pagedir, givenPointer);
  if (point == NULL)
    return false;

  return true;
}

/*
 * VM mmap
 */
mapid_t mmap(int fd, void *addr)
{
  if (addr == NULL || is_user_vaddr(addr) != true || fd <= 1)
  {
    return -1;
  }
  if ((int)addr % PGSIZE != 0)
  {
    return -1;
  }
  lock_acquire(&file_lock);

  /* Open File */
  // struct file *file = curr->fdToFile[fd - 2];
  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    lock_release(&file_lock);
    return -1;
  }
  struct file *file = fd2->file;
  if (file == NULL)
  {
    lock_release(&file_lock);
    return -1;
  }
  struct file *fileCopy = file_reopen(file);
  if (fileCopy == NULL)
  {
    lock_release(&file_lock);
    return -1;
  }
  off_t length_of_file = file_length(fileCopy);
  if (length_of_file <= 0)
  {
    lock_release(&file_lock);
    return -1;
  }
  lock_release(&file_lock);

  int num_of_page = length_of_file / PGSIZE + 1;
  for (int i = 0; i < num_of_page; i++)
  {
    void *pg_addr = addr + (PGSIZE * i);
    if (get_page_from_hash(pg_addr) != NULL)
    {
      return -1;
    }
  }

  thread_current()->num_mapped++;
  mapid_t id = thread_current()->num_mapped;
  off_t offset = 0;
  uint32_t read_bytes = length_of_file;

  while (read_bytes > 0)
  {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    struct spt_entry *page = malloc(sizeof(struct spt_entry));
    if (page == NULL)
    {
      return -1;
    }

    page->file = fileCopy;
    page->offset = offset;
    page->vaddr = addr;
    page->bytes_read = page_read_bytes;
    page->bytes_zero = PGSIZE - page_read_bytes;
    page->writable = true;
    page->page_status = 2;
    page->pagedir = thread_current()->pagedir;

    lock_acquire(&thread_current()->spt_lock);
    hash_insert(&thread_current()->spt, &page->elem);
    lock_release(&thread_current()->spt_lock);

    if (put_mmap_in_list(page) == false)
    {
      munmap(id);
      return -1;
    }

    read_bytes -= page_read_bytes;
    addr += PGSIZE;
    offset += PGSIZE;
  }
  return id;
}


/*
 * VM munmap
 */
bool munmap(mapid_t mapping)
{
  lock_acquire(&file_lock);
  if (mapping <= 0)
  {
    lock_release(&file_lock);
    return false;
  }
  struct list *map_list = &(thread_current()->mmap_list);
  struct list_elem *e = list_begin(map_list);
  for (e = list_begin(map_list); e != list_end(map_list); e = e)
  {
    struct mapped_item *mmapped = list_entry(e, struct mapped_item, elem);

    if (mmapped->id == mapping)
    {

      struct spt_entry *page = mmapped->page;
      file_seek(page->file, 0);

      if (pagedir_is_dirty(thread_current()->pagedir, page->vaddr))
      {
        file_write_at(page->file, page->vaddr, page->bytes_read, page->offset);
      }

      lock_acquire(&thread_current()->spt_lock);
      hash_delete(&thread_current()->spt, &page->elem);
      lock_release(&thread_current()->spt_lock);

      e = list_remove(&mmapped->elem);
      free(mmapped);
    }
    else
    {
      e = list_next(e);
    }
  }
  lock_release(&file_lock);
  thread_current()->num_mapped--;
  return true;
}

/*
 * Helper for mmap
 * Puts page in mmap list
 */
bool put_mmap_in_list(struct spt_entry *page)
{
  struct mapped_item *mmapped = malloc(sizeof(struct mapped_item));
  if (mmapped == NULL)
  {
    return false;
  }

  struct thread *t = thread_current();
  mmapped->page = page;
  mmapped->id = t->num_mapped;
  list_push_back(&t->mmap_list, &mmapped->elem);
  return true;
}

/*
 * Changes the current working directory of the process to dir, which may be relative or absolute. Returns true if successful, false on failure. 
 */
bool chdir (const char *dir) {
  return filesys_chdir(dir);
}

/*
 * Creates the directory named dir, which may be relative or absolute. Returns true if successful, false on failure. 
 * Fails if dir already exists or if any directory name in dir, besides the last, does not already exist.
 * That is, mkdir("/a/b/c") succeeds only if "/a/b" already exists and "/a/b/c" does not. 
 */
bool mkdir (const char *dir) {
  if (dir == NULL || !validate_pointer(dir))
  {
    return false;
  }
  return filesys_create(dir, 0, true);
}

/*
 * Reads a directory entry from file descriptor fd, which must represent a directory.
 * If successful, stores the null-terminated file name in name, which must have room for READDIR_MAX_LEN + 1 bytes, and returns true.
 * If no entries are left in the directory, returns false.
 * 
 * "." and ".." should not be returned by readdir.
 *
 * If the directory changes while it is open, then it is acceptable for some entries not to be read at all or to be read multiple times.
 * Otherwise, each directory entry should be read once, in any order.
 * 
 * READDIR_MAX_LEN is defined in "lib/user/syscall.h".
 * If your file system supports longer file names than the basic file system, you should increase this value from the default of 14. 
 */
bool readdir (int fd, char *name) {
  if (fd < 2 || fd > 1025)
  {
    return false;
  }
  // printf("fd is %d in syscall\n", fd);
  // struct file *file = thread_current()->fdToFile[fd - 2];
  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    // printf("fd2 is null\n");
    return false;
  }
  struct dir *dir = fd2->dir;
  if (dir == NULL)
  {
    return false;
  }
  if (dir_readdir(dir, name))
  {
    return true;
  }
  // inode_close(dir_get_inode(dir));
  return false;
  

}

/* 
 * Returns true if fd represents a directory, false if it represents an ordinary file. 
 */
bool isdir (int fd) {
  if (fd < 2 || fd > 1025)
  {
    // printf("fd is %d\n", fd);
    return false;
  }
  if (fd == 3)
    return true;
  // struct file *file = thread_current()->fdToFile[fd - 2];
  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    // printf("fd2 is null\n");
    return false;
  }
  struct file *file = fd2->file;
  if (file == NULL)
  {
    // printf("file is null\n");
    return false;
  }
  struct inode *inode = file_get_inode(file);
  if (inode == NULL)
  {
    // printf("inode is null\n");
    return false;
  }
  if (inode_is_directory(inode))
  {
    return true;
  }
  return false;
  
}

/*
 * Returns the inode number of the inode associated with fd, which may represent an ordinary file or a directory.
 *
 * An inode number persistently identifies a file or directory. It is unique during the file's existence.
 * In Pintos, the sector number of the inode is suitable for use as an inode number. 
 */
int inumber (int fd) {
  if (fd < 2 || fd > 1025)
  {
    return -1;
  }
  // struct file *file = thread_current()->fdToFile[fd - 2];
  struct file_descriptor *fd2 = find_fd(fd);
  if (fd2 == NULL)
  {
    return -1;
  }
  struct file *file = fd2->file;
  if (file == NULL)
  {
    return -1;
  }
  struct inode *inode = file_get_inode(file);
  if (inode == NULL)
  {
    return -1;
  }
  int inumber = inode_get_inumber(inode);
  return inumber;
}


static struct file_descriptor *find_fd(int fd) {
  struct thread *t = thread_current();
  struct list_elem *e;
  for (e = list_begin(&t->fdToFile); e != list_end(&t->fdToFile); e = list_next(e))
  {
    struct file_descriptor *fileDes = list_entry(e, struct file_descriptor, elem);
    if (fileDes->fd == fd)
    {
      return fileDes;
    }
  }
  return NULL;
}