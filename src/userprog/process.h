#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"


tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (int status);
void process_activate (void);
// unsigned page_hash(const struct hash_elem *elem1, void *aux UNUSED);
// bool is_page_before(const struct hash_elem *elem1, const struct hash_elem *elem2, void *aux UNUSED);
// void destroy_page(struct hash_elem *elem1, void *aux UNUSED);
#endif /* userprog/process.h */
