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
void load_file_to_spt(struct spt_entry *page);
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

   // get current thread?-------------------- FIX? make sure its not holding a lock?
   // printf("%x\n", (uint32_t)f->eip);

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
   //--------------------------------------------------VM addons
   // if page isnt in SPT, make new page for it, *(stack page!)
   struct thread *t = thread_current();
   if (fault_addr == NULL || is_kernel_vaddr(fault_addr))
   { // doesnt exit or is kernel pointer
      goto exit;
   }
   // pointer is good so get the page with it
   struct spt_entry *page = get_page_from_hash(fault_addr);

   if (page == NULL) // page not found
   {
      uint32_t *esp = f->esp;
      //  if its not in stack range
      if (((PHYS_BASE - pg_round_down(fault_addr)) <= 0x800000 && (uint32_t *)fault_addr >= (esp - 32)))
      {
         load_extra_stack_page(fault_addr);
      }
      else
      {
         thread_exit(-1);
      }
      // page isnt in table, therefore make new page.
      // if addr is outside stack range then exit
      return;

      // make new additional stack page and put it in a frame
   }
   if (page->page_status == 2) // filesys
   {
      // get frame and its page
      load_file_to_spt(page);
      return;
   }
   if (page->page_status == 1) // in swap table
   {
      // get frame and its page
      struct frame *new_frame = find_frame();
      if (new_frame == NULL)
      {
         goto exit;
      }
      new_frame->page = page;
      page->frame = new_frame;

      swap_get(page); // GET PAGE FROM SWAP

      // install frame
      if (!install_page(page->vaddr, new_frame->paddr, page->writable))
      {
         goto exit;
      }
      page->page_status = 3; // in frame table

      return;
   }
   if (page->page_status == 0) // mmapped file
   {
      struct frame *new_frame = find_frame();
      new_frame->page = page;
      page->frame = new_frame;

      if (new_frame == NULL)
      {
         goto exit;
      }

      // get to spot in page
      file_seek(page->file, page->offset);
      if (file_read(page->file, new_frame, page->bytes_read) != (int)page->bytes_read)
      {

         palloc_free_page(new_frame->page);
         goto exit;
      }
      memset(new_frame + page->bytes_read, 0, page->bytes_zero);

      if (!install_page(page->vaddr, new_frame->paddr, page->writable))
      {
         goto exit;
      }
      page->page_status = 3;

      return;
   }
   // if page doesnt exist
   if (pagedir_get_page(t->pagedir, fault_addr) == NULL)
   {
      goto exit;
   }
   /* Determine cause. */
exit:
   not_present = (f->error_code & PF_P) == 0;
   write = (f->error_code & PF_W) != 0;

   // ADDED VM
   if (!not_present && write)
   {
      // goto exit;
      thread_exit(-1);
   }
   kill(f);
}
/*
   loads a file into the spt by reading the files data into a frame's paddr
*/
void load_file_to_spt(struct spt_entry *page)
{
   page->pinned = true;
   
   struct frame *new_frame = find_frame();
   
   if (new_frame == NULL)
   {
      thread_exit(-1);
      return;
   }
   uint8_t *kpage = new_frame->paddr;
   new_frame->page = page;
   if (page->bytes_read != 0)
   {
      file_seek(page->file, page->offset);
      if (file_read(page->file, new_frame->paddr, page->bytes_read) != (int)page->bytes_read)
      {
         palloc_free_page(new_frame->page);
         
         thread_exit(-1);
      }

      // mem set the kpage + bytes read
      memset(kpage + page->bytes_read, 0, page->bytes_zero); // make sure page has memory correct range
   }
   // install into a frame
   
   if (!install_page(page->vaddr, new_frame->paddr, page->writable))
   {
      thread_exit(-1);
   }
   page->page_status = 3; // in frame table
   page->frame = new_frame;
   page->pinned = false;
   return;
}
/*
   Creates a new page to put into the spt and a frame for a stack frame
*/
void load_extra_stack_page(void *fault_addr)
{
   //  if its not in stack range
   // page isnt in table, therefore make new page.
   // if addr is outside stack range then exit
   ASSERT(!lock_held_by_current_thread(&thread_current()->spt_lock));
   struct spt_entry *new_page = (struct spt_entry *)malloc(sizeof(struct spt_entry));
   if (new_page == NULL)
   { // check to see it malloced
      thread_exit(-1);
   }
   // new page
   new_page->is_stack = true;
   new_page->vaddr = pg_round_down(fault_addr);
   new_page->page_status = 3;
   new_page->writable = true;
   new_page->pinned = false;
   new_page->file = NULL;
   new_page->offset = 0;
   new_page->bytes_read = 0;
   new_page->pagedir = thread_current()->pagedir;
   new_page->swap_index = -1;

   lock_acquire(&thread_current()->spt_lock);

   hash_insert(&thread_current()->spt, &new_page->elem);
   lock_release(&thread_current()->spt_lock);

   thread_current()->num_stack_pages++;
   if (thread_current()->num_stack_pages > 2048) // hard limit
   {
      thread_exit(-1);
   }
   // get frame and put it in page
   struct frame *new_frame = find_frame();
   if (new_frame == NULL)
   {
      thread_exit(-1);
      return;
   }
   new_frame->page = new_page;
   new_page->frame = new_frame;
   /* Install */
   if (!install_page(new_page->vaddr, new_frame->paddr, new_page->writable))
   {
      PANIC("Error growing stack page!");
   }

   return;
}