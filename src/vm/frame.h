#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <stdbool.h>
#include <list.h>

struct frame {
	struct spt_entry * page;
	struct list_elem elem; //since we are using linked list
	int unused_count;
    bool pinned; //may need to be swapped into page
	void* paddr; //physical address
};

//methods
void frame_init(void); //frame table init
//allocate page into table
void frame_allocate_page(struct spt_entry* page);
struct frame* find_frame(void);
//evict page from table
//chose someone to evict

void free_frame(struct frame *);
struct frame* evict(void);

#endif