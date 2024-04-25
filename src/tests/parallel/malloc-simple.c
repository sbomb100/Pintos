

#include <stdlib.h>
#include <string.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "lib/user/malloc.h"

void test_main (void) {
    //char *buffer = malloc(4072);
    char *buffer = malloc(4072);
    CHECK (buffer != NULL, "malloc(100) returned non-NULL");
    strlcpy(buffer, "hello", 100);
    CHECK (strcmp(buffer, "hello") == 0, "buffer contains 'hello'");
}