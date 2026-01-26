# QuiverDB Refactoring Checklist
**Version**: 1.0
**Date**: 2026-01-26
**Purpose**: Practical step-by-step checklist for implementing refactoring plan

---

## Pre-Refactoring Setup (Week 0)

### 1. Capture Baseline Benchmarks

```bash
cd /Users/anton/code/quiverdb

# Build release version
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run all benchmarks and save results
cd build
./benchmarks/bench_distance --benchmark_min_time=0.2s --benchmark_format=json > ../benchmarks/baseline_distance.json
./benchmarks/bench_vector_store --benchmark_min_time=0.2s --benchmark_format=json > ../benchmarks/baseline_vector_store.json
./benchmarks/bench_hnsw_index --benchmark_min_time=0.2s --benchmark_format=json > ../benchmarks/baseline_hnsw.json

# Commit baselines
cd ..
git add benchmarks/baseline_*.json
git commit -m "Capture v0.1.0 performance baseline"
git push
```

**Verification**:
- [ ] Three baseline files created (distance, vector_store, hnsw)
- [ ] Baseline files committed to git
- [ ] Baseline values documented

---

### 2. Set Up Enhanced CI/CD

**Update** `.github/workflows/build-and-test.yml`:
- [ ] Add sanitizer tests (ASan, TSan, UBSan)
- [ ] Add coverage checking (≥95% threshold)
- [ ] Add benchmark comparison with baseline
- [ ] Add characterization test job

**Create** `tools/compare_benchmarks.py`:
- [ ] Script implemented (see final-implementation-plan.md)
- [ ] Tested with baseline data
- [ ] Integrated into CI workflow

**Create** `tools/verify_all.sh`:
- [ ] Script implemented
- [ ] Runs all verification steps
- [ ] Exits non-zero on failure

---

### 3. Create Testing Infrastructure

**Add** test helper for HNSW inspection:
```cpp
// In src/core/hnsw_index.h, add under #ifdef QUIVERDB_ENABLE_TESTING:

#ifdef QUIVERDB_ENABLE_TESTING
public:
  struct Inspector {
    static int get_level(const HNSWIndex& idx, size_t internal_id) {
      return idx.levels_[internal_id];
    }

    static const std::vector<size_t>& get_neighbors(
        const HNSWIndex& idx, size_t internal_id, int layer) {
      return idx.neighbors_[internal_id][layer];
    }

    static size_t get_entry_point(const HNSWIndex& idx) {
      return idx.entry_point_;
    }
  };
#endif
```

- [ ] Inspector struct added
- [ ] Compiles with `-DQUIVERDB_ENABLE_TESTING=ON`
- [ ] Accessible from test files

---

### 4. Document Current State

**Run** existing test suite:
```bash
cd build
ctest --output-on-failure --verbose > ../test_baseline.txt
```

**Count** assertions:
```bash
grep "test cases" test_baseline.txt
grep "assertions" test_baseline.txt
```

**Document**:
- [ ] Number of test cases: ___
- [ ] Number of assertions: ___
- [ ] All tests passing: YES / NO

---

## Pre-Phase 1 Approval Gate

> **BLOCKING**: Do not start Phase 1 until all approvals obtained.

### Stakeholder Sign-Off

| Role | Name | Date | Approved |
|------|------|------|----------|
| Engineering Lead | _____________ | _______ | [ ] |
| Product Manager | _____________ | _______ | [ ] |
| QA Lead | _____________ | _______ | [ ] |

### Pre-Requisites Verified

- [ ] Resource allocation confirmed (1 FTE or 2 at 50%)
- [ ] Phase 1 owner assigned: _____________
- [ ] Baseline benchmarks captured and committed
- [ ] CI/CD pipeline enhanced with sanitizers
- [ ] Team briefed on 3-4 week Phase 1 timeline

### Go/No-Go Decision

- [ ] **GO** - All approvals obtained, proceed to Phase 1
- [ ] **NO-GO** - Missing: _____________

---

