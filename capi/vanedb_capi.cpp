#include "capi/vanedb_capi.h"
#include "core/vector_store.h"
#include "core/hnsw_index.h"
#include "core/mmap_vector_store.h"

using namespace vanedb;

namespace {
DistanceMetric to_metric(vanedb_metric m) {
  switch (m) {
    case VANEDB_COSINE: return DistanceMetric::COSINE;
    case VANEDB_DOT:    return DistanceMetric::DOT;
    case VANEDB_L2:
    default:            return DistanceMetric::L2;
  }
}
} // namespace

extern "C" {

float vanedb_cpp_l2_sq(const float* a, const float* b, size_t dim) {
  return l2_sq(a, b, dim);
}
float vanedb_cpp_cosine_distance(const float* a, const float* b, size_t dim) {
  return cosine_distance(a, b, dim);
}
float vanedb_cpp_dot_product(const float* a, const float* b, size_t dim) {
  return dot_product(a, b, dim);
}

vanedb_cpp_store* vanedb_cpp_store_new(size_t dim, vanedb_metric metric) {
  try { return reinterpret_cast<vanedb_cpp_store*>(new VectorStore(dim, to_metric(metric))); }
  catch (...) { return nullptr; }
}
int vanedb_cpp_store_add(vanedb_cpp_store* s, uint64_t id, const float* v) {
  if (!s) return 1;
  try { reinterpret_cast<VectorStore*>(s)->add(id, v); return 0; }
  catch (...) { return 1; }
}
size_t vanedb_cpp_store_search(vanedb_cpp_store* s, const float* q, size_t k,
                               uint64_t* out_ids, float* out_dists) {
  if (!s) return 0;
  try {
    auto res = reinterpret_cast<VectorStore*>(s)->search(q, k);
    size_t n = res.size() < k ? res.size() : k;
    for (size_t i = 0; i < n; ++i) { out_ids[i] = res[i].id; out_dists[i] = res[i].distance; }
    return n;
  } catch (...) { return 0; }
}
void vanedb_cpp_store_free(vanedb_cpp_store* s) {
  delete reinterpret_cast<VectorStore*>(s);
}

vanedb_cpp_hnsw* vanedb_cpp_hnsw_new(size_t dim, vanedb_metric metric, size_t capacity,
                                     size_t M, size_t ef_construction, uint64_t seed) {
  try {
    return reinterpret_cast<vanedb_cpp_hnsw*>(
      // seed is uint64_t in the ABI (Rust parity) but the core takes uint32_t; high bits are dropped.
      new HNSWIndex(dim, to_metric(metric), capacity, M, ef_construction,
                    static_cast<uint32_t>(seed)));
  } catch (...) { return nullptr; }
}
int vanedb_cpp_hnsw_add(vanedb_cpp_hnsw* h, uint64_t id, const float* v) {
  if (!h) return 1;
  try { reinterpret_cast<HNSWIndex*>(h)->add(id, v); return 0; }
  catch (...) { return 1; }
}
size_t vanedb_cpp_hnsw_search(vanedb_cpp_hnsw* h, const float* q, size_t k, size_t ef_search,
                              uint64_t* out_ids, float* out_dists) {
  if (!h) return 0;
  try {
    auto* idx = reinterpret_cast<HNSWIndex*>(h);
    idx->set_ef_search(ef_search);
    auto res = idx->search(q, k);
    size_t n = res.size() < k ? res.size() : k;
    for (size_t i = 0; i < n; ++i) { out_ids[i] = res[i].id; out_dists[i] = res[i].distance; }
    return n;
  } catch (...) { return 0; }
}
int vanedb_cpp_hnsw_save(vanedb_cpp_hnsw* h, const char* path) {
  if (!h) return 1;
  if (!path) return 1;
  try { reinterpret_cast<HNSWIndex*>(h)->save(path); return 0; }
  catch (...) { return 1; }
}
vanedb_cpp_hnsw* vanedb_cpp_hnsw_load(const char* path) {
  if (!path) return nullptr;
  try { return reinterpret_cast<vanedb_cpp_hnsw*>(HNSWIndex::load(path).release()); }
  catch (...) { return nullptr; }
}
void vanedb_cpp_hnsw_free(vanedb_cpp_hnsw* h) {
  delete reinterpret_cast<HNSWIndex*>(h);
}

int vanedb_cpp_mmap_build(const char* path, size_t dim, vanedb_metric metric,
                          const uint64_t* ids, const float* vecs, size_t n) {
  if (!path) return 1;
  try {
    MMapVectorStoreBuilder b(dim, to_metric(metric));
    for (size_t i = 0; i < n; ++i) b.add(ids[i], vecs + i * dim);
    b.save(path);
    return 0;
  } catch (...) { return 1; }
}
vanedb_cpp_mmap* vanedb_cpp_mmap_open(const char* path) {
  if (!path) return nullptr;
  try { return reinterpret_cast<vanedb_cpp_mmap*>(new MMapVectorStore(path)); }
  catch (...) { return nullptr; }
}
size_t vanedb_cpp_mmap_search(vanedb_cpp_mmap* m, const float* q, size_t k,
                              uint64_t* out_ids, float* out_dists) {
  if (!m) return 0;
  try {
    auto res = reinterpret_cast<MMapVectorStore*>(m)->search(q, k);
    size_t n = res.size() < k ? res.size() : k;
    for (size_t i = 0; i < n; ++i) { out_ids[i] = res[i].id; out_dists[i] = res[i].distance; }
    return n;
  } catch (...) { return 0; }
}
void vanedb_cpp_mmap_free(vanedb_cpp_mmap* m) {
  delete reinterpret_cast<MMapVectorStore*>(m);
}

}
