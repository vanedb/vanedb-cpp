# QuiverDB ATDD Implementation Plan
**Version**: 0.1.0 → 0.2.0
**Date**: 2026-01-26
**Approach**: Acceptance Test Driven Development (ATDD)

---

## Executive Summary

This plan integrates the refactoring roadmap with a comprehensive test-driven development strategy. Each refactoring task is paired with acceptance criteria, test specifications, and verification checkpoints to ensure behavioral contracts are preserved while improving code quality.

**Timeline**: 6-8 weeks across 3 phases
**Testing Investment**: 40% of effort (test-first approach)
**Critical Success Factor**: All 131k+ existing assertions pass after each refactoring step

### Test-Driven Approach

```
For each refactoring:
1. Write characterization tests (capture current behavior)
2. Define acceptance criteria (what must not break)
3. Write new tests for extracted abstractions
4. Perform refactoring (red → green → refactor)
5. Verify performance benchmarks (no regression)
6. Run full CI/CD pipeline
7. Rollback if acceptance criteria not met
```

---

## Behavioral Contracts (Immutable Requirements)

These contracts MUST be preserved throughout all refactoring. Any violation triggers immediate rollback.

### Contract 1: Distance Calculation Correctness
**Scope**: All distance functions (L2, cosine, dot product)
**Validation**: Existing 131k+ assertions in test_distance.cpp

**Acceptance Criteria**:
- [ ] L2 distance: `||a-b||² = Σ(aᵢ-bᵢ)²` within 1e-6 tolerance
- [ ] Cosine distance: `1 - (a·b)/(||a||·||b||)` within 1e-6 tolerance
- [ ] Dot product: `a·b = Σ(aᵢ·bᵢ)` within 1e-6 tolerance
- [ ] SIMD and scalar implementations produce identical results
- [ ] Works for dimensions: 0, 1, 2, 3, 4, 8, 64, 128, 768, 1536, 773 (non-aligned)
- [ ] Handles edge cases: zero vectors, identical vectors, orthogonal vectors

**Test Suite**: `tests/test_distance.cpp` (10 test cases)

---

### Contract 2: HNSW Index Correctness
**Scope**: HNSWIndex add(), search(), save(), load()

**Acceptance Criteria**:
- [ ] Search recall@10 ≥ 90% on 10k random vectors (dim=128)
- [ ] Exact match returns distance ≈ 0.0 (within 1e-5)
- [ ] Search returns exactly k results (when available)
- [ ] Add() preserves graph connectivity (no isolated nodes)
- [ ] Save/load round-trip produces identical index behavior
- [ ] Concurrent add() operations are thread-safe
- [ ] Duplicate ID insertion throws std::invalid_argument
- [ ] Max capacity enforcement (no buffer overruns)

**Test Suite**: `tests/test_hnsw_index.cpp` (15 test cases)

---

### Contract 3: Memory Safety
**Scope**: All classes with dynamic allocation

**Acceptance Criteria**:
- [ ] No memory leaks (valgrind/ASan clean)
- [ ] No use-after-free (ASan clean)
- [ ] No buffer overruns (ASan clean)
- [ ] No data races (TSan clean)
- [ ] No undefined behavior (UBSan clean)
- [ ] RAII: All resources cleaned up on exception
- [ ] Move semantics preserve object validity

**Validation Tools**:
- AddressSanitizer (ASan)
- ThreadSanitizer (TSan)
- UndefinedBehaviorSanitizer (UBSan)
- Valgrind (on Linux)

---

### Contract 4: Performance Bounds
**Scope**: All core operations (must not regress by >10%)

**Baseline Benchmarks** (v0.1.0):
| Operation | Baseline | Acceptable Range | Hardware |
|-----------|----------|------------------|----------|
| L2 distance (768d) | 100ns | 90-110ns (10%) | Apple M1/M2 |
| Cosine (768d) | 120ns | 108-132ns (10%) | Apple M1/M2 |
| HNSW add() (10k vectors) | 50μs | 45-55μs (10%) | Apple M1/M2 |
| HNSW search k=10 (10k vectors) | 200μs | 180-220μs (10%) | Apple M1/M2 |
| GPU search (500k vectors) | 2.5ms | 2.25-2.75ms (10%) | Apple Metal |
| VectorStore search k=10 (1k vectors) | 15μs | 13.5-16.5μs (10%) | Apple M1/M2 |

**Measurement Protocol** (to reduce flakiness):
- Run 10 iterations, report median
- Warm up cache with 3 discarded runs
- Document CPU frequency and thermal state
- Isolate benchmark process (nice -20 on Linux/macOS)

**Tail Latency Requirements**:
| Operation | P99 Max | P99.9 Max |
|-----------|---------|-----------|
| L2 (768d) | 150ns | 300ns |
| HNSW search | 500μs | 1ms |

**Measurement**: Google Benchmark with 10 repetitions, report median

**Rollback Trigger**: Any operation exceeds acceptable range after refactoring

---

### Contract 5: API Stability
**Scope**: Public interfaces (breaking changes require major version bump)

**Preserved APIs**:
```cpp
// VectorStore
VectorStore(size_t dim, DistanceMetric metric);
void add(uint64_t id, const float* vector);
std::vector<SearchResult> search(const float* query, size_t k);
const float* get(uint64_t id);

// HNSWIndex
HNSWIndex(size_t dim, HNSWDistanceMetric metric, size_t max_elements,
          int M, int ef_construction, uint64_t random_seed);
void add(uint64_t id, const float* vector);
std::vector<HNSWSearchResult> search(const float* query, size_t k);
void save(const std::string& filename);
static std::unique_ptr<HNSWIndex> load(const std::string& filename);

// MMapVectorStore
MMapVectorStore(const std::string& filename);
std::vector<SearchResult> search(const float* query, size_t k);
```

**Validation**: Existing Python bindings still work without modification

---

## Phase 1: Critical Architectural Improvements (Weeks 1-4)

> **Note**: Original estimate was 2 weeks, revised to 3-4 weeks to include
> characterization test writing, code review cycles, and performance validation.

### Task 1.1: Extract Distance Calculation Strategy

**Addresses**: Issues #3, #4, #17 (Duplicate switch, OCP violation)
**Effort**: 8-12 hours
**Test Effort**: 4-6 hours (50% of development)

#### Acceptance Criteria

**Functional**:
- [ ] All distance metrics produce identical results to v0.1.0 (bit-exact)
- [ ] VectorStore, HNSWIndex, MMapVectorStore use single distance implementation
- [ ] Adding new distance metric requires changes in only 1 location
- [ ] Can register custom distance function via public API
- [ ] All 131k+ existing assertions pass

**Performance**:
- [ ] Distance calculation performance ≤ 110% of baseline (function pointer overhead)
- [ ] Compiler inlines strategy for hot paths (verify with assembly inspection)
- [ ] No virtual dispatch in inner loops

**Code Quality**:
- [ ] Zero duplicated distance calculation code
- [ ] Open/Closed Principle satisfied (extensible without modification)
- [ ] Strategy pattern properly implemented

#### Test-First Sequence

**Step 1: Write Characterization Tests** (1 hour)