## Phase 1, Week 1: Distance Strategy + add() Refactoring

### Task 1.1: Distance Strategy Pattern (Days 1-2)

#### Day 1 Morning: Write Characterization Tests

**Create** `tests/test_distance_strategy.cpp`:

- [ ] Test: L2 strategy matches original implementation
- [ ] Test: Cosine strategy matches original
- [ ] Test: Dot product strategy matches original
- [ ] Test: Strategy works with different dimensions
- [ ] Test: VectorStore uses distance strategy
- [ ] Test: HNSWIndex uses distance strategy
- [ ] Test: All three classes produce consistent distances

**Build and verify tests fail** (red phase):
```bash
cd build
cmake ..
make test_distance_strategy
./tests/test_distance_strategy
# Expected: Tests fail (strategy not implemented yet)
```

- [ ] Tests compile
- [ ] Tests fail with expected errors
- [ ] Errors indicate missing DistanceComputer

#### Day 1 Afternoon: Implement Distance Strategy

**Create** `src/core/distance_strategy.h`:

```cpp
#pragma once
#include "distance.h"

namespace quiverdb {

enum class DistanceMetric : uint32_t {
  L2 = 0,
  COSINE = 1,
  DOT = 2
};

using DistanceFunction = float(*)(const float*, const float*, size_t);

namespace detail {
  inline float dot_wrapper(const float* a, const float* b, size_t n) {
    return -dot_product(a, b, n);  // Negative for max-IP search
  }

  inline DistanceFunction get_distance_function(DistanceMetric metric) {
    switch (metric) {
      case DistanceMetric::L2: return l2_sq;
      case DistanceMetric::COSINE: return cosine_distance;
      case DistanceMetric::DOT: return dot_wrapper;
      default: return nullptr;
    }
  }
}

class DistanceComputer {
  DistanceFunction fn_;
  size_t dim_;

public:
  DistanceComputer(DistanceMetric metric, size_t dim)
    : fn_(detail::get_distance_function(metric)), dim_(dim) {
    if (!fn_) {
      throw std::invalid_argument("Invalid distance metric");
    }
  }

  float operator()(const float* a, const float* b) const {
    return fn_(a, b, dim_);
  }

  DistanceMetric metric() const {
    // Determine metric from function pointer
    if (fn_ == l2_sq) return DistanceMetric::L2;
    if (fn_ == cosine_distance) return DistanceMetric::COSINE;
    if (fn_ == detail::dot_wrapper) return DistanceMetric::DOT;
    return DistanceMetric::L2;  // Default
  }
};

}  // namespace quiverdb
```

- [ ] File created
- [ ] Compiles without errors
- [ ] All functions implemented

**Run tests** (green phase):
```bash
cd build
cmake ..
make test_distance_strategy
./tests/test_distance_strategy
# Expected: All tests pass
```

- [ ] Tests pass
- [ ] No compiler warnings

#### Day 2 Morning: Update VectorStore

**In** `src/core/vector_store.h`:

1. **Add include**:
   ```cpp
   #include "distance_strategy.h"
   ```

2. **Replace** `DistanceMetric metric_` with `DistanceComputer distance_`:
   ```cpp
   // Before:
   DistanceMetric metric_;

   // After:
   DistanceComputer distance_;
   ```

3. **Update constructor**:
   ```cpp
   // Before:
   VectorStore(size_t dim, DistanceMetric metric)
     : dim_(dim), metric_(metric) { }

   // After:
   VectorStore(size_t dim, DistanceMetric metric)
     : dim_(dim), distance_(metric, dim) { }
   ```

4. **Replace compute_distance method**:
   ```cpp
   // Before:
   float compute_distance(const float* a, const float* b) const {
     switch (metric_) {
       case DistanceMetric::L2: return l2_sq(a, b, dim_);
       case DistanceMetric::COSINE: return cosine_distance(a, b, dim_);
       case DistanceMetric::DOT: return -dot_product(a, b, dim_);
       default: return std::numeric_limits<float>::infinity();
     }
   }

   // After:
   float compute_distance(const float* a, const float* b) const {
     return distance_(a, b);
   }
   ```

