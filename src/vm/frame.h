#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <stdbool.h>
#include <list.h>

struct frame {
	struct spt_entry * page;
	struct list_elem elem; /* List element for frame table */
    bool pinned; /* If pinned, don't evict */
	void* paddr; /* Physical address */
};

/* Methods */
void frame_init(void);
struct frame* find_frame(void);
void free_frame(struct frame *);
struct frame* evict(void);

#endif