```cpp
// tests/test_distance_strategy.cpp (NEW FILE)

TEST_CASE("Distance Strategy - Characterization", "[distance][strategy]") {
  SECTION("L2 strategy matches original implementation") {
    float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[] = {5.0f, 6.0f, 7.0f, 8.0f};

    // Original implementation result (from v0.1.0)
    float expected = 64.0f;  // (4-0)^2 + (4-0)^2 + (4-0)^2 + (4-0)^2

    // New strategy implementation
    auto strategy = quiverdb::DistanceComputer(
      quiverdb::DistanceMetric::L2, 4);
    float result = strategy(a, b);

    REQUIRE(result == Approx(expected).margin(1e-6f));
  }

  SECTION("Cosine strategy matches original") {
    float a[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float b[] = {0.0f, 1.0f, 0.0f, 0.0f};

    float expected = 1.0f;  // Orthogonal vectors

    auto strategy = quiverdb::DistanceComputer(
      quiverdb::DistanceMetric::COSINE, 4);
    float result = strategy(a, b);

    REQUIRE(result == Approx(expected).margin(1e-6f));
  }

  SECTION("Dot product strategy matches original") {
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    float expected = -32.0f;  // Negative for max-IP search

    auto strategy = quiverdb::DistanceComputer(
      quiverdb::DistanceMetric::DOT, 3);
    float result = strategy(a, b);

    REQUIRE(result == Approx(expected).margin(1e-6f));
  }
}
```

**Step 2: Write Integration Tests** (1 hour)

```cpp
TEST_CASE("Distance Strategy - Integration", "[distance][strategy]") {
  SECTION("VectorStore uses distance strategy") {
    quiverdb::VectorStore store(4, quiverdb::DistanceMetric::L2);

    float v1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float v2[] = {0.0f, 1.0f, 0.0f, 0.0f};

    store.add(1, v1);
    store.add(2, v2);

    auto results = store.search(v1, 1);
    REQUIRE(results[0].id == 1);
    REQUIRE(results[0].distance == Approx(0.0f).margin(1e-6f));
  }

  SECTION("HNSWIndex uses distance strategy") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100);

    float v1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    index.add(1, v1);

    auto results = index.search(v1, 1);
    REQUIRE(results[0].id == 1);
    REQUIRE(results[0].distance == Approx(0.0f).margin(1e-5f));
  }

  SECTION("All three classes produce consistent distances") {
    const size_t dim = 128;
    std::vector<float> a(dim), b(dim);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (size_t i = 0; i < dim; ++i) {
      a[i] = dis(gen);
      b[i] = dis(gen);
    }

    // Compute distance using all three classes
    quiverdb::VectorStore store(dim, quiverdb::DistanceMetric::L2);
    store.add(1, a.data());
    store.add(2, b.data());
    auto store_result = store.search(a.data(), 2);

    quiverdb::HNSWIndex index(dim, quiverdb::DistanceMetric::L2, 10);
    index.add(1, a.data());
    index.add(2, b.data());
    auto hnsw_result = index.search(a.data(), 2);

    // Note: Exact values may differ due to HNSW approximation,
    // but ordering should be consistent
    REQUIRE(store_result[1].id == hnsw_result[1].id);
  }
}
```

**Step 3: Write Performance Tests** (1 hour)

```cpp
// benchmarks/bench_distance_strategy.cpp (NEW FILE)

static void BM_DistanceStrategy_L2_Original(benchmark::State& state) {
  size_t dim = state.range(0);
  std::vector<float> a(dim, 1.0f);
  std::vector<float> b(dim, 2.0f);

  for (auto _ : state) {
    // Original switch-based implementation
    float result = quiverdb::l2_sq(a.data(), b.data(), dim);
    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(int64_t(state.iterations()) *
                         int64_t(dim) * int64_t(sizeof(float) * 2));
}

static void BM_DistanceStrategy_L2_New(benchmark::State& state) {
  size_t dim = state.range(0);
  std::vector<float> a(dim, 1.0f);
  std::vector<float> b(dim, 2.0f);

  auto strategy = quiverdb::DistanceComputer(
    quiverdb::DistanceMetric::L2, dim);

  for (auto _ : state) {
    float result = strategy(a.data(), b.data());
    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(int64_t(state.iterations()) *
                         int64_t(dim) * int64_t(sizeof(float) * 2));
}

BENCHMARK(BM_DistanceStrategy_L2_Original)->Arg(768)->Repetitions(10);
BENCHMARK(BM_DistanceStrategy_L2_New)->Arg(768)->Repetitions(10);
```

**Step 4: Implement Distance Strategy** (4-6 hours)

Follow refactoring plan Section 1.1 implementation steps.

**Step 5: Run Verification** (1-2 hours)

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run performance benchmarks
./benchmarks/bench_distance_strategy --benchmark_repetitions=10

# Check performance regression
# Acceptable if new implementation is within 110% of original
```

**Step 6: CI/CD Integration**

Update `.github/workflows/build-and-test.yml`:
```yaml
- name: Run distance strategy tests
  run: |
    cd build
    ./tests/test_distance_strategy --success

- name: Benchmark distance strategy
  run: |
    cd build
    ./benchmarks/bench_distance_strategy \
      --benchmark_min_time=0.1s \
      --benchmark_format=json > distance_strategy_bench.json
```

#### Rollback Criteria

**Automatic Rollback** if:
- [ ] Any existing test fails
- [ ] Performance degrades >10%
- [ ] Memory sanitizers detect issues
- [ ] Code coverage drops below 95%

**Manual Review Required** if:
- [ ] New abstractions are unclear to team
- [ ] Integration complexity increases
- [ ] Build time increases >10%

---

### Task 1.2: Refactor HNSWIndex::add() Method

**Addresses**: Issue #1 (70-line method, SRP violation)
**Effort**: 8-12 hours
**Test Effort**: 6-8 hours (60% of development)

#### Acceptance Criteria

**Functional**:
- [ ] Graph structure identical to v0.1.0 (same neighbor connections)
- [ ] Level generation produces same distribution
- [ ] Entry point selection matches original algorithm
- [ ] Thread-safety preserved (no deadlocks, race conditions)
- [ ] All existing HNSW tests pass

**Performance**:
- [ ] add() latency ≤ 110% of baseline (52.5μs at 10k vectors)
- [ ] Memory usage unchanged
- [ ] Lock contention unchanged

**Code Quality**:
- [ ] Main add() method ≤ 10 lines (Compose Method pattern)
- [ ] Each extracted method has single responsibility
- [ ] Each method independently testable

#### Test-First Sequence

**Step 1: Characterization Tests for Graph Structure** (2 hours)

```cpp
// tests/test_hnsw_add_characterization.cpp (NEW FILE)

// Helper to extract graph structure (requires friendship or reflection)
#ifdef QUIVERDB_ENABLE_TESTING
namespace quiverdb {
  struct HNSWInspector {
    static int get_level(const HNSWIndex& idx, size_t internal_id);
    static const std::vector<size_t>& get_neighbors(
      const HNSWIndex& idx, size_t internal_id, int layer);
    static size_t get_entry_point(const HNSWIndex& idx);
  };
}
#endif

