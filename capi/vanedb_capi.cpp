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
// Implementations added per task below.
}
