#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "userprog/exception.h"
#include "threads/vaddr.h"

#include "threads/cpu.h"

struct futex_object {
    struct hash_elem elem;
    void * addr;
    struct thread * t;
};

struct lock file_lock;

static void syscall_handler(struct intr_frame *);

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
    //int arg = *(int *)(p + 4);
    int arg = *(int *)(p + 1);
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
  case SYS_PTHREAD_CREATE:
  {
    if ( !parse_arguments(f, &args[0], 3)) {
        thread_exit(-1);
        return;
    }
    f->eax = sys_pthread_create((wrapper_func) args[0], (start_routine) args[1], (void *) args[2]);
    break;
  }
  case SYS_PTHREAD_EXIT:
  {
    sys_pthread_exit();
    break;
  }
  case SYS_PTHREAD_JOIN:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }
    f->eax = sys_pthread_join((tid_t) args[0]);
    break;
  }
  case SYS_SBRK:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }
    f->eax = (uint32_t) sbrk((intptr_t) args[0]);
    break;
  }
  case SYS_INITLOCK:
  {
    pthread_lock_t lock_num = thread_current()->pcb->lock_num;
    lock_init(&(thread_current()->pcb->locks[lock_num]));
    thread_current()->pcb->lock_num++;
    f->eax = (pthread_lock_t) lock_num;
    break;
  }
  case SYS_LOCK:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }
    pthread_lock_t lock_num = (pthread_lock_t) args[0];

    lock_acquire(&(thread_current()->pcb->locks[lock_num]));
    break;

  }
  case SYS_UNLOCK:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }
    pthread_lock_t lock_num = (pthread_lock_t) args[0];

    lock_release(&(thread_current()->pcb->locks[lock_num]));
    break;
  }
  case SYS_INITSEMA:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }

    pthread_sema_t sema = thread_current()->pcb->sema_num;
    sema_init(&(thread_current()->pcb->semas[sema]), args[0]);
    thread_current()->pcb->sema_num++;
    break;
  }
  case SYS_SEMAUP:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }
    pthread_sema_t sema = (pthread_sema_t) args[0];
    sema_up(&(thread_current()->pcb->semas[sema]));
    break;

  }
  case SYS_SEMADOWN:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }
    pthread_sema_t sema = (pthread_sema_t) args[0];
    sema_down(&(thread_current()->pcb->semas[sema]));
    break;
  }
  case SYS_INITCOND:
  {
    pthread_cond_t cond_num = thread_current()->pcb->cond_num;
    cond_init(&(thread_current()->pcb->conds[cond_num]));
    thread_current()->pcb->cond_num++;
    f->eax = (pthread_cond_t) cond_num;
    break;
  }
  case SYS_COND_WAIT:
  {
    if ( !parse_arguments(f, &args[0], 2)) {
        thread_exit(-1);
        return;
    }
    pthread_cond_t cond_num = (pthread_cond_t) args[0];
    pthread_lock_t lock_num = (pthread_lock_t) args[1];

    cond_wait(&(thread_current()->pcb->conds[cond_num]), &(thread_current()->pcb->locks[lock_num]));
    break;
  }
  case SYS_COND_SIGNAL:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }
    pthread_cond_t cond_num = (pthread_cond_t) args[0];

    cond_signal(&(thread_current()->pcb->conds[cond_num]), NULL);
    break;
  }
  case SYS_FUTEX_WAIT:
  {
    if ( !parse_arguments(f, &args[0], 1)) {
        thread_exit(-1);
        return;
    }

    futex_wait((void *) args[0]);
    break;
  }
  case SYS_FUTEX_WAKE:
  {
    if ( !parse_arguments(f, &args[0], 2)) {
        thread_exit(-1);
        return;
    }

    futex_wake((void *) args[0], (int) args[1]);
    break;
  }
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
  int ret = filesys_create(file, initial_size);
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
  //
  if (file == NULL || !validate_pointer(file))
  {
    thread_exit(-1);
  }

  lock_file();
  struct file *fp = filesys_open(file);
  if (fp == NULL)
  {
    unlock_file();
    return -1;
  }
  unlock_file();

  if (fp == NULL)
    thread_exit(0);

  int fd = findFdForFile();
  if (fd == -1)
  {
    file_close(fp);
    thread_exit(-1);
  }
  thread_current()->pcb->fdToFile[fd - 2] = fp;

  return fd;
}

/*
 * Returns: the size, in bytes, of the file open as fd.
 */