TEST_CASE("HNSW add() - Graph Structure Characterization", "[hnsw][add]") {
  SECTION("Single vector creates entry point") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100, 8, 50, 42);

    float vec[] = {1.0f, 0.0f, 0.0f, 0.0f};
    index.add(1, vec);

    #ifdef QUIVERDB_ENABLE_TESTING
    // Verify graph structure
    REQUIRE(index.size() == 1);
    auto entry = quiverdb::HNSWInspector::get_entry_point(index);
    REQUIRE(entry == 0);  // First internal ID

    int level = quiverdb::HNSWInspector::get_level(index, 0);
    REQUIRE(level >= 0);
    #endif
  }

  SECTION("Multiple vectors form connected graph") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100, 8, 50, 42);

    // Add 10 vectors with fixed seed for reproducibility
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (uint64_t i = 0; i < 10; ++i) {
      std::vector<float> vec(4);
      for (size_t j = 0; j < 4; ++j) {
        vec[j] = dis(gen);
      }
      index.add(i, vec.data());
    }

    #ifdef QUIVERDB_ENABLE_TESTING
    // Verify no isolated nodes (all nodes have neighbors at layer 0)
    for (size_t i = 0; i < 10; ++i) {
      auto& neighbors = quiverdb::HNSWInspector::get_neighbors(index, i, 0);
      REQUIRE(neighbors.size() > 0);
    }
    #endif
  }

  SECTION("Deterministic graph construction with fixed seed") {
    // Build index twice with same seed
    auto build_index = [](uint64_t seed) {
      quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2,
                               100, 8, 50, seed);

      std::mt19937 gen(seed);
      std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

      for (uint64_t i = 0; i < 10; ++i) {
        std::vector<float> vec(4);
        for (size_t j = 0; j < 4; ++j) {
          vec[j] = dis(gen);
        }
        index.add(i, vec.data());
      }

      return index;
    };

    auto idx1 = build_index(42);
    auto idx2 = build_index(42);

    // Verify identical structure
    #ifdef QUIVERDB_ENABLE_TESTING
    for (size_t i = 0; i < 10; ++i) {
      REQUIRE(quiverdb::HNSWInspector::get_level(idx1, i) ==
              quiverdb::HNSWInspector::get_level(idx2, i));

      int level = quiverdb::HNSWInspector::get_level(idx1, i);
      for (int L = 0; L <= level; ++L) {
        auto& n1 = quiverdb::HNSWInspector::get_neighbors(idx1, i, L);
        auto& n2 = quiverdb::HNSWInspector::get_neighbors(idx2, i, L);
        REQUIRE(n1 == n2);
      }
    }
    #endif
  }
}
```

**Step 2: Unit Tests for Extracted Methods** (2 hours)

```cpp
TEST_CASE("HNSW add() - Extracted Methods", "[hnsw][add]") {
  SECTION("validate_and_allocate - success") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100);
    float vec[] = {1.0f, 2.0f, 3.0f, 4.0f};

    #ifdef QUIVERDB_ENABLE_TESTING
    size_t internal_id = index.validate_and_allocate(1, vec);
    REQUIRE(internal_id == 0);
    REQUIRE(index.size() == 1);
    #endif
  }

  SECTION("validate_and_allocate - duplicate ID throws") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100);
    float vec[] = {1.0f, 2.0f, 3.0f, 4.0f};

    index.add(1, vec);

    #ifdef QUIVERDB_ENABLE_TESTING
    REQUIRE_THROWS_AS(index.validate_and_allocate(1, vec),
                     std::invalid_argument);
    #endif
  }

  SECTION("validate_and_allocate - null vector throws") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100);

    #ifdef QUIVERDB_ENABLE_TESTING
    REQUIRE_THROWS_AS(index.validate_and_allocate(1, nullptr),
                     std::invalid_argument);
    #endif
  }

  SECTION("validate_and_allocate - capacity exceeded throws") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 2);
    float vec[] = {1.0f, 2.0f, 3.0f, 4.0f};

    index.add(1, vec);
    index.add(2, vec);

    #ifdef QUIVERDB_ENABLE_TESTING
    REQUIRE_THROWS_AS(index.validate_and_allocate(3, vec),
                     std::runtime_error);
    #endif
  }

  SECTION("initialize_graph_node creates valid structure") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100, 8, 50, 42);
    float vec[] = {1.0f, 0.0f, 0.0f, 0.0f};

    #ifdef QUIVERDB_ENABLE_TESTING
    size_t iid = index.validate_and_allocate(1, vec);
    int level = index.initialize_graph_node(iid);

    REQUIRE(level >= 0);
    REQUIRE(level <= quiverdb::HNSWIndex::MAX_LEVEL);
    REQUIRE(quiverdb::HNSWInspector::get_level(index, iid) == level);
    #endif
  }
}
```

**Step 3: Concurrency Tests** (2 hours)

```cpp
TEST_CASE("HNSW add() - Thread Safety", "[hnsw][add][concurrency]") {
  SECTION("Concurrent add operations are safe") {
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 1000);

    std::atomic<int> error_count{0};

    auto add_worker = [&](int start_id, int count) {
      try {
        std::mt19937 gen(start_id);
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        for (int i = 0; i < count; ++i) {
          std::vector<float> vec(4);
          for (size_t j = 0; j < 4; ++j) {
            vec[j] = dis(gen);
          }
          index.add(start_id + i, vec.data());
        }
      } catch (...) {
        error_count++;
      }
    };

    // Launch 10 threads, each adding 10 vectors
    std::vector<std::thread> threads;
    for (int t = 0; t < 10; ++t) {
      threads.emplace_back(add_worker, t * 10, 10);
    }

    for (auto& th : threads) {
      th.join();
    }

    REQUIRE(error_count == 0);
    REQUIRE(index.size() == 100);
  }
}
```

**Step 4: Implement Refactoring** (6-8 hours)

Follow refactoring plan Section 1.2 implementation steps.

**Step 5: Regression Testing** (2 hours)

```bash
# Run all HNSW tests
cd build
./tests/test_hnsw_index --success

# Run characterization tests
./tests/test_hnsw_add_characterization --success

# Run concurrency tests with TSan
cmake -B build_tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build_tsan
cd build_tsan
./tests/test_hnsw_add_characterization --success

# Performance benchmarks
cd build
./benchmarks/bench_hnsw_index \
  --benchmark_filter=BM_HNSW_Add \
  --benchmark_repetitions=10

# Check search recall is unchanged
./tests/test_hnsw_search_quality --success
```

#### Rollback Criteria

**Automatic Rollback** if:
- [ ] Any HNSW test fails
- [ ] add() latency increases >10%
- [ ] TSan detects race conditions
- [ ] Search recall decreases >1%
- [ ] Graph structure diverges from v0.1.0

---

### Task 1.3: Refactor HNSWIndex::load() Method

**Addresses**: Issue #2 (96-line method)
**Effort**: 10-14 hours
**Test Effort**: 6-8 hours (50%)

#### Acceptance Criteria

**Functional**:
- [ ] Loads v1 and v2 file formats correctly
- [ ] Detects all 12 corruption scenarios
- [ ] Round-trip: save() → load() → save() produces identical file
- [ ] RNG state correctly restored (deterministic behavior after load)
- [ ] All validation checks from original code preserved

**Performance**:
- [ ] Load time ≤ 110% of baseline
- [ ] Memory usage unchanged

**Code Quality**:
- [ ] Main load() method ≤ 30 lines
- [ ] Each validation step testable in isolation

#### Test-First Sequence

**Step 1: Corruption Detection Tests** (3 hours)

```cpp
// tests/test_hnsw_corruption.cpp (NEW FILE)

