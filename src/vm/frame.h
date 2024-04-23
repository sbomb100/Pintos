#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <stdbool.h>
#include "lib/kernel/list.h"

struct frame {
	struct spt_entry * page;
	struct list_elem elem; /* List element for frame table */
    bool pinned; /* If pinned, don't evict */
	void* paddr; /* Physical address */
};

/* Methods */
void frame_init(void);
void lock_frame(void);
void unlock_frame(void);
struct frame* find_frame(struct spt_entry *);
void free_frame(struct frame *);
struct frame* evict(void);

#endif