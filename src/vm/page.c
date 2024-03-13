#include "vm/page.h"
#include <stdio.h>
#include <stdint.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/swap.h"

// Moved to process.c
struct spt_page_entry * get_page_from_hash (void *);
// /* hash function, address comparator */
// /* Returns a hash value for spt_page_entry p. */
// unsigned
// page_hash (const struct hash_elem *elem1)
// {
//   const struct spt_page_entry *page = hash_entry (elem1, struct spt_page_entry, elem);
//   return hash_bytes (&page->vaddr, sizeof page->vaddr);
// }

// // true if spt_page_entry 1 is before 2
// bool is_page_before (const struct hash_elem *elem1, const struct hash_elem *elem2, void *aux UNUSED)
// {
//   const struct spt_page_entry *page_one = hash_entry (elem1, struct spt_page_entry, elem);
//   const struct spt_page_entry *page_two = hash_entry (elem2, struct spt_page_entry, elem);

//   return  pg_no(page_one->vaddr) < pg_no(page_two->vaddr); //where does pg_no come from
// }

// //destroy the page. clear any references as well
// void destroy_page (struct hash_elem *elem1, void *aux UNUSED)
// {
//   struct spt_page_entry *page = hash_entry (elem1, struct spt_page_entry, elem);
//   if (page->frame != NULL) { //if it has a frame, free the frame
//     struct frame_entry *f = page->frame;
//     page->frame = NULL;
//     free_frame (f);
//   }
//   if (page->swap_index != -1){
//     //remove page from swap, make reference -1
//   }
//     //this means its in the swap table, so free it from swap table
//   free (page);
// }

//search the hash table for a page, returns null if no such
struct spt_page_entry * get_page_from_hash (void *given_address)
{
  struct thread *t = thread_current ();
  struct spt_page_entry page;
  struct hash_elem *elem_in_hash;

  page.vaddr = (void *) (pg_no(given_address) << PGBITS);
  elem_in_hash = hash_find (&t->spt, &page.elem);
  return elem_in_hash != NULL ? hash_entry (elem_in_hash, struct spt_page_entry, elem) : NULL; //????
}