- [ ] VectorStore updated
- [ ] Compiles without errors
- [ ] Existing VectorStore tests pass

#### Day 2 Afternoon: Update HNSWIndex and MMapVectorStore

**Repeat same process** for:
- [ ] HNSWIndex (src/core/hnsw_index.h)
- [ ] MMapVectorStore (src/core/mmap_vector_store.h)

**Verify**:
```bash
cd build
ctest --output-on-failure
# All tests must pass
```

- [ ] All 131k+ assertions pass
- [ ] Zero compiler warnings

#### Day 2 End: Performance Validation

**Run benchmarks**:
```bash
cd build
./benchmarks/bench_distance --benchmark_min_time=0.2s --benchmark_format=json > current_distance.json

python3 ../tools/compare_benchmarks.py \
  ../benchmarks/baseline_distance.json \
  current_distance.json \
  --threshold=0.10
```

- [ ] Benchmark passes (≤10% regression)
- [ ] If regression >10%, investigate optimization opportunities

**Commit**:
```bash
git add src/core/distance_strategy.h
git add src/core/vector_store.h
git add src/core/hnsw_index.h
git add src/core/mmap_vector_store.h
git add tests/test_distance_strategy.cpp
git commit -m "feat: extract distance calculation strategy pattern

- Eliminates 3 instances of duplicated switch statements
- Enables custom distance metrics
- Maintains performance within 10% of baseline
- All 131k+ assertions pass

Addresses: Issue #3, #4, #17"
git push origin refactor/distance-strategy
```

---

### Task 1.2: Refactor HNSWIndex::add() (Days 3-4)

#### Day 3 Morning: Write Characterization Tests

**Create** `tests/test_hnsw_add_characterization.cpp`:

- [ ] Test: Single vector creates entry point
- [ ] Test: Multiple vectors form connected graph (no isolated nodes)
- [ ] Test: Deterministic construction with fixed seed
- [ ] Test: Graph structure snapshot for 10 vectors (seed=42)

**Run tests** to capture baseline behavior:
```bash
cd build
cmake ..
make test_hnsw_add_characterization
./tests/test_hnsw_add_characterization
```

- [ ] Tests compile
- [ ] Tests pass (capturing v0.1.0 behavior)
- [ ] Graph structure documented

#### Day 3 Afternoon: Write Unit Tests for Extracted Methods

**Add to** `tests/test_hnsw_add_characterization.cpp`:

- [ ] Test: validate_and_allocate - success
- [ ] Test: validate_and_allocate - duplicate ID throws
- [ ] Test: validate_and_allocate - null vector throws
- [ ] Test: validate_and_allocate - capacity exceeded throws
- [ ] Test: initialize_graph_node creates valid structure
- [ ] Test: connect_node_to_graph adds bidirectional links
- [ ] Test: update_entry_point_if_needed updates correctly

**Run tests** (should fail - methods not extracted yet):
```bash
./tests/test_hnsw_add_characterization
# Expected: Compilation errors (methods don't exist)
```

#### Day 4 Morning: Extract Methods

**In** `src/core/hnsw_index.h`, extract methods:

1. **validate_and_allocate**:
   ```cpp
   private:
   size_t validate_and_allocate(uint64_t id, const float* vec) {
     if (!vec) throw std::invalid_argument("Vector must not be null");

     std::unique_lock lk(global_mtx_);

     if (id_map_.count(id)) {
       throw std::invalid_argument("ID " + std::to_string(id) + " already exists");
     }
     if (count_ >= max_elements_) {
       throw std::runtime_error("Max elements reached");
     }

     size_t iid = count_++;
     id_map_[id] = iid;
     std::memcpy(&vectors_[iid * dim_], vec, dim_ * sizeof(float));

     return iid;
   }
   ```

