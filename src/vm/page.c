#include "vm/page.h"
#include <stdio.h>
#include <stdint.h>
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* Returns a hash value for spt_entry p. */
unsigned
page_hash (const struct hash_elem *elem1, void *aux UNUSED)
{
  const struct spt_entry *page = hash_entry (elem1, struct spt_entry, elem);
  return hash_bytes (&page->vaddr, sizeof page->vaddr);
}

/* True if spt_entry 1 is before 2 */
bool is_page_before (const struct hash_elem *elem1, const struct hash_elem *elem2, void *aux UNUSED)
{
  const struct spt_entry *page_one = hash_entry (elem1, struct spt_entry, elem);
  const struct spt_entry *page_two = hash_entry (elem2, struct spt_entry, elem);

  return  pg_no(page_one->vaddr) < pg_no(page_two->vaddr);
}

/* Destroy the page. Clear any references as well */
void destroy_page (struct hash_elem *elem1, void *aux UNUSED)
{
  struct spt_entry *page = hash_entry (elem1, struct spt_entry, elem);
  if ( page->frame != NULL && page == page->frame->page ) {
    page->frame->page = NULL;
  }
  if ( page->swap_index != -1 ) {
    swap_free(page);
  }
  pagedir_clear_page(page->pagedir, page->vaddr);
  free (page);
}

/* Search the hash table for a page, returns null if no such */
struct spt_entry * get_page_from_hash (void *given_address)
{
  struct thread *t = thread_current ();
  struct spt_entry page;
  struct hash_elem *elem_in_hash;

  page.vaddr = (void *) (pg_no(given_address) << PGBITS);
  elem_in_hash = hash_find (&t->spt, &page.elem);
  return elem_in_hash != NULL ? hash_entry (elem_in_hash, struct spt_entry, elem) : NULL;
}

