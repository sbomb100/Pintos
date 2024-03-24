#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "vm/page.h"

void swap_init (void);
void swap_insert (struct spt_entry *);
void swap_get (struct spt_entry *);
void swap_free (struct spt_entry *);

#endif