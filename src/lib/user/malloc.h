#ifndef __LIB_USER_MALLOC_H
#define __LIB_USER_MALLOC_H

#include <stddef.h>

int malloc_init(void);

void * malloc(size_t size);
void free(void *ptr);

#endif