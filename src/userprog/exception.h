#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H
#include "vm/page.h"
/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

void exception_init (void);
void exception_print_stats (void);

//load a file into the spt
void load_swap_to_spt(struct spt_entry* );
void load_mmap_to_spt(struct spt_entry* );
void load_file_to_spt(struct spt_entry* );
//create another single stack page
void load_extra_stack_page(void*);
#endif /* userprog/exception.h */