TEST_CASE("HNSW load() - Corruption Detection", "[hnsw][load][corruption]") {
  // Helper to create corrupted file
  auto create_corrupted_file = [](const std::string& path,
                                  const std::string& corruption_type) {
    // Create a valid index first
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 10);
    std::vector<float> vec(4, 1.0f);
    for (uint64_t i = 0; i < 5; ++i) {
      index.add(i, vec.data());
    }
    index.save(path);

    // Corrupt the file
    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);

    if (corruption_type == "bad_magic") {
      uint32_t bad_magic = 0xDEADBEEF;
      file.seekp(0);
      file.write(reinterpret_cast<const char*>(&bad_magic), 4);
    } else if (corruption_type == "bad_version") {
      uint32_t bad_version = 999;
      file.seekp(4);
      file.write(reinterpret_cast<const char*>(&bad_version), 4);
    } else if (corruption_type == "invalid_metric") {
      uint32_t bad_metric = 99;
      file.seekp(8);
      file.write(reinterpret_cast<const char*>(&bad_metric), 4);
    } else if (corruption_type == "truncated") {
      // Truncate file
      file.close();
      std::filesystem::resize_file(path, 100);  // Too small
    } else if (corruption_type == "bad_neighbor_count") {
      // Set impossibly high neighbor count
      file.seekp(512);  // Approximate position
      uint32_t bad_count = 999999;
      file.write(reinterpret_cast<const char*>(&bad_count), 4);
    }

    file.close();
  };

  SECTION("Invalid magic number throws") {
    std::string path = "test_bad_magic.hnsw";
    create_corrupted_file(path, "bad_magic");

    REQUIRE_THROWS_WITH(
      quiverdb::HNSWIndex::load(path),
      Catch::Matchers::ContainsSubstring("Invalid file format")
    );

    std::filesystem::remove(path);
  }

  SECTION("Unsupported version throws") {
    std::string path = "test_bad_version.hnsw";
    create_corrupted_file(path, "bad_version");

    REQUIRE_THROWS_WITH(
      quiverdb::HNSWIndex::load(path),
      Catch::Matchers::ContainsSubstring("Unsupported version")
    );

    std::filesystem::remove(path);
  }

  SECTION("Invalid metric throws") {
    std::string path = "test_bad_metric.hnsw";
    create_corrupted_file(path, "invalid_metric");

    REQUIRE_THROWS_WITH(
      quiverdb::HNSWIndex::load(path),
      Catch::Matchers::ContainsSubstring("invalid metric")
    );

    std::filesystem::remove(path);
  }

  SECTION("Truncated file throws") {
    std::string path = "test_truncated.hnsw";
    create_corrupted_file(path, "truncated");

    REQUIRE_THROWS_WITH(
      quiverdb::HNSWIndex::load(path),
      Catch::Matchers::ContainsSubstring("Failed to read")
    );

    std::filesystem::remove(path);
  }

  SECTION("Invalid neighbor count throws") {
    std::string path = "test_bad_neighbors.hnsw";
    create_corrupted_file(path, "bad_neighbor_count");

    REQUIRE_THROWS_WITH(
      quiverdb::HNSWIndex::load(path),
      Catch::Matchers::ContainsSubstring("too many neighbors")
    );

    std::filesystem::remove(path);
  }
}
```

**Step 2: Format Compatibility Tests** (2 hours)

```cpp
TEST_CASE("HNSW load() - Format Compatibility", "[hnsw][load][compat]") {
  SECTION("V2 format loads correctly") {
    // Create v2 index
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100, 8, 50, 42);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (uint64_t i = 0; i < 10; ++i) {
      std::vector<float> vec(4);
      for (size_t j = 0; j < 4; ++j) {
        vec[j] = dis(gen);
      }
      index.add(i, vec.data());
    }

    std::string path = "test_v2_format.hnsw";
    index.save(path);

    // Load and verify
    auto loaded = quiverdb::HNSWIndex::load(path);

    REQUIRE(loaded->size() == 10);
    REQUIRE(loaded->dimension() == 4);

    // Verify search works
    std::vector<float> query(4, 1.0f);
    auto results = loaded->search(query.data(), 5);
    REQUIRE(results.size() == 5);

    std::filesystem::remove(path);
  }

  SECTION("V1 format loads with default RNG state") {
    // Note: If v1 test files exist, load them
    // Otherwise, skip this test

    if (std::filesystem::exists("testdata/v1_index.hnsw")) {
      auto loaded = quiverdb::HNSWIndex::load("testdata/v1_index.hnsw");

      REQUIRE(loaded->size() > 0);

      // Verify basic search works
      std::vector<float> query(loaded->dimension(), 1.0f);
      auto results = loaded->search(query.data(), 1);
      REQUIRE(results.size() == 1);
    }
  }
}
```

**Step 3: Round-Trip Tests** (1 hour)

```cpp
TEST_CASE("HNSW load() - Round-Trip Integrity", "[hnsw][load][roundtrip]") {
  SECTION("Save/Load/Save produces identical files") {
    // Create index
    quiverdb::HNSWIndex index(8, quiverdb::DistanceMetric::COSINE,
                             100, 16, 100, 42);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (uint64_t i = 0; i < 50; ++i) {
      std::vector<float> vec(8);
      for (size_t j = 0; j < 8; ++j) {
        vec[j] = dis(gen);
      }
      index.add(i, vec.data());
    }

    // Save
    std::string path1 = "test_roundtrip1.hnsw";
    index.save(path1);

    // Load
    auto loaded = quiverdb::HNSWIndex::load(path1);

    // Save again
    std::string path2 = "test_roundtrip2.hnsw";
    loaded->save(path2);

    // Compare file contents
    std::ifstream f1(path1, std::ios::binary);
    std::ifstream f2(path2, std::ios::binary);

    std::vector<char> data1((std::istreambuf_iterator<char>(f1)),
                            std::istreambuf_iterator<char>());
    std::vector<char> data2((std::istreambuf_iterator<char>(f2)),
                            std::istreambuf_iterator<char>());

    REQUIRE(data1 == data2);

    std::filesystem::remove(path1);
    std::filesystem::remove(path2);
  }

  SECTION("Loaded index produces identical search results") {
    quiverdb::HNSWIndex index(32, quiverdb::DistanceMetric::L2,
                             100, 8, 50, 123);

    std::mt19937 gen(123);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    std::vector<std::vector<float>> vectors(20);
    for (size_t i = 0; i < 20; ++i) {
      vectors[i].resize(32);
      for (size_t j = 0; j < 32; ++j) {
        vectors[i][j] = dis(gen);
      }
      index.add(i, vectors[i].data());
    }

    // Save
    std::string path = "test_search_consistency.hnsw";
    index.save(path);

    // Search before load
    std::vector<float> query(32);
    for (size_t i = 0; i < 32; ++i) {
      query[i] = dis(gen);
    }
    auto results_before = index.search(query.data(), 10);

    // Load
    auto loaded = quiverdb::HNSWIndex::load(path);

    // Search after load
    auto results_after = loaded->search(query.data(), 10);

    // Verify results are identical
    REQUIRE(results_before.size() == results_after.size());
    for (size_t i = 0; i < results_before.size(); ++i) {
      REQUIRE(results_before[i].id == results_after[i].id);
      REQUIRE(results_before[i].distance ==
              Approx(results_after[i].distance).margin(1e-6f));
    }

    std::filesystem::remove(path);
  }
}
```

**Step 4: Implement Refactoring** (8-10 hours)

Follow refactoring plan Section 1.3 implementation steps.

#### Rollback Criteria

- [ ] Any corruption not detected
- [ ] Format compatibility broken
- [ ] Round-trip integrity violated
- [ ] Load time increases >10%

---

### Task 1.4: Replace GPU Singleton with Interface

**Addresses**: Issue #6 (GPU Singleton, DIP violation)
**Effort**: 8-12 hours
**Test Effort**: 4-6 hours (40%)

#### Acceptance Criteria

**Functional**:
- [ ] GPU compute results identical to v0.1.0 (bit-exact)
- [ ] Can inject fake GPU for testing
- [ ] Can use multiple GPU devices
- [ ] Backward compatibility maintained (deprecated API)
- [ ] Null object pattern for systems without GPU

**Performance**:
- [ ] GPU operations performance unchanged
- [ ] No virtual dispatch overhead in hot paths

**Testability**:
- [ ] Can run GPU tests on CPU-only systems
- [ ] Can simulate GPU errors
- [ ] Can control GPU behavior in tests

#### Test-First Sequence

**Step 1: Define GPU Interface Tests** (1 hour)

```cpp
// tests/test_gpu_interface.cpp (NEW FILE)