2. **initialize_graph_node**:
   ```cpp
   private:
   int initialize_graph_node(size_t internal_id) {
     int level = get_level();
     levels_[internal_id] = level;
     neighbors_[internal_id].resize(level + 1);
     locks_[internal_id] = std::make_unique<std::shared_mutex>();
     return level;
   }
   ```

3. **connect_node_to_graph** (extract layer connection logic)

4. **update_entry_point_if_needed**:
   ```cpp
   private:
   void update_entry_point_if_needed(size_t internal_id, int level) {
     if (level > entry_level_) {
       entry_point_ = internal_id;
       entry_level_ = level;
     }
   }
   ```

5. **Refactor add() to use extracted methods**:
   ```cpp
   public:
   void add(uint64_t id, const float* vec) {
     size_t internal_id = validate_and_allocate(id, vec);
     int level = initialize_graph_node(internal_id);
     connect_node_to_graph(internal_id, level);
     update_entry_point_if_needed(internal_id, level);
   }
   ```

- [ ] Methods extracted
- [ ] add() reduced to ≤10 lines
- [ ] Compiles without errors

#### Day 4 Afternoon: Verify Tests Pass

**Run all tests**:
```bash
cd build
cmake ..
make
ctest --output-on-failure
```

- [ ] All existing tests pass
- [ ] Characterization tests pass
- [ ] Unit tests for extracted methods pass

**Run concurrency tests with TSan**:
```bash
cmake -B build_tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build_tsan
cd build_tsan
./tests/test_hnsw_index
```

- [ ] TSan reports no data races
- [ ] Concurrent add() operations safe

**Performance validation**:
```bash
cd build
./benchmarks/bench_hnsw_index --benchmark_filter=BM_HNSW_Add --benchmark_format=json > current_hnsw_add.json
```

- [ ] Performance within 10% of baseline

**Commit**:
```bash
git add src/core/hnsw_index.h
git add tests/test_hnsw_add_characterization.cpp
git commit -m "refactor: decompose HNSWIndex::add() method (70→10 lines)

- Extract validate_and_allocate
- Extract initialize_graph_node
- Extract connect_node_to_graph
- Extract update_entry_point_if_needed

Each method now has single responsibility.
All characterization tests pass.
Thread-safety preserved (TSan clean).

Addresses: Issue #1"
git push origin refactor/hnsw-add
```

---

## Phase 1, Week 2: load() + GPU Interface

### Task 1.3: Refactor HNSWIndex::load() (Days 1-3)

#### Day 1: Write Corruption Detection Tests

**Create** `tests/test_hnsw_corruption.cpp`:

- [ ] Test: Invalid magic number throws
- [ ] Test: Unsupported version throws
- [ ] Test: Invalid metric throws
- [ ] Test: Truncated file throws
- [ ] Test: Invalid neighbor count throws
- [ ] Test: Entry point out of range throws
- [ ] Test: Neighbor ID out of range throws
- [ ] Test: Duplicate ID in map throws
- [ ] Test: Dimension overflow throws
- [ ] Test: Vector count overflow throws
- [ ] Test: Invalid level value throws
- [ ] Test: ID map size mismatch throws

**Helper function** to create corrupted files:
```cpp
void corrupt_file(const std::string& path, const std::string& corruption_type);
```

- [ ] 12 corruption tests implemented
- [ ] All tests pass (detect corruption correctly)

#### Day 2: Write Format Compatibility Tests

**Create tests** in `tests/test_hnsw_load.cpp`:

- [ ] Test: V2 format loads correctly
- [ ] Test: V1 format loads with default RNG state
- [ ] Test: Round-trip: save→load→save produces identical file
- [ ] Test: Loaded index produces identical search results

- [ ] Tests implemented
- [ ] Tests pass

#### Day 3: Extract Methods and Refactor

**In** `src/core/hnsw_index.h`, extract:

1. **read_and_validate_header**
2. **deserialize_vectors**
3. **deserialize_graph_structure**
4. **validate_graph_integrity**
5. **restore_rng_state**

