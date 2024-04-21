#include "userprog/exception.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "threads/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill(struct intr_frame *);
static void page_fault(struct intr_frame *);
/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void exception_init(void)
{
   /* These exceptions can be raised explicitly by a user program,
      e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
      we set DPL==3, meaning that user programs are allowed to
      invoke them via these instructions. */
   intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
   intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
   intr_register_int(5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

   /* These exceptions have DPL==0, preventing user processes from
      invoking them via the INT instruction.  They can still be
      caused indirectly, e.g. #DE can be caused by dividing by
      0.  */
   intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
   intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
   intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
   intr_register_int(7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
   intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
   intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
   intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
   intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
   intr_register_int(19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

   /* Most exceptions can be handled with interrupts turned on.
      We need to disable interrupts for page faults because the
      fault address is stored in CR2 and needs to be preserved. */
   intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void exception_print_stats(void)
{
   printf("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill(struct intr_frame *f)
{
   /* This interrupt is one (probably) caused by a user process.
      For example, the process might have tried to access unmapped
      virtual memory (a page fault).  For now, we simply kill the
      user process.  Later, we'll want to handle page faults in
      the kernel.  Real Unix-like operating systems pass most
      exceptions back to the process via signals, but we don't
      implement them. */

   /* The interrupt frame's code segment value tells us where the
      exception originated. */
   switch (f->cs)
   {
   case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf("%s: dying due to interrupt %#04x (%s).\n",
             thread_name(), f->vec_no, intr_name(f->vec_no));
      intr_dump_frame(f);
      thread_exit(-1);

   case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame(f);
      PANIC("Kernel bug - unexpected interrupt in kernel");

   default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name(f->vec_no), f->cs);
      thread_exit(-1);
   }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault(struct intr_frame *f)
{
   bool not_present; /* True: not-present page, false: writing r/o page. */
   bool write;       /* True: access was write, false: access was read. */
   // bool user;        /* True: access by user, false: access by kernel. */
   void *fault_addr; /* Fault address. */

   /* Obtain faulting address, the virtual address that was
      accessed to cause the fault.  It may point to code or to
      data.  It is not necessarily the address of the instruction
      that caused the fault (that's f->eip).
      See [IA32-v2a] "MOV--Move to/from Control Registers" and
      [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
      (#PF)". */
   asm("movl %%cr2, %0" : "=r"(fault_addr));

   /* Turn interrupts back on (they were only off so that we could
      be assured of reading CR2 before it changed). */
   intr_enable();

   /* Counts page faults. */
   page_fault_cnt++;
   struct thread *t = thread_current();
   if (fault_addr == NULL || is_kernel_vaddr(fault_addr))
   {
      goto exit;
   }
   /* Pointer is good so get the page with it */
   
   //if (!lock_held_by_current_thread(&t->parent_process->spt_lock))
   //{
    lock_frame();
    lock_acquire(&t->pcb->spt_lock);
   //}

   struct spt_entry *page = get_page_from_hash(fault_addr);

   if (page == NULL) /* Page not found */
   {
      // uint32_t *esp = f->esp;
      /* if its not in stack range */
      if (((PHYS_BASE - pg_round_down(fault_addr)) <= (1 << 23) && fault_addr >= (f->esp - 32)))
      {
         load_extra_stack_page(fault_addr);
         lock_release(&t->pcb->spt_lock);
         unlock_frame();
      }
      else
      {
         lock_release(&t->pcb->spt_lock);
         unlock_frame();
         thread_exit(-1);
      }
      return;
   }
   if (page->page_status == 2) /* in filesys */
   {
      load_file_to_spt(page);
      lock_release(&t->pcb->spt_lock);
      unlock_frame();
      return;
   }
   if (page->page_status == 1) /* in swap table */
   {
      load_swap_to_spt(page);
      lock_release(&t->pcb->spt_lock);
      unlock_frame();
      return;
   }
   if (page->page_status == 0) /* mmapped file */
   {
      load_mmap_to_spt(page);
      lock_release(&t->pcb->spt_lock);
      unlock_frame();
      return;
   }
   lock_release(&t->pcb->spt_lock);
   unlock_frame();
   if (pagedir_get_page(t->pcb->pagedir, fault_addr) == NULL)
   {
      goto exit;
   }
   /* Determine cause. */
exit:
   not_present = (f->error_code & PF_P) == 0;
   write = (f->error_code & PF_W) != 0;

   if (!not_present && write)
   {
      thread_exit(-1);
   }

   kill(f);
}

void load_swap_to_spt(struct spt_entry *page)
{
   page->pinned = true;
   page->frame = find_frame(page);
   
   if (page->frame == NULL)
   {
      thread_exit(-1);
   }

   if (!install_page(page->vaddr, page->frame->paddr, page->writable))
   {
      thread_exit(-1);
   }

   swap_get(page);

   page->page_status = 3;

   page->pinned = false;
   unlock_frame();
}

void load_mmap_to_spt(struct spt_entry *page)
{
   page->pinned = true;
   struct frame *new_frame = find_frame(page);
   
   if (new_frame == NULL)
   {
      thread_exit(-1);
   }

   file_seek(page->file, page->offset);
   if (file_read(page->file, new_frame, page->bytes_read) != (int)page->bytes_read)
   {
      palloc_free_page(new_frame->page);

      thread_exit(-1);
   }
   memset(new_frame + page->bytes_read, 0, page->bytes_zero);

   if (!install_page(page->vaddr, new_frame->paddr, page->writable))
   {
      thread_exit(-1);
   }

   page->page_status = 3;
   page->pinned = false;
   unlock_frame();
}

/*
   loads a file into the spt by reading the files data into a frame's paddr
*/
void load_file_to_spt(struct spt_entry *page)
{
   page->pinned = true;
   struct frame *new_frame = find_frame(page);
   
   if (new_frame == NULL)
   {
      thread_exit(-1);
      return;
   }

   if (!install_page(page->vaddr, new_frame->paddr, page->writable))
   {
      thread_exit(-1);
   }

   if (page->bytes_read != 0)
   {
      file_seek(page->file, page->offset);
      if (file_read(page->file, new_frame->paddr, page->bytes_read) != (int)page->bytes_read)
      {
         palloc_free_page(new_frame->page);
         thread_exit(-1);
      }
      /* memset the kpage + bytes read */

      /* TODO: */
      
   }
   memset(new_frame->paddr + page->bytes_read, 0, page->bytes_zero); /* make sure page has memory correct range */
   page->page_status = 3; /* in frame table */
   page->pinned = false;
   unlock_frame();
}

/*
   Creates a new page to put into the spt and a frame for a stack frame
*/
void load_extra_stack_page(void *fault_addr)
{
   struct spt_entry *new_page = (struct spt_entry *)malloc(sizeof(struct spt_entry));
   if (new_page == NULL)
   {
      thread_exit(-2);
   }
   /* Creates a new page */
   new_page->is_stack = true;
   new_page->vaddr = pg_round_down(fault_addr);
   new_page->page_status = 3;
   new_page->writable = true;
   new_page->pinned = false;
   new_page->file = NULL;
   new_page->offset = 0;
   new_page->bytes_read = 0;
   new_page->pagedir = thread_current()->pcb->pagedir;
   new_page->swap_index = -1;

   if (thread_current()->pcb->num_stack_pages > 2048)
   {
      lock_release(&thread_current()->pcb->spt_lock);
      unlock_frame();
      thread_exit(-3);
   }
   hash_insert(&thread_current()->pcb->spt, &new_page->elem);
   thread_current()->pcb->num_stack_pages++;
   hash_insert(&thread_current()->pcb->spt, &new_page->elem);
   thread_current()->pcb->num_stack_pages++;
   struct frame *new_frame = find_frame(new_page);
   if (new_frame == NULL)
   {

      lock_release(&thread_current()->pcb->spt_lock);
      unlock_frame();
      thread_exit(-4);
   }

   /* Install */
   if (!install_page(new_page->vaddr, new_frame->paddr, new_page->writable))
   {
      PANIC("Error growing stack page!");
   }
}