TEST_CASE("GPU Interface - Contract", "[gpu][interface]") {
  SECTION("IGPUCompute interface is complete") {
    // Verify interface has all required methods
    // This is a compile-time check

    class TestGPU : public quiverdb::gpu::IGPUCompute {
    public:
      bool available() const override { return true; }

      std::vector<float> l2(const float* q, const float* v,
                           size_t d, size_t n) override {
        return std::vector<float>(n, 0.0f);
      }

      std::vector<float> dot(const float* q, const float* v,
                            size_t d, size_t n) override {
        return std::vector<float>(n, 0.0f);
      }

      std::vector<float> cosine(const float* q, const float* v,
                               size_t d, size_t n) override {
        return std::vector<float>(n, 0.0f);
      }
    };

    TestGPU gpu;
    REQUIRE(gpu.available());
  }

  SECTION("Factory creates appropriate GPU context") {
    auto gpu = quiverdb::gpu::createGPUCompute();
    REQUIRE(gpu != nullptr);

    #ifdef QUIVER_HAS_METAL
    REQUIRE(gpu->available());  // Should be available on macOS with Metal
    #elif defined(QUIVER_HAS_CUDA)
    // May or may not be available depending on hardware
    #else
    REQUIRE_FALSE(gpu->available());  // Null GPU
    #endif
  }
}
```

**Step 2: Fake GPU Tests** (2 hours)

```cpp
TEST_CASE("GPU Interface - Fake GPU", "[gpu][fake]") {
  SECTION("Fake GPU returns controlled results") {
    auto fake = std::make_unique<quiverdb::gpu::testing::FakeGPUCompute>();

    std::vector<float> expected_dists = {1.0f, 2.0f, 3.0f};
    fake->setTestResults(expected_dists);

    float query[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float vectors[] = {
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };

    auto results = fake->l2(query, vectors, 4, 3);

    REQUIRE(results == expected_dists);
    REQUIRE(fake->getCallCount() == 1);
  }

  SECTION("Fake GPU can simulate errors") {
    auto fake = std::make_unique<quiverdb::gpu::testing::FakeGPUCompute>();
    fake->setShouldFail(true);

    float query[] = {1.0f};
    float vectors[] = {2.0f};

    REQUIRE_THROWS_WITH(
      fake->l2(query, vectors, 1, 1),
      Catch::Matchers::ContainsSubstring("Simulated GPU error")
    );
  }

  SECTION("Fake GPU can simulate unavailability") {
    auto fake = std::make_unique<quiverdb::gpu::testing::FakeGPUCompute>(false);
    REQUIRE_FALSE(fake->available());
  }
}
```

**Step 3: Integration Tests** (1 hour)

```cpp
TEST_CASE("GPU Interface - Integration", "[gpu][integration]") {
  SECTION("VectorStore with injected GPU") {
    // Future: If VectorStore gains GPU support
    // For now, test standalone GPU usage

    auto gpu = quiverdb::gpu::createGPUCompute();

    if (gpu->available()) {
      float query[] = {1.0f, 0.0f, 0.0f, 0.0f};
      float vectors[] = {
        1.0f, 0.0f, 0.0f, 0.0f,  // Identical
        0.0f, 1.0f, 0.0f, 0.0f,  // Orthogonal
        -1.0f, 0.0f, 0.0f, 0.0f  // Opposite
      };

      auto results = gpu->l2(query, vectors, 4, 3);

      REQUIRE(results.size() == 3);
      REQUIRE(results[0] == Approx(0.0f).margin(1e-5f));  // Identical
      REQUIRE(results[1] == Approx(2.0f).margin(1e-5f));  // Orthogonal
      REQUIRE(results[2] == Approx(8.0f).margin(1e-5f));  // Opposite
    }
  }
}
```

**Step 4: Implement Refactoring** (6-8 hours)

Follow refactoring plan Section 1.4 implementation steps.

**Step 5: GPU Hardware Verification** (2 hours)

```bash
# Test on actual GPU hardware

# macOS with Metal
cd build
./tests/test_gpu_metal --success

# NVIDIA with CUDA (if available)
./tests/test_gpu_cuda --success

# Verify backward compatibility
./tests/test_gpu_backward_compat --success

# Performance benchmarks
./benchmarks/bench_gpu \
  --benchmark_filter=BM_GPU_L2 \
  --benchmark_repetitions=10
```

#### Rollback Criteria

- [ ] GPU results diverge from v0.1.0
- [ ] Performance degrades >10%
- [ ] Cannot test GPU code without hardware
- [ ] Backward compatibility broken

---

## Phase 2: Code Quality and Platform Abstraction (Weeks 3-4)

### Task 2.1: Unify Distance Metric Enums

**Effort**: 2-4 hours
**Test Effort**: 1 hour (25%)

#### Acceptance Criteria

- [ ] Single DistanceMetric enum used everywhere
- [ ] No enum conversion functions needed
- [ ] Python bindings still work
- [ ] All tests pass

#### Test-First Sequence

```cpp
TEST_CASE("Distance Metric - Unified Enum", "[distance][enum]") {
  SECTION("All classes use same enum") {
    quiverdb::VectorStore store(4, quiverdb::DistanceMetric::L2);
    quiverdb::HNSWIndex index(4, quiverdb::DistanceMetric::L2, 100);

    // Should compile without conversion
    REQUIRE(store.dimension() == 4);
    REQUIRE(index.dimension() == 4);
  }

  SECTION("Enum serialization is compatible") {
    // Verify that enum values match original values
    REQUIRE(static_cast<uint32_t>(quiverdb::DistanceMetric::L2) == 0);
    REQUIRE(static_cast<uint32_t>(quiverdb::DistanceMetric::COSINE) == 1);
    REQUIRE(static_cast<uint32_t>(quiverdb::DistanceMetric::DOT) == 2);
  }
}
```

---

### Task 2.2: Extract Platform-Specific File Operations

**Effort**: 8-12 hours
**Test Effort**: 4-6 hours (50%)

#### Acceptance Criteria

- [ ] Platform-specific code centralized
- [ ] Works on Windows, Linux, macOS
- [ ] File integrity after sync verified
- [ ] No duplicated fsync code

#### Test-First Sequence

```cpp
// tests/test_platform_file.cpp (NEW FILE)

TEST_CASE("Platform File Operations", "[platform][file]") {
  SECTION("sync_file_to_disk ensures durability") {
    std::string path = "test_sync.dat";

    // Write data
    {
      std::ofstream f(path, std::ios::binary);
      uint64_t data = 0xDEADBEEFCAFEBABE;
      f.write(reinterpret_cast<const char*>(&data), 8);
    }

    // Sync
    REQUIRE_NOTHROW(quiverdb::platform::sync_file_to_disk(path));

    // Verify data persisted
    {
      std::ifstream f(path, std::ios::binary);
      uint64_t data;
      f.read(reinterpret_cast<char*>(&data), 8);
      REQUIRE(data == 0xDEADBEEFCAFEBABE);
    }

    std::filesystem::remove(path);
  }

  SECTION("sync_file_to_disk throws on invalid path") {
    REQUIRE_THROWS_AS(
      quiverdb::platform::sync_file_to_disk("/nonexistent/path.dat"),
      std::runtime_error
    );
  }
}
```

---

### Task 2.3: Replace Magic Numbers with Named Constants

**Effort**: 3-4 hours
**Test Effort**: 0.5 hour (10%) - mostly refactoring

#### Acceptance Criteria

- [ ] All magic numbers replaced
- [ ] Constants documented
- [ ] All tests pass (no functional change)

#### Verification

```bash
# Verify no magic numbers remain
grep -r "1e-12" src/core/ | grep -v constants.h
grep -r "1e-9" src/core/ | grep -v constants.h

# Should return no results
```

---

### Task 2.4: Refactor MMapVectorStore Constructor

**Effort**: 6-8 hours
**Test Effort**: 3-4 hours (40%)

#### Acceptance Criteria

- [ ] Constructor ≤ 30 lines
- [ ] Each validation step testable
- [ ] All corruption scenarios detected
- [ ] Memory mapping works on all platforms

#### Test-First Sequence

```cpp
TEST_CASE("MMapVectorStore - Constructor Refactoring", "[mmap]") {
  SECTION("Validation methods detect corruption") {
    // Test individual validation steps
    // Similar structure to HNSW corruption tests
  }

  SECTION("Memory mapping succeeds on all platforms") {
    // Create valid file
    // Memory map it
    // Verify pointers are valid
  }
}
```

---

## Phase 3: Polish and Low-Priority Improvements (Weeks 5-6)

### Quick Wins (1-2 hours each)

**Tasks**:
1. Improve variable naming
2. Standardize error messages
3. Extract validation functions
4. Refactor HNSWIndex::save()

**Testing**: Minimal (mostly refactoring, no functional changes)

---

## Performance Benchmarking Strategy

### Continuous Performance Monitoring

**Baseline Capture** (Week 0):
```bash
# Capture v0.1.0 baseline before any refactoring
cd build
./benchmarks/bench_distance --benchmark_format=json > baseline_distance.json
./benchmarks/bench_vector_store --benchmark_format=json > baseline_vector_store.json
./benchmarks/bench_hnsw_index --benchmark_format=json > baseline_hnsw.json

# Commit baseline to repo
git add benchmarks/baseline_*.json
git commit -m "Capture v0.1.0 performance baseline"
```

**After Each Refactoring**:
```bash
# Run benchmarks
./benchmarks/bench_<component> --benchmark_format=json > current.json

# Compare with baseline
python3 tools/compare_benchmarks.py \
  baseline_<component>.json \
  current.json \
  --threshold=0.10  # 10% regression threshold
```

**Benchmark Comparison Tool** (NEW FILE):
```python
# tools/compare_benchmarks.py

import json
import sys

def compare_benchmarks(baseline_path, current_path, threshold=0.10):
    with open(baseline_path) as f:
        baseline = json.load(f)
    with open(current_path) as f:
        current = json.load(f)

    regressions = []

    for baseline_bench in baseline['benchmarks']:
        name = baseline_bench['name']
        baseline_time = baseline_bench['real_time']

        # Find matching current benchmark
        current_bench = next((b for b in current['benchmarks']
                             if b['name'] == name), None)

        if not current_bench:
            print(f"Warning: {name} not found in current benchmarks")
            continue

        current_time = current_bench['real_time']

        # Calculate regression
        if baseline_time > 0:
            regression = (current_time - baseline_time) / baseline_time

            if regression > threshold:
                regressions.append({
                    'name': name,
                    'baseline': baseline_time,
                    'current': current_time,
                    'regression': regression * 100
                })

    if regressions:
        print("PERFORMANCE REGRESSIONS DETECTED:")
        for r in regressions:
            print(f"  {r['name']}: {r['regression']:.1f}% slower")
            print(f"    Baseline: {r['baseline']:.2f}ns")
            print(f"    Current:  {r['current']:.2f}ns")
        sys.exit(1)
    else:
        print("All benchmarks within acceptable performance bounds")
        sys.exit(0)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('baseline')
    parser.add_argument('current')
    parser.add_argument('--threshold', type=float, default=0.05)
    args = parser.parse_args()

    compare_benchmarks(args.baseline, args.current, args.threshold)
```

---

## CI/CD Test Strategy

### Enhanced Workflow for Refactoring

Update `.github/workflows/build-and-test.yml`:

```yaml
name: Build and Test (Refactoring Phase)

on:
  push:
    branches: [ main, develop, "refactor/**" ]
  pull_request:
    branches: [ main ]

jobs:
  test-functional:
    name: Functional Tests - ${{ matrix.os }}
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]

    steps:
    - uses: actions/checkout@v4

    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release

    - name: Build
      run: cmake --build build --parallel

    - name: Run Unit Tests
      run: cd build && ctest --output-on-failure

    - name: Upload Test Results
      if: failure()
      uses: actions/upload-artifact@v4
      with:
        name: test-results-${{ matrix.os }}
        path: build/Testing/Temporary/LastTest.log

  test-sanitizers:
    name: Sanitizer Tests
    runs-on: ubuntu-latest
    strategy:
      matrix:
        sanitizer: [asan, tsan, ubsan]

    steps:
    - uses: actions/checkout@v4

    - name: Configure with ${{ matrix.sanitizer }}
      run: |
        if [ "${{ matrix.sanitizer }}" = "asan" ]; then
          cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
        elif [ "${{ matrix.sanitizer }}" = "tsan" ]; then
          cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
        else
          cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON
        fi

    - name: Build
      run: cmake --build build --parallel

    - name: Run Tests
      run: cd build && ctest --output-on-failure

  test-performance:
    name: Performance Benchmarks
    runs-on: macos-latest  # Consistent hardware for benchmarking

    steps:
    - uses: actions/checkout@v4
      with:
        fetch-depth: 0  # Need full history for baseline comparison

    - name: Get Baseline Benchmarks
      run: |
        git show origin/main:benchmarks/baseline_distance.json > baseline_distance.json
        git show origin/main:benchmarks/baseline_hnsw.json > baseline_hnsw.json

    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release

    - name: Build
      run: cmake --build build --parallel

    - name: Run Benchmarks
      run: |
        cd build
        ./benchmarks/bench_distance \
          --benchmark_min_time=0.2s \
          --benchmark_format=json > current_distance.json
        ./benchmarks/bench_hnsw_index \
          --benchmark_min_time=0.2s \
          --benchmark_format=json > current_hnsw.json

    - name: Compare Performance
      run: |
        python3 tools/compare_benchmarks.py \
          baseline_distance.json \
          build/current_distance.json \
          --threshold=0.10
        python3 tools/compare_benchmarks.py \
          baseline_hnsw.json \
          build/current_hnsw.json \
          --threshold=0.10

    - name: Upload Benchmark Results
      uses: actions/upload-artifact@v4
      with:
        name: benchmarks
        path: build/current_*.json

  test-characterization:
    name: Characterization Tests
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v4

    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQUIVERDB_ENABLE_TESTING=ON

    - name: Build
      run: cmake --build build --parallel

    - name: Run Characterization Tests
      run: |
        cd build
        ./tests/test_hnsw_add_characterization --success
        ./tests/test_hnsw_corruption --success

  test-coverage:
    name: Code Coverage
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v4

    - name: Install lcov
      run: sudo apt-get install -y lcov

    - name: Configure with Coverage
      run: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

    - name: Build
      run: cmake --build build --parallel

    - name: Run Tests
      run: cd build && ctest

    - name: Generate Coverage Report
      run: |
        cd build
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' --output-file coverage.info
        lcov --list coverage.info

    - name: Upload to Codecov
      uses: codecov/codecov-action@v4
      with:
        files: ./build/coverage.info
        fail_ci_if_error: true

    - name: Check Coverage Threshold
      run: |
        coverage=$(lcov --summary build/coverage.info | grep lines | awk '{print $2}' | sed 's/%//')
        if (( $(echo "$coverage < 95.0" | bc -l) )); then
          echo "Coverage $coverage% is below 95% threshold"
          exit 1
        fi

  test-integration:
    name: Integration Tests (Python Bindings)
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        python-version: ["3.9", "3.11"]

    steps:
    - uses: actions/checkout@v4

    - uses: actions/setup-python@v5
      with:
        python-version: ${{ matrix.python-version }}

    - name: Install uv
      run: pip install uv

    - name: Build Python Bindings
      run: uv pip install -e .

    - name: Run Python Tests
      run: uv run pytest tests/test_python_bindings.py -v
