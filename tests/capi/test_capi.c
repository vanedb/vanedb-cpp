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
        int rc_add0 = vanedb_cpp_store_add(s, 10, v0);
        assert(rc_add0 == 0);
        int rc_add1 = vanedb_cpp_store_add(s, 20, v1);
        assert(rc_add1 == 0);
        uint64_t ids[2]; float ds[2];
        size_t n = vanedb_cpp_store_search(s, q, 2, ids, ds);
        assert(n == 2);
        assert(ids[0] == 10);          /* nearest to (0.1,0.1) is (0,0) */
        assert(ds[0] <= ds[1]);        /* sorted ascending */

        /* negative paths */
        vanedb_cpp_store* s_bad = vanedb_cpp_store_new(0, VANEDB_L2); /* dim=0 => ctor throws => NULL */
        assert(s_bad == NULL);
        int rc_null_add = vanedb_cpp_store_add(NULL, 1, v0);           /* null handle guarded */
        assert(rc_null_add == 1);
        size_t n_null = vanedb_cpp_store_search(NULL, q, 2, ids, ds);  /* null handle guarded */
        assert(n_null == 0);

        vanedb_cpp_store_free(s);
        printf("capi: store OK\n");
    }

    {
        float v0[2] = {0.f, 0.f};
        float v1[2] = {1.f, 1.f};
        float q[2]  = {0.1f, 0.1f};
        vanedb_cpp_hnsw* h = vanedb_cpp_hnsw_new(2, VANEDB_L2, 100, 16, 200, 42);
        assert(h != NULL);
        int rc_hadd0 = vanedb_cpp_hnsw_add(h, 10, v0);
        assert(rc_hadd0 == 0);
        int rc_hadd1 = vanedb_cpp_hnsw_add(h, 20, v1);
        assert(rc_hadd1 == 0);
        uint64_t ids[2]; float ds[2];
        size_t n = vanedb_cpp_hnsw_search(h, q, 2, 50, ids, ds);
        assert(n == 2);
        assert(ids[0] == 10);
        int rc_save = vanedb_cpp_hnsw_save(h, "capi_hnsw.bin");
        assert(rc_save == 0);
        vanedb_cpp_hnsw_free(h);

        vanedb_cpp_hnsw* h2 = vanedb_cpp_hnsw_load("capi_hnsw.bin");
        assert(h2 != NULL);
        uint64_t ids2[1]; float ds2[1];
        size_t n2 = vanedb_cpp_hnsw_search(h2, q, 1, 50, ids2, ds2);
        assert(n2 == 1 && ids2[0] == 10);
        vanedb_cpp_hnsw_free(h2);
        /* negative paths */
        vanedb_cpp_hnsw* h_bad = vanedb_cpp_hnsw_new(0, VANEDB_L2, 100, 16, 200, 42); /* dim=0 throws => NULL */
        assert(h_bad == NULL);
        int rc_null_hadd = vanedb_cpp_hnsw_add(NULL, 1, v0);                           /* null handle guarded */
        assert(rc_null_hadd == 1);
        size_t n_null_h = vanedb_cpp_hnsw_search(NULL, q, 1, 50, ids2, ds2);           /* null handle guarded */
        assert(n_null_h == 0);
        int rc_null_save = vanedb_cpp_hnsw_save(NULL, "x.bin");                        /* null handle guarded */
        assert(rc_null_save == 1);
        printf("capi: hnsw OK\n");
        remove("capi_hnsw.bin");
    }

    {
        uint64_t ids_in[2] = {10, 20};
        float vecs[4] = {0.f, 0.f, 1.f, 1.f}; /* row-major: id10=(0,0), id20=(1,1) */
        float q[2] = {0.1f, 0.1f};
        int rc_build = vanedb_cpp_mmap_build("capi_mmap.bin", 2, VANEDB_L2, ids_in, vecs, 2);
        assert(rc_build == 0);
        vanedb_cpp_mmap* m = vanedb_cpp_mmap_open("capi_mmap.bin");
        assert(m != NULL);
        uint64_t ids[2]; float ds[2];
        size_t n = vanedb_cpp_mmap_search(m, q, 2, ids, ds);
        assert(n == 2 && ids[0] == 10);
        vanedb_cpp_mmap_free(m);
        /* negative path */
        size_t n_null_m = vanedb_cpp_mmap_search(NULL, q, 2, ids, ds); /* null handle guarded */
        assert(n_null_m == 0);
        printf("capi: mmap OK\n");
        remove("capi_mmap.bin");
    }

    {
        int rc_ns = vanedb_cpp_hnsw_save(NULL, NULL);   assert(rc_ns == 1); /* null handle */
        vanedb_cpp_hnsw* h_np = vanedb_cpp_hnsw_load(NULL); assert(h_np == NULL);
        int rc_mb = vanedb_cpp_mmap_build(NULL, 2, VANEDB_L2, NULL, NULL, 0); assert(rc_mb == 1);
        vanedb_cpp_mmap* m_np = vanedb_cpp_mmap_open(NULL); assert(m_np == NULL);
        printf("capi: null-path guards OK\n");
    }

    return 0;
}
