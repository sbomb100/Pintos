#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/gdt.h"
#include "userprog/pagedir.h"
#include "threads/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "lib/kernel/hash.h"

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);
struct process *find_child(pid_t child_pid);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t process_execute(const char *file_name)
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL)
  {
    palloc_free_page(fn_copy);
    return TID_ERROR;
  }
  strlcpy(fn_copy, file_name, PGSIZE);

  tid = thread_create(file_name, NICE_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR) {
    palloc_free_page(fn_copy);
    return tid;
  }
  else {
    struct process * child = find_child((pid_t) tid);
    sema_down(&child->wait_sema);

    if ( child->status == PROCESS_ABORT ) {
        lock_acquire(&thread_current()->children_lock);
        list_remove(&child->elem);
        lock_release(&thread_current()->children_lock);
        free(child);
        tid = -1;
    }
  }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process(void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  thread_current()->fdToFile = malloc(128 * sizeof(struct file *));
  if (thread_current()->fdToFile == NULL)
  {
    printf("fail 69\n");
    thread_exit(-1);
  }

  for (int i = 0; i < 128; i++)
  {
    thread_current()->fdToFile[i] = NULL;
  }

  /* Initialize interrupt frame and load executable. */
  memset(&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load(file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page(file_name);
  if (!success)
  {
    thread_current()->parent->status = PROCESS_ABORT;
    thread_exit(-1);
  }
  sema_up(&thread_current()->parent->wait_sema);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(&if_) : "memory");
  NOT_REACHED();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int process_wait(tid_t child_tid)
{
  /* Finds the child */
  struct process *cur_child = find_child((pid_t) child_tid);

  /* exits if no child was found */
  if (cur_child == NULL)
  {
    return -1;
  }
  
  lock_acquire(&thread_current()->children_lock);
  list_remove(&cur_child->elem);
  lock_release(&thread_current()->children_lock);
  sema_down(&cur_child->wait_sema);
  int exit_status = cur_child->exit_status;
  free(cur_child);
  return exit_status;
}

/* Free the current process's resources. */
void process_exit(int status)
{
  struct thread *cur = thread_current();

  while (cur->num_mapped != 0)
  {
    munmap(cur->num_mapped);
  }

  /* Process Termination Message */
  char *tmp;
  printf("%s: exit(%d)\n", strtok_r(cur->name, " ", &tmp), status);
  
  lock_file();
  if ( cur->exec_file != NULL ) {
    file_allow_write(cur->exec_file);
  }
  file_close(cur->exec_file);
  unlock_file();
  
  /* Mark orphanized child processes */
  lock_acquire(&cur->children_lock);
  for ( struct list_elem * e = list_begin(&cur->children); e != list_end(&cur->children);) {
    struct process * p = list_entry(e, struct process, elem);
    lock_acquire(&p->process_lock);
    if ( p->status == PROCESS_RUNNING ) {
        p->status = PROCESS_ORPHAN;
        lock_release(&p->process_lock);
        e = list_next(e);
    }
    else {
        lock_release(&p->process_lock);
        e = list_remove(e);
        free(p);
    }
  }
  lock_release(&cur->children_lock);

  /* Cleanup semantics for orphan or child process */
  if ( cur->parent != NULL ) {
    lock_acquire(&cur->parent->process_lock);
    if ( cur->parent->status == PROCESS_ORPHAN ) {
        lock_release(&cur->parent->process_lock);
        free(cur->parent);
        cur->parent = NULL;
    }
    else {
        cur->parent->status = status == PID_ERROR ? PROCESS_ABORT : PROCESS_EXIT;
        lock_release(&cur->parent->process_lock);
        cur->parent->exit_status = status;
        sema_up(&cur->parent->wait_sema);
    }
  }

  /* Destroy the current process's spt entries */
  lock_acquire(&cur->spt_lock);
  hash_destroy(&cur->spt, destroy_page);
  lock_release(&cur->spt_lock);
}


/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void process_activate(void)
{
  struct thread *t = thread_current();

  /* Activate thread's page tables. */
  pagedir_activate(t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
  unsigned char e_ident[16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

static bool setup_stack(void **esp);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool load(const char *file_name, void (**eip)(void), void **esp)
{
  struct thread *t = thread_current();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  char *fn_copy;
  char **argv;
  int argc = 0;

  fn_copy = malloc(strlen(file_name) + 1);
  if (fn_copy == NULL)
  {
    printf("fail 299 p\n");
    return false;
  }

  argv = malloc(4096);
  if (argv == NULL)
  {
    printf("fail 306 p\n");
    free(fn_copy);
    return false;
  }

  strlcpy(fn_copy, file_name, PGSIZE);

  char *token, *save_ptr;
  token = strtok_r(fn_copy, " ", &save_ptr);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create();
  if (t->pagedir == NULL)
    goto done;
  process_activate();

  /* Open executable file. */
  lock_file();
  file = filesys_open(token);
  if (file == NULL)
  {
    unlock_file();
    printf("load: %s: open failed\n", token);
    goto done;
  }
  unlock_file();
  /* Read and verify executable header. */
  if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 3 || ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024)
  {
    printf("load: %s: error loading executable\n", token);
    goto done;
  }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
  {
    struct Elf32_Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length(file))
      goto done;
    file_seek(file, file_ofs);

    if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
      goto done;
    file_ofs += sizeof phdr;
    switch (phdr.p_type)
    {
    case PT_NULL:
    case PT_NOTE:
    case PT_PHDR:
    case PT_STACK:
    default:
      /* Ignore this segment. */
      break;
    case PT_DYNAMIC:
    case PT_INTERP:
    case PT_SHLIB:
      goto done;
    case PT_LOAD:
      if (validate_segment(&phdr, file))
      {
        bool writable = (phdr.p_flags & PF_W) != 0;
        uint32_t file_page = phdr.p_offset & ~PGMASK;
        uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
        uint32_t page_offset = phdr.p_vaddr & PGMASK;
        uint32_t read_bytes, zero_bytes;
        if (phdr.p_filesz > 0)
        {
          /* Normal segment.
             Read initial part from disk and zero the rest. */
          read_bytes = page_offset + phdr.p_filesz;
          zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
        }
        else
        {
          /* Entirely zero.
             Don't read anything from disk. */
          read_bytes = 0;
          zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
        }
        if (!load_segment(file, file_page, (void *)mem_page,
                          read_bytes, zero_bytes, writable))
          goto done;
      }
      else
        goto done;
      break;
    }
  }

  /* Set up stack. */
  if (!setup_stack(esp))
  {
    goto done;
  }

  for (; token != NULL; token = strtok_r(NULL, " ", &save_ptr))
  {
    *esp -= strlen(token) + 1;

    memcpy(*esp, token, strlen(token) + 1);
    argv[argc] = *esp;
    argc++;
  }

  *esp = (void *)((int32_t)*esp & (~3));
  *esp -= sizeof(char *);

  for (int i = argc - 1; i >= 0; i--)
  {
    *esp -= sizeof(char *);
    memcpy(*esp, argv + i, sizeof(char *));
  }

  char *temp = *esp;
  *esp -= sizeof(char **);
  memcpy(*esp, &temp, sizeof(char **));

  *esp -= sizeof(char *);
  memcpy(*esp, &argc, sizeof(int));

  *esp -= sizeof(char *);

  /* Start address. */
  *eip = (void (*)(void))ehdr.e_entry;

  success = true;

  if (success)
  {
    file_deny_write(file);
    thread_current()->exec_file = file;
  }
  else
  {
    file_close(file);
  }

done:
  /* We arrive here whether the load is successful or not. */
  free(fn_copy);
  free(argv);
  return success;
}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment(const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off)file_length(file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr((void *)phdr->p_vaddr))
    return false;
  if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
             uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(ofs % PGSIZE == 0);

  struct thread *t = thread_current();

  while (read_bytes > 0 || zero_bytes > 0)
  {
    /* Calculate how to fill this page.
       We will read PAGE_READ_BYTES bytes from FILE
       and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
    
    /* Creates a page*/
    struct spt_entry *page = (struct spt_entry *)malloc(sizeof(struct spt_entry));
    if (page == NULL)
    {
      return false;
    }
    page->vaddr = upage;
    page->is_stack = false;
    page->frame = NULL;
    page->file = file;
    page->offset = ofs;
    page->page_status = 2; /* file page */
    page->writable = writable;
    page->bytes_read = page_read_bytes;
    page->bytes_zero = page_zero_bytes;
    page->pagedir = t->pagedir;
    page->swap_index = -1;
    lock_acquire(&t->spt_lock);
    hash_insert(&t->spt, &page->elem);
    lock_release(&t->spt_lock);
    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    ofs += page_read_bytes;
    upage += PGSIZE;
  }
  file_seek(file, ofs);

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack(void **esp)
{
  bool success = false;
  void *upage = ((uint8_t *)PHYS_BASE) - PGSIZE;
  struct thread *curr = thread_current();
  /* Create a page, put it in a frame, then set stack */
  struct spt_entry *page = (struct spt_entry *)malloc(sizeof(struct spt_entry));
  if (page == NULL)
  {
    free(page);
    return false;
  }
  page->is_stack = true;
  page->vaddr = pg_round_down(upage);
  page->page_status = 3;
  page->writable = true;
  page->file = NULL;
  page->offset = 0;
  page->bytes_read = 0;
  page->pagedir = curr->pagedir;
  page->swap_index = -1;
  lock_acquire(&curr->spt_lock);
  hash_insert(&curr->spt, &page->elem);
  lock_release(&curr->spt_lock);
  thread_current()->num_stack_pages++;

  lock_frame();
  struct frame *stack_frame = find_frame(page);
  
  if (stack_frame == NULL || stack_frame->paddr == NULL)
  {
    printf("NO FRAME 604\n");
    free(page);
    unlock_frame();
    thread_exit(-1);
  }

  /* By setting kpage to the frame the rest of stack setup is good */
  success = install_page(page->vaddr, page->frame->paddr, page->writable);
  if (success)
  {
    *esp = PHYS_BASE;
  }
  else
  {
    free(page);
  }
  unlock_frame();
  return success;
}

/* Helper function for finding the relevant child */
struct process *find_child(pid_t child_tid)
{
  lock_acquire(&thread_current()->children_lock);
  for (struct list_elem * e = list_begin(&thread_current()->children); e != list_end(&thread_current()->children); e = list_next(e))
  {
    struct process *temp = list_entry(e, struct process, elem);
    if (temp->pid == child_tid)
    {
      lock_release(&thread_current()->children_lock);
      return temp;
    }
  }
  lock_release(&thread_current()->children_lock);
  return NULL;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool install_page(void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable));
}
