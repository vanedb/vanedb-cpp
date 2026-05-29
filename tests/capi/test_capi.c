#include "capi/vanedb_capi.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(VANEDB_L2 == 0);
    assert(VANEDB_COSINE == 1);
    assert(VANEDB_DOT == 2);
    printf("capi: enum OK\n");

    {
        float a[4] = {1.f, 2.f, 3.f, 4.f};
        float b[4] = {1.f, 2.f, 3.f, 5.f};
        float l2 = vanedb_cpp_l2_sq(a, b, 4);   /* (4-5)^2 = 1 */
        assert(l2 > 0.99f && l2 < 1.01f);
        float dot = vanedb_cpp_dot_product(a, b, 4); /* 1+4+9+20 = 34 */
        assert(dot > 33.9f && dot < 34.1f);
        float cos = vanedb_cpp_cosine_distance(a, a, 4); /* identical => ~0 */
        assert(cos > -0.01f && cos < 0.01f);
        printf("capi: distance OK\n");
    }

    return 0;
}
