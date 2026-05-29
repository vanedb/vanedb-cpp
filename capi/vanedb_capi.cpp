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

}
