#ifndef VANEDB_CAPI_H
#define VANEDB_CAPI_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { VANEDB_L2 = 0, VANEDB_COSINE = 1, VANEDB_DOT = 2 } vanedb_metric;

/*
 * ABI conventions (C API v0 — unstable until a tagged release):
 *  - Handles are opaque; every constructor (_new/_open) has a matching _free.
 *  - Handle pointers are intentionally non-const (incl. read-only search/save)
 *    to keep this ABI byte-identical to the parallel Rust C API (vanedb_rs_*),
 *    which the benchmark harness calls through one uniform FFI. Do not add const.
 *  - int returns: 0 = success, non-zero = failure. Constructors return NULL on
 *    failure. _search returns the number of results written (0 on error/empty).
 *  - out_ids / out_dists are caller-owned buffers of length k.
 *  - vanedb_cpp_hnsw_search takes ef_search per call (the implementation sets it
 *    then searches). This mirrors the Rust ABI; it is not thread-safe to call
 *    concurrently with different ef_search values on the same handle, which the
 *    single-threaded benchmark consumer never does. Do not remove this parameter.
 *  - to_metric maps any unrecognized metric value to L2 (no error).
 *  - vanedb_cpp_hnsw_new: seed is uint64_t for ABI parity; only the low 32 bits are used.
 */

/* Distance (stateless) */
float vanedb_cpp_l2_sq(const float* a, const float* b, size_t dim);
float vanedb_cpp_cosine_distance(const float* a, const float* b, size_t dim);
float vanedb_cpp_dot_product(const float* a, const float* b, size_t dim);

/* VectorStore (brute force) */
typedef struct vanedb_cpp_store vanedb_cpp_store;
vanedb_cpp_store* vanedb_cpp_store_new(size_t dim, vanedb_metric metric);
int    vanedb_cpp_store_add(vanedb_cpp_store* s, uint64_t id, const float* v);
size_t vanedb_cpp_store_search(vanedb_cpp_store* s, const float* q, size_t k,
                               uint64_t* out_ids, float* out_dists);
void   vanedb_cpp_store_free(vanedb_cpp_store* s);

/* HNSW */
typedef struct vanedb_cpp_hnsw vanedb_cpp_hnsw;
vanedb_cpp_hnsw* vanedb_cpp_hnsw_new(size_t dim, vanedb_metric metric, size_t capacity,
                                     size_t M, size_t ef_construction, uint64_t seed);
int    vanedb_cpp_hnsw_add(vanedb_cpp_hnsw* h, uint64_t id, const float* v);
size_t vanedb_cpp_hnsw_search(vanedb_cpp_hnsw* h, const float* q, size_t k, size_t ef_search,
                              uint64_t* out_ids, float* out_dists);
int    vanedb_cpp_hnsw_save(vanedb_cpp_hnsw* h, const char* path);
vanedb_cpp_hnsw* vanedb_cpp_hnsw_load(const char* path);
void   vanedb_cpp_hnsw_free(vanedb_cpp_hnsw* h);

/* MMap store */
typedef struct vanedb_cpp_mmap vanedb_cpp_mmap;
int    vanedb_cpp_mmap_build(const char* path, size_t dim, vanedb_metric metric,
                             const uint64_t* ids, const float* vecs, size_t n);
vanedb_cpp_mmap* vanedb_cpp_mmap_open(const char* path);
size_t vanedb_cpp_mmap_search(vanedb_cpp_mmap* m, const float* q, size_t k,
                              uint64_t* out_ids, float* out_dists);
void   vanedb_cpp_mmap_free(vanedb_cpp_mmap* m);

#ifdef __cplusplus
}
#endif
#endif /* VANEDB_CAPI_H */
