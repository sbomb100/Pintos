//The header file for a frame table entry
#include <stdint.h>
#include <stdbool.h>
#include "threads/thread.h"
#include <list.h>
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"


struct frame {
	struct spt_page_entry * page;
	struct list_elem elem; //since we are using linked list
	int unused_count;
    bool pinned; //may need to be swapped into page
};

//methods

//frame table init
//allocate page into table
//evict page from table
//chose someone to evict
