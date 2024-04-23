/* Basic test of sbrk system call to see if it returns contiguous memory. */

#include <stdlib.h>
#include <string.h>
#include "tests/lib.h"
#include "tests/main.h"

void test_main (void) {
    char *p = sbrk(4096);
    char *q = sbrk(4096);
    char *r = sbrk(4096);
    char *s = sbrk(4096);
    char *t = sbrk(4096);
    CHECK (p != NULL, "sbrk(4096) returned non-NULL");
    CHECK (q != NULL, "sbrk(4096) returned non-NULL");
    CHECK (r != NULL, "sbrk(4096) returned non-NULL");
    CHECK (s != NULL, "sbrk(4096) returned non-NULL");
    CHECK (t != NULL, "sbrk(4096) returned non-NULL");
    CHECK (p + 4096 == q, "sbrk(4096) returned contiguous memory");
    CHECK (q + 4096 == r, "sbrk(4096) returned contiguous memory");
    CHECK (r + 4096 == s, "sbrk(4096) returned contiguous memory");
    CHECK (s + 4096 == t, "sbrk(4096) returned contiguous memory");

}