```

---

## Rollback Criteria and Procedures

### Automated Rollback Triggers

The CI/CD pipeline automatically fails (and triggers rollback discussion) if:

1. **Test Failures**
   - Any unit test fails
   - Any integration test fails
   - Coverage drops below 95%

2. **Performance Regression**
   - Any benchmark exceeds 110% of baseline
   - Memory usage increases >10%

3. **Memory Safety**
   - ASan detects leaks or overruns
   - TSan detects race conditions
   - UBSan detects undefined behavior
   - Valgrind reports errors

4. **API Compatibility**
   - Python bindings fail
   - Example code no longer compiles

### Manual Rollback Criteria

**Consider rollback** if:
- Code complexity increases (subjective review)
- Build time increases >20%
- Team cannot understand new abstractions
- Refactoring scope creeps beyond plan

### Rollback Procedure

1. **Immediate**: Revert commit/PR
2. **Analysis**: Identify root cause
3. **Fix**: Either fix the issue or defer refactoring
4. **Retry**: Once fixes validated, retry refactoring

```bash
# SAFE rollback procedure (preserves history)
git revert <commit-sha> --no-edit
git push origin refactor/<task-name>

# For multiple commits:
git revert HEAD~3..HEAD --no-edit
git push origin refactor/<task-name>