**Refactor load()**:
```cpp
static std::unique_ptr<HNSWIndex> load(const std::string& filename) {
  std::ifstream f(filename, std::ios::binary);
  if (!f) throw std::runtime_error("Cannot open: " + filename);

  FileHeader header = read_and_validate_header(f);

  auto idx = std::make_unique<HNSWIndex>(
    header.dim,
    static_cast<DistanceMetric>(header.metric),
    header.max_elements,
    header.M,
    header.ef_construction
  );

  idx->vectors_ = deserialize_vectors(f, header.dim, header.count);
  GraphData graph = deserialize_graph_structure(f, header.count, header.max_elements);

  validate_graph_integrity(graph, header.count, header.entry_point);

  idx->count_ = header.count;
  idx->entry_point_ = header.entry_point;
  idx->entry_level_ = header.entry_level;
  idx->levels_ = std::move(graph.levels);
  idx->neighbors_ = std::move(graph.neighbors);
  idx->id_map_ = std::move(graph.id_map);

  // Initialize mutexes
  idx->locks_.resize(header.max_elements);
  for (size_t i = 0; i < header.count; ++i) {
    idx->locks_[i] = std::make_unique<std::shared_mutex>();
  }

  restore_rng_state(f, header.version, idx->level_gen_);

  return idx;
}
```

**Verify**:
- [ ] load() reduced from 96 to ~30 lines
- [ ] All corruption tests pass
- [ ] All format compatibility tests pass
- [ ] Round-trip test passes
- [ ] Performance within 10%

**Commit**:
```bash
git add src/core/hnsw_index.h
git add tests/test_hnsw_corruption.cpp
git add tests/test_hnsw_load.cpp
git commit -m "refactor: decompose HNSWIndex::load() method (96→30 lines)

- Extract read_and_validate_header
- Extract deserialize_vectors
- Extract deserialize_graph_structure
- Extract validate_graph_integrity
- Extract restore_rng_state

All 12 corruption scenarios detected.
V1/V2 format compatibility maintained.

Addresses: Issue #2"
git push origin refactor/hnsw-load
```

---

### Task 1.4: GPU Interface (Days 4-5)

#### Day 4 Morning: Write GPU Interface Tests

**Create** `tests/test_gpu_interface.cpp`:

- [ ] Test: IGPUCompute interface is complete
- [ ] Test: Factory creates appropriate GPU context
- [ ] Test: Fake GPU returns controlled results
- [ ] Test: Fake GPU can simulate errors
- [ ] Test: Fake GPU can simulate unavailability
- [ ] Test: Integration with GPU (if available)

**Create** `tests/fake_gpu_compute.h`:
```cpp
#ifdef QUIVERDB_ENABLE_TESTING
namespace quiverdb::gpu::testing {

class FakeGPUCompute : public IGPUCompute {
  bool available_ = true;
  bool should_fail_ = false;
  std::vector<float> test_results_;
  size_t call_count_ = 0;

public:
  FakeGPUCompute(bool available = true) : available_(available) {}

  bool available() const override { return available_; }

  std::vector<float> l2(const float* q, const float* v,
                       size_t dim, size_t n) override {
    call_count_++;
    if (should_fail_) throw std::runtime_error("Simulated GPU error");
    return test_results_.empty()
      ? std::vector<float>(n, 1.0f)
      : test_results_;
  }

  // Similar for dot, cosine

  // Test controls
  void setAvailable(bool avail) { available_ = avail; }
  void setTestResults(std::vector<float> results) {
    test_results_ = std::move(results);
  }
  void setShouldFail(bool fail) { should_fail_ = fail; }
  size_t getCallCount() const { return call_count_; }
};

}
#endif
```

- [ ] Tests implemented
- [ ] Fake GPU implemented

#### Day 4 Afternoon: Implement GPU Interface

