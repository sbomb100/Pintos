//The header file for a frame table entry
#include <stdint.h>
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

