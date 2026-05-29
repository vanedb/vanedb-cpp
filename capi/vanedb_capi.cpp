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
    for (size_t i = 0; i < res.size(); ++i) { out_ids[i] = res[i].id; out_dists[i] = res[i].distance; }
    return res.size();
  } catch (...) { return 0; }
}
void vanedb_cpp_store_free(vanedb_cpp_store* s) {
  delete reinterpret_cast<VectorStore*>(s);
}

}