**Create** `src/core/gpu/gpu_interface.h`:
```cpp
namespace quiverdb::gpu {

class IGPUCompute {
public:
  virtual ~IGPUCompute() = default;

  virtual bool available() const = 0;

  virtual std::vector<float> l2(const float* query, const float* vectors,
                                size_t dim, size_t num_vectors) = 0;

  virtual std::vector<float> dot(const float* query, const float* vectors,
                                 size_t dim, size_t num_vectors) = 0;

  virtual std::vector<float> cosine(const float* query, const float* vectors,
                                    size_t dim, size_t num_vectors) = 0;
};

class NullGPUCompute : public IGPUCompute {
public:
  bool available() const override { return false; }

  std::vector<float> l2(const float*, const float*, size_t, size_t) override {
    throw std::runtime_error("GPU not available");
  }

  std::vector<float> dot(const float*, const float*, size_t, size_t) override {
    throw std::runtime_error("GPU not available");
  }

  std::vector<float> cosine(const float*, const float*, size_t, size_t) override {
    throw std::runtime_error("GPU not available");
  }
};

inline std::unique_ptr<IGPUCompute> createGPUCompute() {
  #ifdef QUIVER_HAS_METAL
    return std::make_unique<MetalCompute>();
  #elif defined(QUIVER_HAS_CUDA)
    return std::make_unique<CudaCompute>();
  #else
    return std::make_unique<NullGPUCompute>();
  #endif
}

}
```

**Update** `src/core/gpu/metal_distance.h`:
```cpp
class MetalCompute : public IGPUCompute {
public:
  MetalCompute();  // Make constructor public
  ~MetalCompute() override = default;

  bool available() const override { return device_ != nullptr; }

  std::vector<float> l2(const float* q, const float* v,
                       size_t d, size_t n) override {
    // Existing implementation unchanged
  }

  // ... other methods
};

// Backward compatibility (deprecated)
[[deprecated("Use createGPUCompute() instead")]]
inline MetalCompute& get() {
  static MetalCompute instance;
  return instance;
}
```

- [ ] Interface created
- [ ] MetalCompute implements interface
- [ ] Backward compatibility maintained

#### Day 5: Test on GPU Hardware

**Run GPU tests**:
```bash
# macOS with Metal
cd build
./tests/test_gpu_interface --success
./tests/test_gpu_metal --success

# Verify backward compatibility
grep -r "MetalCompute::get()" tests/
# Should still compile and work
```

**Performance validation**:
```bash
./benchmarks/bench_gpu --benchmark_format=json > current_gpu.json

python3 ../tools/compare_benchmarks.py \
  ../benchmarks/baseline_gpu.json \
  current_gpu.json \
  --threshold=0.10
```

- [ ] GPU tests pass on hardware
- [ ] Backward compatibility works
- [ ] Performance within 10%

**Commit**:
```bash
git add src/core/gpu/gpu_interface.h
git add src/core/gpu/metal_distance.h
git add tests/test_gpu_interface.cpp
git add tests/fake_gpu_compute.h
git commit -m "refactor: replace GPU singleton with interface pattern

- Create IGPUCompute interface
- MetalCompute implements interface
- Add NullGPUCompute for systems without GPU
- Create FakeGPUCompute for testing
- Maintain backward compatibility (deprecated)

Enables GPU testing without hardware.
Follows Dependency Inversion Principle.

Addresses: Issue #6"
git push origin refactor/gpu-interface
```

---

## Phase 1 Completion Checklist

### End of Week 2 Verification

**Run full test suite**:
```bash
cd build
ctest --output-on-failure --verbose
```

- [ ] All tests pass (100%)
- [ ] Test count increased (was 38, now ≥50)
- [ ] Assertion count increased (was 131k+, now ≥150k)

**Run sanitizers**:
```bash
# Address Sanitizer
cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build_asan
cd build_asan && ctest

# Thread Sanitizer
cmake -B build_tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build_tsan
cd build_tsan && ctest

# Undefined Behavior Sanitizer
cmake -B build_ubsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON
cmake --build build_ubsan
cd build_ubsan && ctest
```

- [ ] ASan: No leaks, no overruns
- [ ] TSan: No data races
- [ ] UBSan: No undefined behavior