# Document reason
echo "Rollback reason: <description>" > rollback_log.txt
git add rollback_log.txt
git commit -m "Document rollback of <task>"
```

> **WARNING**: Never use `git reset --hard` or `git push --force` without team lead approval.
> These commands destroy history and can cause data loss.

---

## Testing Milestones and Verification Checkpoints

### Week 1 Milestones

**End of Week 1**:
- [ ] Distance strategy refactoring complete
- [ ] All 131k+ assertions pass
- [ ] Performance within 10% of baseline
- [ ] Zero sanitizer errors
- [ ] Code coverage ≥ 95%

**Verification**:
```bash
cd build
ctest --output-on-failure  # All tests pass
./tools/run_sanitizers.sh  # Clean
./tools/check_coverage.sh  # ≥95%
./tools/benchmark_all.sh   # Within bounds
```

### Week 2 Milestones

**End of Week 2**:
- [ ] HNSWIndex::add() refactored (≤10 lines)
- [ ] HNSWIndex::load() refactored (≤30 lines)
- [ ] All graph structure tests pass
- [ ] Round-trip integrity verified
- [ ] Concurrency tests pass (TSan clean)

**Verification**:
```bash
cd build
./tests/test_hnsw_add_characterization --success
./tests/test_hnsw_corruption --success
cd ../build_tsan
./tests/test_hnsw_index --success  # With TSan
```

### Week 3 Milestones

**End of Week 3**:
- [ ] GPU interface refactoring complete
- [ ] Can test GPU code without hardware
- [ ] Platform abstraction extracted
- [ ] Works on Windows/Linux/macOS

**Verification**:
```bash
# CPU-only system can run GPU tests
./tests/test_gpu_interface --success

# Platform-specific code centralized
grep -r "ifdef.*WINDOWS" src/core/ | wc -l  # Should be reduced
```

### Week 4 Milestones

**End of Week 4**:
- [ ] All Phase 2 tasks complete
- [ ] Magic numbers eliminated
- [ ] Enums unified
- [ ] MMapVectorStore constructor refactored

**Verification**:
```bash
# No magic numbers
grep -r "1e-12" src/core/ | grep -v constants.h  # Empty
grep -r "1e-9" src/core/ | grep -v constants.h   # Empty

# Single enum
grep -r "HNSWDistanceMetric" src/core/  # Should not exist
```

### Week 5-6 Milestones

**End of Week 6**:
- [ ] All refactoring complete
- [ ] Documentation updated
- [ ] Migration guide written
- [ ] v0.2.0 ready for release

**Final Verification**:
```bash
# Full test suite
cd build && ctest --output-on-failure

# Performance
./tools/benchmark_all.sh
python3 tools/compare_benchmarks.py \
  ../benchmarks/baseline_*.json \
  current_*.json

# Memory safety
./tools/run_sanitizers.sh

# API compatibility
cd ../python
uv run pytest tests/

# Code quality metrics
cloc src/core/  # Check LOC
./tools/check_complexity.sh  # Cyclomatic complexity
```

---

## Timeline with Testing Milestones

```
Week 1: Distance Strategy + HNSWIndex::add()
├─ Day 1: Write characterization tests for distance calculation
├─ Day 2: Implement distance strategy, verify tests pass
├─ Day 3: Write graph structure characterization tests
├─ Day 4: Refactor add(), verify tests pass
└─ Day 5: Performance validation, documentation

Week 2: HNSWIndex::load() + GPU Interface
├─ Day 1: Write corruption detection tests (12 scenarios)
├─ Day 2: Write format compatibility tests
├─ Day 3: Refactor load(), verify tests pass
├─ Day 4: Write GPU interface tests with fakes
└─ Day 5: Refactor GPU singleton, verify on hardware

Week 3: Platform Abstraction + Enum Unification
├─ Day 1: Write platform file operation tests
├─ Day 2: Extract platform abstraction
├─ Day 3: Test on Windows/Linux/macOS
├─ Day 4: Unify enums, eliminate magic numbers
└─ Day 5: Integration testing, benchmark validation

Week 4: MMapVectorStore + save() Refactoring
├─ Day 1: Write MMap constructor tests
├─ Day 2: Refactor MMap constructor
├─ Day 3: Write save() tests
├─ Day 4: Refactor save()
└─ Day 5: Full regression testing

