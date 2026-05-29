#include "capi/vanedb_capi.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(VANEDB_L2 == 0);
    assert(VANEDB_COSINE == 1);
    assert(VANEDB_DOT == 2);
    printf("capi: enum OK\n");
    return 0;
}