**Check code coverage**:
```bash
cmake -B build_coverage -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build_coverage
cd build_coverage
ctest
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --list coverage.info | grep "lines......:"
```

- [ ] Line coverage ≥95%
- [ ] Branch coverage ≥90%

**Performance validation**:
```bash
cd build
./tools/benchmark_all.sh
```

- [ ] All benchmarks within 10% (median of 10 runs) of baseline
- [ ] No regressions detected

**Code quality metrics**:
```bash
# Check longest method
find src/core -name "*.h" -exec grep -A 100 "void\|float\|bool" {} \; | \
  awk '/^{/,/^}/' | wc -l | sort -n | tail -1
```

- [ ] Longest method ≤40 lines (was 96)

```bash
# Check for duplicated distance switches
grep -r "switch.*metric" src/core/ | wc -l
```

- [ ] Zero duplicated distance switch statements (was 3)

**Phase 1 Deliverables**:
- [ ] distance_strategy.h created
- [ ] gpu_interface.h created
- [ ] hnsw_index.h refactored (add and load methods decomposed)
- [ ] 20+ new test cases added
- [ ] All documentation updated

---

## Phase 2 Checklist (Weeks 3-4)

### Task 2.1: Unify Enums (Day 1)

- [ ] Create src/core/distance_metric.h with single DistanceMetric enum
- [ ] Remove HNSWDistanceMetric
- [ ] Remove MetalMetric
- [ ] Remove CudaMetric
- [ ] Update all usages
- [ ] All tests pass
- [ ] Python bindings work

### Task 2.2: Platform Abstraction (Days 2-4)

- [ ] Create src/core/platform/file_utils.h
- [ ] Implement sync_file_to_disk()
- [ ] Test on Windows/Linux/macOS
- [ ] Create FileMapper interface (optional)
- [ ] Update MMapVectorStore
- [ ] Update HNSWIndex
- [ ] All tests pass

### Task 2.3: Named Constants (Day 5)

- [ ] Create src/core/constants.h
- [ ] Replace all magic numbers
- [ ] Document rationale for each constant
- [ ] All tests pass

### Task 2.4: Refactor MMap Constructor (Week 4)

- [ ] Write corruption detection tests
- [ ] Extract validation methods
- [ ] Constructor reduced to ≤30 lines
- [ ] All tests pass

---

## Phase 3 Checklist (Weeks 5-6)

### Quick Wins

- [ ] Variable naming improvements
- [ ] Standardized error messages
- [ ] Extracted validation functions
- [ ] Refactor HNSWIndex::save()

### Documentation

- [ ] Update README.md
- [ ] Update API documentation
- [ ] Create migration guide
- [ ] Update examples

### Final Verification

- [ ] All tests pass
- [ ] All benchmarks within 10% (median of 10 runs)
- [ ] Code coverage ≥95%
- [ ] Zero sanitizer errors
- [ ] Zero duplicated code
- [ ] Zero magic numbers
- [ ] All methods ≤40 lines

---

## Release Preparation

### v0.2.0 Checklist

**Code Quality**:
- [ ] All refactoring complete
- [ ] All tests passing
- [ ] Performance validated
- [ ] Memory safety verified

**Documentation**:
- [ ] CHANGELOG.md updated
- [ ] Migration guide complete
- [ ] API docs updated
- [ ] Examples tested

**Testing**:
- [ ] CI/CD passing on all platforms
- [ ] Python bindings tested
- [ ] Example applications work

**Release**:
- [ ] Version bumped to 0.2.0
- [ ] Git tag created
- [ ] Release notes published
- [ ] PyPI package published (if ready)

---

## Troubleshooting

### If Tests Fail

1. **Check which tests failed**:
   ```bash
   cd build
   ctest --output-on-failure --verbose
   ```

2. **Run specific test**:
   ```bash
   ./tests/test_hnsw_index --success
   ```

3. **Check for compilation errors**:
   ```bash
   cmake --build build 2>&1 | grep error
   ```