int filesize(int fd)
{
  lock_acquire(&file_lock);
  struct file *filePtr = thread_current()->pcb->fdToFile[fd - 2];
  int length = file_length(filePtr);
  lock_release(&file_lock);
  return length;
}
/*
 * Reads size bytes from the file open as fd into buffer.
 * Returns: the number of bytes actually read (0 at end of file) or -1 if the file could not be read (due to a condition other than end of file).
 */
int read(int fd, void *buffer, unsigned size, void *esp)
{
  if (fd < 0 || fd == 1 || fd > 1025 || thread_current()->pcb->fdToFile[fd - 2] == NULL)
  {
    return -1;
  }

  if (buffer == NULL || !is_user_vaddr(buffer))
  {
    thread_exit(-1);
  }

  if ((void *)get_page_from_hash(buffer) == NULL)
  {
    if (buffer < esp)
    {
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
    else if (page->page_status == 1)
    {
      load_swap_to_spt(page);
      byteCount++;
    }
    else if (page->page_status == 0)
    {
      load_mmap_to_spt(page);
      byteCount++;
    }
  }
  //pin it
  for (buffer_page = buffer_start; buffer_page <= buffer + size; buffer_page += PGSIZE)
  {
    struct spt_entry *page = get_page_from_hash(buffer_page);
    page->pinned = true;
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
  struct file *filePtr = thread_current()->pcb->fdToFile[fd - 2];
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
  //unpin
  for (buffer_page = buffer_start; buffer_page <= buffer + size; buffer_page += PGSIZE)
  {
    struct spt_entry *page = get_page_from_hash(buffer_page);
    page->pinned = false;
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
    //pin it
  void *buffer_start = pg_round_down(buffer);
  void *buffer_page;
  for (buffer_page = buffer_start; buffer_page <= buffer + size; buffer_page += PGSIZE)
  {
    struct spt_entry *page = get_page_from_hash(buffer_page);
    page->pinned = true;
  }

  int ret = -1;
  if (fd == 1)
  {
    putbuf(buffer, size);
    ret = size;
  }
  else
  {

    struct file *fileDes = thread_current()->pcb->fdToFile[fd - 2];
    if (fileDes != NULL)
    {
      ret = file_write(fileDes, buffer, size);
      if (ret == 0)
      {
        file_seek(fileDes, 0);
        ret = file_write(fileDes, buffer, size);
      }
    }
  }
  //unpin
  for (buffer_page = buffer_start; buffer_page <= buffer + size; buffer_page += PGSIZE)
  {
    struct spt_entry *page = get_page_from_hash(buffer_page);
    page->pinned = false;
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
  struct file *fileDes = thread_current()->pcb->fdToFile[fd - 2];

  if (fileDes == NULL)
    return;

  file_seek(thread_current()->pcb->fdToFile[fd - 2], position);
  unlock_file();
}
/*
 * Returns: the position of the next byte to be read or written in open file fd
 */
unsigned tell(int fd)
{
  lock_acquire(&file_lock);
  struct file *fileDes = thread_current()->pcb->fdToFile[fd - 2];
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

  struct file *fileDes = thread_current()->pcb->fdToFile[fd - 2];
  if (fileDes == NULL)
    return;

  /* Closing file using file sys function */
  file_close(fileDes);
  thread_current()->pcb->fdToFile[fd - 2] = NULL;
  lock_release(&file_lock);
}

/*
 * Returns the file descriptor for the file
 * index of zero is fd of 2
 * MUST BE CALLED WITH A LOCK OVER IT
 */
int findFdForFile()
{
  struct file **fdArray = thread_current()->pcb->fdToFile;

  for (int i = 0; i < 128; i++)
  {
    if (fdArray[i] == NULL)
    {
      return i + 2;
    }
  }
  return -1;
}

/*
 * This checks the address of a given pointer to validate it.
 * Returns 0 if not valid
 */
bool validate_pointer(const void *givenPointer)
{
  if (!is_user_vaddr(givenPointer))
    return false;

  void *point = (void *)pagedir_get_page(thread_current()->pcb->pagedir, givenPointer);
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
  struct thread *curr = thread_current();
  lock_acquire(&file_lock);

  /* Open File */
  struct file *file = curr->pcb->fdToFile[fd - 2];
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
  lock_acquire(&thread_current()->pcb->mmap_lock);
  thread_current()->pcb->num_mapped++;
  mapid_t id = thread_current()->pcb->num_mapped; // race cond?
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
    page->pagedir = thread_current()->pcb->pagedir;

    lock_acquire(&thread_current()->pcb->spt_lock);
    hash_insert(&thread_current()->pcb->spt, &page->elem);
    lock_release(&thread_current()->pcb->spt_lock);

    if (put_mmap_in_list(page) == false)
    {
      munmap(id);
      return -1;
    }

    read_bytes -= page_read_bytes;
    addr += PGSIZE;
    offset += PGSIZE;
  }
  lock_release(&thread_current()->pcb->mmap_lock);
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
  
  lock_acquire(&thread_current()->pcb->mmap_lock);
  struct list *map_list = &(thread_current()->pcb->mmap_list);
  struct list_elem *e = list_begin(map_list);
  for (e = list_begin(map_list); e != list_end(map_list); e = e)
  {
    struct mapped_item *mmapped = list_entry(e, struct mapped_item, elem);

    if (mmapped->id == mapping)
    {

      struct spt_entry *page = mmapped->page;
      file_seek(page->file, 0);

      if (pagedir_is_dirty(thread_current()->pcb->pagedir, page->vaddr))
      {
        file_write_at(page->file, page->vaddr, page->bytes_read, page->offset);
      }

      lock_acquire(&thread_current()->pcb->spt_lock);
      hash_delete(&thread_current()->pcb->spt, &page->elem);
      lock_release(&thread_current()->pcb->spt_lock);

      e = list_remove(&mmapped->elem);
      free(mmapped);
    }
    else
    {
      e = list_next(e);
    }
  }
  thread_current()->pcb->num_mapped--;
  lock_release(&thread_current()->pcb->mmap_lock);
  lock_release(&file_lock);
  return true;
}

/*
 * Helper for mmap
 * Puts page in mmap list
 * MUST BE CALLED WITH LOCK
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
  mmapped->id = t->pcb->num_mapped;
  list_push_back(&t->pcb->mmap_list, &mmapped->elem);
  return true;
}

tid_t sys_pthread_create(wrapper_func wf, start_routine sr, void *args) {
    return pthread_create(wf, sr, args);
}

void sys_pthread_exit() {
    pthread_exit();
}

bool sys_pthread_join(tid_t tid) {
    return pthread_join(tid);
}

void * sbrk(intptr_t increment) {
    struct thread * t = thread_current();
    struct process * pcb = t->pcb;
    void * old_break = pcb->heap_break;
    void * new_break = old_break + increment;
    if ( increment == 0 ) {
        return old_break;
    }
    if ( increment > 0 ) {
        if ( new_break > PHYS_BASE ) {
            return (void *)-1;
        }
        pcb->heap_break = new_break;
        return old_break;
    }
    else {
        void * page = pg_round_down(new_break);
        void * old_page = pg_round_down(old_break);
        while ( page < old_page ) {
            struct spt_entry * spte = get_page_from_hash(page);
            if ( spte == NULL ) {
                return (void *)-1;
            }
            lock_acquire(&pcb->spt_lock);
            hash_delete(&pcb->spt, &spte->elem);
            lock_release(&pcb->spt_lock);
            free(spte);
            page += PGSIZE;
        }
        pcb->heap_break = new_break;
        return old_break;
    }
}

void futex_wait(void * addr) {
    struct thread * t = thread_current();
    struct futex_object * f = malloc(sizeof(struct futex_object));

    f->addr = addr;
    f->t = t;

    spinlock_acquire(&t->pcb->futex_lock);
    hash_insert(&t->pcb->futex_hash, &f->elem);
    thread_block(&t->pcb->futex_lock);
    spinlock_release(&t->pcb->futex_lock);
}

void futex_wake(void * addr, int val) {
    struct thread * t = thread_current();
    struct futex_object fo;
    fo.addr = addr;

    spinlock_acquire(&t->pcb->futex_lock);
    for ( int i = 0; i < val; i++ ) {
        struct hash_elem * h = hash_delete(&t->pcb->futex_hash, &fo.elem);
        if ( h == NULL ) {
            break;
        }

        struct futex_object * f = hash_entry(h, struct futex_object, elem);
        thread_unblock(f->t);
        free(f);
    }
    spinlock_release(&t->pcb->futex_lock);
}

unsigned futex_hash(const struct hash_elem *elem, void *aux UNUSED) {
    const struct futex_object * f = hash_entry(elem, struct futex_object, elem);
    return hash_bytes(&f->addr, sizeof f->addr);
}

bool futex_less_than(const struct hash_elem *elem1, const struct hash_elem *elem2, void *aux UNUSED) {
    const struct futex_object * f_1 = hash_entry(elem1, struct futex_object, elem);
    const struct futex_object * f_2 = hash_entry(elem2, struct futex_object, elem);

    return f_1->addr < f_2->addr;
}