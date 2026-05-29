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

    {
        float v0[2] = {0.f, 0.f};
        float v1[2] = {1.f, 1.f};
        float q[2]  = {0.1f, 0.1f};
        vanedb_cpp_store* s = vanedb_cpp_store_new(2, VANEDB_L2);
        assert(s != NULL);
        assert(vanedb_cpp_store_add(s, 10, v0) == 0);
        assert(vanedb_cpp_store_add(s, 20, v1) == 0);
        uint64_t ids[2]; float ds[2];
        size_t n = vanedb_cpp_store_search(s, q, 2, ids, ds);
        assert(n == 2);
        assert(ids[0] == 10);          /* nearest to (0.1,0.1) is (0,0) */
        assert(ds[0] <= ds[1]);        /* sorted ascending */

        /* negative paths */
        assert(vanedb_cpp_store_new(0, VANEDB_L2) == NULL);     /* dim=0 => ctor throws => NULL */
        assert(vanedb_cpp_store_add(NULL, 1, v0) == 1);         /* null handle guarded */
        assert(vanedb_cpp_store_search(NULL, q, 2, ids, ds) == 0); /* null handle guarded */

        vanedb_cpp_store_free(s);
        printf("capi: store OK\n");
    }

    {
        float v0[2] = {0.f, 0.f};
        float v1[2] = {1.f, 1.f};
        float q[2]  = {0.1f, 0.1f};
        vanedb_cpp_hnsw* h = vanedb_cpp_hnsw_new(2, VANEDB_L2, 100, 16, 200, 42);
        assert(h != NULL);
        assert(vanedb_cpp_hnsw_add(h, 10, v0) == 0);
        assert(vanedb_cpp_hnsw_add(h, 20, v1) == 0);
        uint64_t ids[2]; float ds[2];
        size_t n = vanedb_cpp_hnsw_search(h, q, 2, 50, ids, ds);
        assert(n == 2);
        assert(ids[0] == 10);
        assert(vanedb_cpp_hnsw_save(h, "capi_hnsw.bin") == 0);
        vanedb_cpp_hnsw_free(h);

        vanedb_cpp_hnsw* h2 = vanedb_cpp_hnsw_load("capi_hnsw.bin");
        assert(h2 != NULL);
        uint64_t ids2[1]; float ds2[1];
        size_t n2 = vanedb_cpp_hnsw_search(h2, q, 1, 50, ids2, ds2);
        assert(n2 == 1 && ids2[0] == 10);
        vanedb_cpp_hnsw_free(h2);
        /* negative paths */
        assert(vanedb_cpp_hnsw_new(0, VANEDB_L2, 100, 16, 200, 42) == NULL); /* dim=0 throws => NULL */
        assert(vanedb_cpp_hnsw_add(NULL, 1, v0) == 1);                       /* null handle guarded */
        assert(vanedb_cpp_hnsw_search(NULL, q, 1, 50, ids2, ds2) == 0);      /* null handle guarded */
        assert(vanedb_cpp_hnsw_save(NULL, "x.bin") == 1);                    /* null handle guarded */
        printf("capi: hnsw OK\n");
    }

    return 0;
}