4. **Rollback if necessary** (safe method - preserves history):
   ```bash
   # Create revert commit (PREFERRED - preserves history)
   git revert HEAD --no-edit
   git push origin refactor/<branch-name>

   # For multiple commits:
   git revert HEAD~3..HEAD --no-edit
   git push origin refactor/<branch-name>
   ```

   **WARNING**: Never use `git reset --hard` or `git push --force` without team lead approval.

### If Performance Regresses

1. **Identify slow benchmark**:
   ```bash
   python3 tools/compare_benchmarks.py \
     benchmarks/baseline_distance.json \
     build/current_distance.json
   ```

2. **Profile with perf** (Linux):
   ```bash
   perf record ./benchmarks/bench_distance
   perf report
   ```

3. **Check assembly** (verify inlining):
   ```bash
   objdump -d build/benchmarks/bench_distance | grep -A 20 "DistanceComputer"
   ```

4. **Optimize or rollback**

### If Sanitizers Report Issues

1. **ASan leak**:
   ```bash
   ASAN_OPTIONS=detect_leaks=1 ./tests/test_hnsw_index
   ```

2. **TSan race**:
   ```bash
   TSAN_OPTIONS=second_deadlock_stack=1 ./tests/test_hnsw_index
   ```

3. **Fix and re-test**

---

## Daily Progress Tracking

### Week 1

| Day | Task | Status | Tests Pass | Benchmarks OK | Notes |
|-----|------|--------|------------|---------------|-------|
| Mon | Distance strategy tests | ⬜ | ⬜ | ⬜ | |
| Mon | Distance strategy impl | ⬜ | ⬜ | ⬜ | |
| Tue | Update VectorStore | ⬜ | ⬜ | ⬜ | |
| Tue | Update HNSW/MMap | ⬜ | ⬜ | ⬜ | |
| Wed | HNSW add tests | ⬜ | ⬜ | ⬜ | |
| Wed | Extract methods | ⬜ | ⬜ | ⬜ | |
| Thu | Verify + commit | ⬜ | ⬜ | ⬜ | |
| Thu | Documentation | ⬜ | ⬜ | ⬜ | |
| Fri | Code review | ⬜ | ⬜ | ⬜ | |

### Week 2

| Day | Task | Status | Tests Pass | Benchmarks OK | Notes |
|-----|------|--------|------------|---------------|-------|
| Mon | Corruption tests | ⬜ | ⬜ | ⬜ | |
| Tue | Format compat tests | ⬜ | ⬜ | ⬜ | |
| Wed | Extract load methods | ⬜ | ⬜ | ⬜ | |
| Thu | GPU interface tests | ⬜ | ⬜ | ⬜ | |
| Thu | GPU refactoring | ⬜ | ⬜ | ⬜ | |
| Fri | Phase 1 validation | ⬜ | ⬜ | ⬜ | |

---

## Success Criteria Summary

### Must Have (Mandatory)

- [ ] All 131k+ assertions pass
- [ ] Performance within 10% of baseline
- [ ] Zero sanitizer errors
- [ ] Code coverage ≥95%
- [ ] All methods ≤40 lines
- [ ] Zero code duplication

### Should Have (Important)

- [ ] Test count ≥50 (was 38)
- [ ] Assertions ≥150k (was 131k)
- [ ] Branch coverage ≥90%
- [ ] Build time ≤110% of baseline
- [ ] Python bindings work

### Nice to Have (Optional)

- [ ] GPU tests on all platforms
- [ ] iOS/Android tested
- [ ] Documentation examples updated
- [ ] Migration guide reviewed

---

## Contact and Support

**Questions?** Refer to:
- `final-implementation-plan.md` - Detailed ATDD strategy
- `refactoring-plan.md` - Code examples and implementation
- `code-smell-detector-report.md` - Problem analysis

**Issues?** Document in rollback_log.txt and discuss with team.

---

**Checklist Version**: 1.0
**Last Updated**: 2026-01-26
