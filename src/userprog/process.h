#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define THREAD_SIZE 65536 //Size of new thread stack created by pthread_create
#define TLSIZE 512 //Size of thread local storage

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (int status);
void process_activate (void);
bool install_page(void *upage, void *kpage, bool writable);
// unsigned page_hash(const struct hash_elem *elem1, void *aux UNUSED);
// bool is_page_before(const struct hash_elem *elem1, const struct hash_elem *elem2, void *aux UNUSED);
// void destroy_page(struct hash_elem *elem1, void *aux UNUSED);

tid_t pthread_create(wrapper_func, start_routine, void *);
bool pthread_join(tid_t);       
void pthread_exit(void);

#endif /* userprog/process.h */