Week 5-6: Polish and Documentation
├─ Variable naming improvements
├─ Error message standardization
├─ Documentation updates
├─ Migration guide
└─ Final validation and v0.2.0 prep
```

---

## Success Metrics

### Quantitative Goals (Measurable)

| Metric | Baseline (v0.1.0) | Target (v0.2.0) | Measured By |
|--------|-------------------|-----------------|-------------|
| **Code Quality** |
| Longest method | 96 lines | ≤40 lines | Line count |
| Methods >50 lines | 3 | 0 | Static analysis |
| Duplicated code blocks | 5 instances | 0 | Manual inspection |
| Magic numbers | 12+ | 0 | grep scan |
| **Test Coverage** |
| Line coverage | 95% | ≥95% | lcov |
| Branch coverage | 85% | ≥90% | lcov |
| Test count (C++) | 38 | ≥50 | ctest |
| Assertions | 131k+ | ≥150k | test output |
| **Performance** |
| L2 distance (768d) | 100ns | ≤105ns | Google Benchmark |
| HNSW add() | 50μs | ≤52.5μs | Google Benchmark |
| HNSW search | 200μs | ≤210μs | Google Benchmark |
| GPU search | 2.5ms | ≤2.6ms | Google Benchmark |
| **Memory Safety** |
| ASan errors | 0 | 0 | AddressSanitizer |
| TSan errors | 0 | 0 | ThreadSanitizer |
| UBSan errors | 0 | 0 | UBSan |
| Valgrind leaks | 0 | 0 | Valgrind |
| **Build System** |
| Build time | baseline | ≤110% | time measurement |
| Binary size | baseline | ≤110% | ls -lh |

### Qualitative Goals (Subjective)

- [ ] **Extensibility**: Adding new distance metric takes <4 hours (vs 2 days)
- [ ] **Testability**: Can test GPU code without hardware
- [ ] **Readability**: New developer understands HNSW algorithm flow in <1 hour
- [ ] **Maintainability**: Bug fix requires changes in 1 location (vs 3+)
- [ ] **Portability**: Adding new platform takes <1 day (vs 3-4 days)

### Business Impact Goals

- [ ] **Development Velocity**: 3x faster feature development (tracked via story points)
- [ ] **Bug Risk**: 60% reduction in post-release bugs (tracked over 3 months)
- [ ] **Onboarding**: New developer productive in 1 week (vs 2)
- [ ] **Roadmap Readiness**: Can start v0.2.0 features (PyPI, npm, distributed) without technical debt blocking

---

## Risk Management

### High-Risk Items

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Performance regression | Medium | High | Continuous benchmarking, 5% threshold |
| Graph structure changes | Medium | Critical | Characterization tests, bit-exact comparison |
| Concurrency bugs | Medium | High | TSan on every commit, stress testing |
| API breakage | Low | High | Python binding tests, backward compat |
| Schedule overrun | Medium | Medium | Phase-based approach, can stop after Phase 1 |

### Risk Mitigation Strategies

1. **Performance Regression**
   - Run benchmarks after every commit
   - Compare with baseline automatically in CI
   - Hard fail if >10% regression

2. **Correctness Issues**
   - 131k+ existing assertions must pass
   - Characterization tests capture current behavior
   - Round-trip testing for persistence

3. **Concurrency Bugs**
   - TSan enabled for all concurrency tests
   - Stress testing with 100+ concurrent operations
   - Lock analysis and deadlock detection

4. **API Breakage**
   - Python bindings tested on every commit
   - Deprecated APIs maintained temporarily
   - Migration guide for any breaking changes

---

## Documentation and Knowledge Transfer

### Architecture Decision Records (ADRs)

Create ADRs for major refactoring decisions:

```markdown
# ADR-001: Distance Calculation Strategy Pattern

Date: 2026-01-26

## Status
Accepted

## Context
We had duplicated distance calculation switch statements in 3 locations.
Adding new distance metrics required modifying multiple files.

## Decision
Extract distance calculation to Strategy Pattern using function pointers.

## Consequences
Positive:
- Single location for distance metric logic
- Can add custom metrics without modifying library
- Eliminates code duplication

Negative:
- Slight function pointer overhead (within 10% tolerance)
- More abstraction may increase learning curve

## Verification
- All 131k+ tests pass
- Performance within 10% of baseline
- Can register custom distance function
```

### Migration Guide

```markdown
# QuiverDB v0.1.0 → v0.2.0 Migration Guide

## API Changes

### Unified Distance Metric Enum

**Before (v0.1.0)**:
```cpp
HNSWIndex idx(768, HNSWDistanceMetric::L2, 100000);
```

**After (v0.2.0)**:
```cpp
HNSWIndex idx(768, DistanceMetric::L2, 100000);
```

**Migration**: Replace `HNSWDistanceMetric` with `DistanceMetric`.

### GPU Interface (Optional)

**Before (v0.1.0)**:
```cpp
auto& gpu = quiverdb::gpu::MetalCompute::get();
```

**After (v0.2.0)** - Recommended:
```cpp
auto gpu = quiverdb::gpu::createGPUCompute();
```

**After (v0.2.0)** - Backward Compatible:
```cpp
auto& gpu = quiverdb::gpu::MetalCompute::get();  // Deprecated
```

## Behavioral Changes

None. All behavior is preserved from v0.1.0.
```

---

## Tools and Automation

### Test Automation Scripts

**Run All Verification** (`tools/verify_all.sh`):
```bash
#!/bin/bash
set -e

echo "Running full verification suite..."

# Unit tests
echo "1. Unit Tests"
cd build && ctest --output-on-failure

# Sanitizers
echo "2. Address Sanitizer"
cd ../build_asan && ctest --output-on-failure

echo "3. Thread Sanitizer"
cd ../build_tsan && ctest --output-on-failure

echo "4. Undefined Behavior Sanitizer"
cd ../build_ubsan && ctest --output-on-failure

# Coverage
echo "5. Code Coverage"
cd ../build_coverage
ctest
lcov --capture --directory . --output-file coverage.info
coverage=$(lcov --summary coverage.info | grep lines | awk '{print $2}' | sed 's/%//')
if (( $(echo "$coverage < 95.0" | bc -l) )); then
  echo "FAIL: Coverage $coverage% below 95%"
  exit 1
fi
echo "PASS: Coverage $coverage%"

# Performance
echo "6. Performance Benchmarks"
cd ../build
./tools/benchmark_all.sh

echo "All verification passed!"
```

**Benchmark All** (`tools/benchmark_all.sh`):
```bash
#!/bin/bash
set -e

echo "Running performance benchmarks..."

components=("distance" "vector_store" "hnsw_index")

for comp in "${components[@]}"; do
  echo "Benchmarking $comp..."

  ./benchmarks/bench_$comp \
    --benchmark_min_time=0.2s \
    --benchmark_format=json > current_$comp.json

  if [ -f "../benchmarks/baseline_$comp.json" ]; then
    python3 ../tools/compare_benchmarks.py \
      ../benchmarks/baseline_$comp.json \
      current_$comp.json \
      --threshold=0.10
  fi
done

echo "All benchmarks passed!"
```

---

## Conclusion

This ATDD implementation plan provides a comprehensive, test-driven approach to refactoring QuiverDB. Key principles:

1. **Test-First**: Write tests before refactoring
2. **Behavioral Preservation**: All 131k+ assertions must pass
3. **Performance Monitoring**: Continuous benchmarking with 5% threshold
4. **Incremental Progress**: Phase-based approach with clear milestones
5. **Automatic Rollback**: Clear criteria for when to revert
6. **CI/CD Integration**: Automated verification on every commit

**Success Criteria**: After 6-8 weeks, QuiverDB will have:
- Cleaner architecture (SOLID principles)
- Better testability (injectable dependencies)
- Maintained performance (within 10%)
- Preserved behavior (all tests pass)
- Ready for v0.2.0 features

**Next Steps**:
1. Review and approve this plan with team
2. Set up enhanced CI/CD pipeline
3. Capture v0.1.0 baseline benchmarks
4. Begin Phase 1, Week 1: Distance Strategy refactoring

---

**Document Version**: 1.0
**Last Updated**: 2026-01-26
**Approved By**: [Pending]
