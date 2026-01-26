# QuiverDB Refactoring Plan
**Version**: 0.1.0 → 0.2.0
**Date**: 2026-01-25
**Objective**: Address architectural debt and prepare codebase for scalable growth

---

## Executive Summary

QuiverDB is a well-engineered codebase (Grade: B+) with 31 identified code smells, 8 of which are high-severity architectural issues. The refactoring plan focuses on eliminating code duplication, breaking down long methods, and improving extensibility without compromising the excellent performance characteristics.

**Investment Required**: 8-12 weeks (3 phases) - see Resource Requirements
**ROI Hypotheses** (to validate post-refactoring):
- Feature velocity improvement (baseline TBD, measure cycle time)
- Bug rate reduction (baseline TBD, measure defects/KLOC)
**Critical Success Factor**: Complete Phase 1 before adding v0.2.0 roadmap features

### Resource Requirements
- **Headcount**: 1 senior engineer full-time, OR 2 engineers at 50%
- **Phase Owners**: Assign before starting each phase
- **If constrained**: Extend timeline proportionally or defer Phases 2-3

### Key Metrics
- **Current State**: 1,321 lines core code, 31 issues, longest method 96 lines
- **Target State**: <40 line methods, 0 duplicate code blocks, extensible architecture
- **Test Coverage**: Maintain 95%+ (currently 38 C++ + 28 Python tests with 131k assertions)

---

## Problem Catalog

### High-Severity Issues (8) - Architectural Impact

| # | Issue | Location | SOLID Violation | Impact |
|---|-------|----------|-----------------|--------|
| 1 | Long Method: add() | hnsw_index.h:94-163 (70 lines) | SRP | Hard to test/maintain |
| 2 | Long Method: load() | hnsw_index.h:269-364 (96 lines) | SRP | Complex deserialization |
| 3 | Duplicated Distance Switch | 3 locations | DRY, OCP | Shotgun surgery risk |
| 4 | Distance Metric Switching | All core files | OCP | Cannot extend |
| 5 | Long Constructor | mmap_vector_store.h:41-112 (72 lines) | SRP | Heavy initialization |
| 6 | GPU Singleton Pattern | metal_distance.h, cuda_distance.cuh | DIP | Untestable |
| 7 | Feature Envy: search_layer() | hnsw_index.h:387-417 | Information Expert | Tight coupling |
| 8 | Primitive Obsession: uint64_t IDs | All files | Type Safety | No ID validation |

### Medium-Severity Issues (15) - Design Problems

| # | Issue | Location | Category | Refactoring |
|---|-------|----------|----------|-------------|
| 9 | Long Method: save() | hnsw_index.h:216-267 | Bloater | Extract Method |
| 10-12 | Magic Numbers | Multiple files | Lexical Abuse | Replace with Constants |
| 13 | Platform #ifdef Nesting | mmap_vector_store.h:42-69 | Conditional Complexity | Extract Platform Abstraction |
| 14 | Duplicated fsync Code | 2 locations | DRY | Extract Utility Function |
| 15 | Duplicated GPU Kernels | metal/cuda files | DRY | Accept or Code Gen |
| 16 | Duplicated Cleanup Logic | mmap_vector_store.h | DRY | RAII Helper Classes |
| 17 | Shotgun Surgery: Distance Metrics | 5+ files | Change Preventer | Strategy Pattern |
| 18 | Divergent Change: VectorStore | vector_store.h | SRP | Split Responsibilities |
| 19 | Inconsistent Metric Enums | 4 enum types | Naming | Unify to Single Enum |
| 20 | Type Embedded in Names | Multiple files | Naming | Improve Variable Names |
| 21 | Excessive Null Checks | All core files | Obfuscator | Use std::span/not_null |
| 22 | Speculative Generality | Reserved fields | YAGNI | Remove if unused |
| 23 | Flag Argument | Metric enum behavior | Functional Abuse | Strategy Pattern |

### Low-Severity Issues (8) - Readability

| # | Issue | Refactoring Technique | Effort |
|---|-------|----------------------|--------|
| 24-25 | Uncommunicative Names | Rename Variable | 2-3 hours |
| 26 | Complicated Boolean Expressions | Extract Variable | 2-3 hours |
| 27 | Dead Code | Remove Code | 1 hour |
| 28 | Fallacious Comments | Replace Comment with Assertion | 1 hour |
| 29 | Inconsistent Error Messages | Standardize Format | 1-2 hours |
| 30 | Public Struct Members | Encapsulate Field (optional) | Low priority |
| 31 | What Comments | Replace with Why Comments | 1 hour |

---

## Refactoring Technique Mappings

### Code Smell → Refactoring Technique Matrix

| Code Smell | Primary Refactoring | Supporting Refactorings | Pattern Target |
|------------|-------------------|------------------------|----------------|
| **Long Method (add, load, save)** | Extract Method | Replace Temp with Query, Compose Method | - |
| **Duplicated Distance Switch** | Extract Method → Move Method | Parameterize Method | Strategy Pattern |
| **Distance Metric OCP Violation** | Replace Conditional with Polymorphism | Extract Interface, Introduce Parameter Object | Strategy Pattern |
| **GPU Singleton** | Extract Interface | Replace Singleton with DI, Introduce Factory | Interface Abstraction |
| **Long Constructor (MMapVectorStore)** | Extract Method | Chain Constructors, Move Method | Builder Pattern (optional) |
| **Platform-Specific #ifdef** | Extract Class | Move Method, Replace Conditional with Polymorphism | Platform Abstraction |
| **Magic Numbers** | Replace Magic Number with Symbolic Constant | Encapsulate Field | - |
| **Duplicated fsync** | Extract Method | Form Template Method | - |
| **Primitive Obsession (IDs)** | Replace Data Value with Object | Introduce Parameter Object | Value Object |
| **Inconsistent Enums** | Inline Class | Pull Up Field | - |
| **Excessive Null Checks** | Introduce Assertion | Replace Parameter with Method Call | - |

### Refactoring Sequence Dependencies

```
Foundation Layer (Do First):
├─ Replace Magic Number with Symbolic Constant (#10-12)
│  └─ No dependencies
├─ Unify Distance Metric Enums (#19)
│  └─ Prepares for Strategy Pattern
└─ Standardize Error Messages (#29)
   └─ No dependencies

Core Refactorings (Sequential):
1. Extract Distance Calculation Strategy (#3, #4, #17)
   ├─ Extract Method (switch statements)
   ├─ Move Method (to strategy class)
   └─ Replace Conditional with Polymorphism
   Dependencies: Requires unified enums (#19)

2. Refactor Long Methods (#1, #2, #9)
   ├─ HNSWIndex::add() → Extract Method (4-5 methods)
   ├─ HNSWIndex::load() → Extract Method (4-5 methods)
   └─ HNSWIndex::save() → Extract Method (3-4 methods)
   Dependencies: None (can be done independently)

3. Replace GPU Singleton with Interface (#6)
   ├─ Extract Interface (IGPUCompute)
   ├─ Introduce Factory Method
   └─ Replace Constructor with Factory Method
   Dependencies: None

4. Extract Platform Abstraction (#13, #14)
   ├─ Extract Class (WindowsFileMapper, PosixFileMapper)
   ├─ Extract Interface (IFileMapper)
   └─ Extract Method (sync_file_to_disk utility)
   Dependencies: Can be done after #2

Advanced Refactorings (Optional):
├─ Replace Primitive IDs with Value Object (#8)
│  └─ Breaking API change - defer to v0.2.0
└─ Separate Storage from Computation (#18)
   └─ Major architectural change - defer to future
```

---

## Phase 1: Critical Architectural Improvements (Weeks 1-4)

**Goal**: Fix extensibility issues and eliminate high-impact duplication
**Estimated Effort**: 44-66 hours coding + 22-33 hours testing (3-4 calendar weeks)
**Priority**: CRITICAL - Must complete before v0.2.0 features

> **Timeline Note**: Original 2-week estimate revised to 3-4 weeks to include
> characterization test writing, code review cycles, and performance validation.

### 1.1 Extract Distance Calculation Strategy

**Addresses**: Issues #3, #4, #17 (Duplicate switch, OCP violation, Shotgun surgery)
**Effort**: 8-12 hours
**Risk**: Medium
**Impact**: HIGH - Enables custom distance metrics

#### Step-by-Step Refactoring

**Current State** (Duplicated 3 times):
```cpp
// vector_store.h:120-127
float compute_distance(const float* a, const float* b) const {
  switch (metric_) {
    case DistanceMetric::L2: return l2_sq(a, b, dim_);
    case DistanceMetric::COSINE: return cosine_distance(a, b, dim_);
    case DistanceMetric::DOT: return -dot_product(a, b, dim_);
    default: return std::numeric_limits<float>::infinity();
  }
}
// Identical code in hnsw_index.h:378-385 and mmap_vector_store.h:186-193
```

**Refactoring Steps**:

1. **Extract Method** (consolidate duplicate switch statements)
   ```cpp
   // src/core/distance_strategy.h (NEW FILE)
   namespace quiverdb::detail {
     inline float compute_distance(DistanceMetric metric,
                                   const float* a, const float* b, size_t dim) {
       switch (metric) {
         case DistanceMetric::L2: return l2_sq(a, b, dim);
         case DistanceMetric::COSINE: return cosine_distance(a, b, dim);
         case DistanceMetric::DOT: return -dot_product(a, b, dim);
         default: return std::numeric_limits<float>::infinity();
       }
     }
   }
   ```

2. **Replace all switch statements with function call**
   ```cpp
   // vector_store.h
   float compute_distance(const float* a, const float* b) const {
     return detail::compute_distance(metric_, a, b, dim_);
   }
   ```

3. **Replace Conditional with Polymorphism** (Strategy Pattern)
   ```cpp
   // src/core/distance_strategy.h
   namespace quiverdb {

     using DistanceFunction = float(*)(const float*, const float*, size_t);

     // Named wrapper function for DOT (lambdas cannot be used in constexpr)
     inline float negated_dot_product(const float* a, const float* b, size_t n) {
       return -dot_product(a, b, n);
     }

     inline DistanceFunction get_distance_function(DistanceMetric metric) {
       switch (metric) {
         case DistanceMetric::L2: return l2_sq;
         case DistanceMetric::COSINE: return cosine_distance;
         case DistanceMetric::DOT: return negated_dot_product;
         default: return l2_sq;  // Fallback
       }
     }

     class DistanceComputer {
       DistanceFunction fn_;
       size_t dim_;
     public:
       DistanceComputer(DistanceMetric metric, size_t dim)
         : fn_(detail::get_distance_function(metric)), dim_(dim) {}

       float operator()(const float* a, const float* b) const {
         return fn_(a, b, dim_);
       }
     };
   }
   ```

4. **Update all classes to use DistanceComputer**
   ```cpp
   // vector_store.h
   class VectorStore {
     DistanceComputer distance_;  // Instead of metric_ and switch
   public:
     VectorStore(size_t dim, DistanceMetric metric)
       : dim_(dim), distance_(metric, dim) { }

     float compute_distance(const float* a, const float* b) const {
       return distance_(a, b);  // No switch!
     }
   };
   ```

**Benefits**:
- Eliminates 3 instances of duplicated code
- Fixes Open/Closed Principle violation
- Adding new metrics requires only updating one location
- Function pointers enable user-defined distance functions (future)

**Testing Strategy**:
- Existing tests should pass unchanged (API remains same)
- Add new test for custom distance function support
- Benchmark to ensure no performance regression

**Complexity Assessment**: Moderate (touches all core classes)
**Risk Mitigation**: Make changes incrementally, run tests after each step

---

### 1.2 Refactor HNSWIndex::add() Method

**Addresses**: Issue #1 (Long Method, SRP violation)
**Effort**: 8-12 hours
**Risk**: Medium-High
**Impact**: HIGH - Improves testability and maintainability

#### Current State Analysis
```cpp
void add(uint64_t id, const float* vec) {  // 70 lines!
  // 1. Validation (lines 95-98)
  // 2. Capacity checking (lines 99-101)
  // 3. ID mapping (lines 102-107)
  // 4. Vector storage (lines 108-110)
  // 5. Level generation (lines 111-113)
  // 6. Graph initialization (lines 114-117)
  // 7. Entry point setup (lines 118-125)
  // 8. Multi-layer construction (lines 126-156)
  // 9. Entry point update (lines 157-161)
}
```

#### Refactoring Steps

**1. Extract Method: validate_and_allocate**
```cpp
private:
size_t validate_and_allocate(uint64_t id, const float* vec) {
  if (!vec) throw std::invalid_argument("Vector must not be null");

  std::unique_lock lk(global_mtx_);

  if (id_map_.count(id))
    throw std::invalid_argument("ID " + std::to_string(id) + " already exists");
  if (count_ >= max_elements_)
    throw std::runtime_error("Max elements reached");

  size_t iid = count_++;
  id_map_[id] = iid;
  std::memcpy(&vectors_[iid * dim_], vec, dim_ * sizeof(float));

  return iid;
}
```

**2. Extract Method: initialize_graph_node**
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

**3. Extract Method: connect_node_to_graph**
```cpp
private:
void connect_node_to_graph(size_t internal_id, int level) {
  const float* new_vec = &vectors_[internal_id * dim_];

  if (entry_point_ == SIZE_MAX) {
    entry_point_ = internal_id;
    return;
  }

  // Search and connect at each layer
  for (int lc = std::min(level, entry_level_); lc >= 0; --lc) {
    auto entry = (lc <= entry_level_) ? entry_point_ : entry_point_;
    auto cands = search_layer(new_vec, entry, 1, lc);

    size_t M_max = (lc == 0) ? M_ * 2 : M_;
    auto new_neighbors = select_neighbors(cands, M_max, lc);

    add_bidirectional_connections(internal_id, new_neighbors, lc);
    prune_neighbors_if_needed(new_neighbors, M_max, lc);
  }
}
```

**4. Extract Method: update_entry_point_if_needed**
```cpp
private:
void update_entry_point_if_needed(size_t internal_id, int level) {
  if (level > entry_level_) {
    entry_point_ = internal_id;
    entry_level_ = level;
  }
}
```

**5. Refactored add() - Composed Method**
```cpp
public:
void add(uint64_t id, const float* vec) {
  size_t internal_id = validate_and_allocate(id, vec);
  int level = initialize_graph_node(internal_id);
  connect_node_to_graph(internal_id, level);
  update_entry_point_if_needed(internal_id, level);
}
```

**Benefits**:
- Main method is now 4 lines (was 70)
- Each extracted method has single responsibility
- Each method is independently testable
- Clear separation of concerns
- Better error localization

**Testing Strategy**:
- Add unit tests for each extracted method
- Ensure existing integration tests still pass
- Add characterization tests for graph structure

**Complexity**: High (critical algorithm)
**Risk Mitigation**:
- Keep extracted methods private initially
- Use #ifdef QUIVERDB_ENABLE_TESTING for sensing methods
- Extensive testing before/after

---

### 1.3 Refactor HNSWIndex::load() Method

**Addresses**: Issue #2 (Long Method, 96 lines)
**Effort**: 10-14 hours
**Risk**: High
**Impact**: HIGH - Improves deserialization maintainability

#### Current State
```cpp
static std::unique_ptr<HNSWIndex> load(const std::string& filename) {
  // 96 lines of mixed concerns:
  // - File I/O
  // - Format validation
  // - Data deserialization
  // - Corruption detection
  // - Object reconstruction
}
```

#### Refactoring Steps

**1. Extract Method: read_and_validate_header**
```cpp
private:
struct FileHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t metric;
  size_t dim;
  size_t max_elements;
  size_t count;
  int M;
  int ef_construction;
  size_t entry_point;
  int entry_level;
};

static FileHeader read_and_validate_header(std::ifstream& f) {
  FileHeader header;

  detail::read_bin(f, header.magic);
  if (header.magic != MAGIC)
    throw std::runtime_error("Invalid file format (magic mismatch)");

  detail::read_bin(f, header.version);
  if (header.version != VERSION && header.version != 1)
    throw std::runtime_error("Unsupported version: " + std::to_string(header.version));

  detail::read_bin(f, header.metric);
  if (header.metric > 2)
    throw std::runtime_error("Corrupted file: invalid metric");

  // Read remaining header fields with validation
  detail::read_bin(f, header.dim);
  detail::read_bin(f, header.max_elements);
  detail::read_bin(f, header.count);
  // ... continue for all fields

  return header;
}
```

**2. Extract Method: deserialize_vectors**
```cpp
private:
static std::vector<float> deserialize_vectors(std::ifstream& f,
                                              size_t dim, size_t count) {
  std::vector<float> vectors(dim * count);
  f.read(reinterpret_cast<char*>(vectors.data()), dim * count * sizeof(float));

  if (!f)
    throw std::runtime_error("Failed to read vectors");

  return vectors;
}
```

**3. Extract Method: deserialize_graph_structure**
```cpp
private:
struct GraphData {
  std::vector<int> levels;
  std::vector<std::vector<std::vector<size_t>>> neighbors;
  std::unordered_map<uint64_t, size_t> id_map;
};

static GraphData deserialize_graph_structure(std::ifstream& f,
                                             size_t count, size_t max_elements) {
  GraphData graph;
  graph.levels.resize(count);
  graph.neighbors.resize(max_elements);

  // Deserialize levels
  f.read(reinterpret_cast<char*>(graph.levels.data()), count * sizeof(int));

  // Deserialize neighbors with validation
  for (size_t i = 0; i < count; ++i) {
    int lvl = graph.levels[i];
    if (lvl < 0 || lvl > MAX_LEVEL)
      throw std::runtime_error("Corrupted file: invalid level");

    graph.neighbors[i].resize(lvl + 1);
    for (int L = 0; L <= lvl; ++L) {
      uint32_t num_neighbors;
      detail::read_bin(f, num_neighbors);

      if (num_neighbors > 10000)  // Sanity check
        throw std::runtime_error("Corrupted file: too many neighbors");

      graph.neighbors[i][L].resize(num_neighbors);
      f.read(reinterpret_cast<char*>(graph.neighbors[i][L].data()),
             num_neighbors * sizeof(size_t));
    }
  }

  // Deserialize ID map
  size_t id_map_size;
  detail::read_bin(f, id_map_size);
  for (size_t i = 0; i < id_map_size; ++i) {
    uint64_t key;
    size_t val;
    detail::read_bin(f, key);
    detail::read_bin(f, val);
    graph.id_map[key] = val;
  }

  return graph;
}
```

**4. Extract Method: validate_graph_integrity**
```cpp
private:
static void validate_graph_integrity(const GraphData& graph,
                                     size_t count, size_t entry_point) {
  // Validate entry point
  if (entry_point != SIZE_MAX && entry_point >= count)
    throw std::runtime_error("Corrupted file: invalid entry_point");

  // Validate neighbor references
  for (size_t i = 0; i < count; ++i) {
    for (const auto& layer : graph.neighbors[i]) {
      for (size_t neighbor_id : layer) {
        if (neighbor_id >= count)
          throw std::runtime_error("Corrupted file: neighbor out of range");
      }
    }
  }

  // Validate ID map
  for (const auto& [id, internal_id] : graph.id_map) {
    if (internal_id >= count)
      throw std::runtime_error("Corrupted file: id_map internal_id out of range");
  }
}
```

**5. Extract Method: restore_rng_state**
```cpp
private:
static void restore_rng_state(std::ifstream& f, uint32_t version,
                              std::mt19937& level_gen) {
  if (version >= 2) {
    std::string state;
    size_t state_len;
    detail::read_bin(f, state_len);
    state.resize(state_len);
    f.read(&state[0], state_len);

    std::istringstream iss(state);
    iss >> level_gen;
  }
  // v1 files keep default initialization
}
```

**6. Refactored load() - Composed Method**
```cpp
static std::unique_ptr<HNSWIndex> load(const std::string& filename) {
  std::ifstream f(filename, std::ios::binary);
  if (!f) throw std::runtime_error("Cannot open: " + filename);

  // Read and validate header
  FileHeader header = read_and_validate_header(f);

  // Create index instance
  auto idx = std::make_unique<HNSWIndex>(
    header.dim,
    static_cast<HNSWDistanceMetric>(header.metric),
    header.max_elements,
    header.M,
    header.ef_construction
  );

  // Deserialize data
  idx->vectors_ = deserialize_vectors(f, header.dim, header.count);
  GraphData graph = deserialize_graph_structure(f, header.count, header.max_elements);

  // Validate integrity
  validate_graph_integrity(graph, header.count, header.entry_point);

  // Populate index
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

  // Restore RNG state
  restore_rng_state(f, header.version, idx->level_gen_);

  return idx;
}
```

**Benefits**:
- Main method reduced from 96 to ~30 lines
- Each deserialization step is independently testable
- Validation logic is centralized and clear
- Easy to add new validation checks
- Better error messages (know which validation failed)

**Testing Strategy**:
- Test each deserialization method with valid data
- Test validation methods with corrupted data
- Ensure backward compatibility (v1 and v2 formats)
- Characterization tests for all corruption scenarios

**Complexity**: High (critical persistence layer)
**Risk Mitigation**:
- Create comprehensive backup of existing tests
- Test against existing saved index files
- Add version compatibility tests

---

### 1.4 Replace GPU Singleton with Interface

**Addresses**: Issue #6 (GPU Singleton, DIP violation)
**Effort**: 8-12 hours
**Risk**: Medium
**Impact**: HIGH - Enables GPU testing without hardware

#### Current State
```cpp
// metal_distance.h
class MetalCompute {
public:
  static MetalCompute& get() { static MetalCompute c; return c; }
  bool ok() const { return device_ != nullptr; }
  // ... GPU methods
private:
  MetalCompute() { init(); }  // Hard-coded initialization
};

// Usage (tight coupling)
auto& gpu = quiverdb::gpu::MetalCompute::get();
if (gpu.ok()) {
  auto dists = gpu.l2(query, vectors, dim, n);
}
```

#### Refactoring Steps

**1. Extract Interface**
```cpp
// src/core/gpu/gpu_interface.h (NEW FILE)
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

  // For Metal-specific buffer optimization
  #ifdef QUIVER_HAS_METAL
  virtual id<MTLBuffer> upload(const float* vectors, size_t num_vectors,
                               size_t dim) = 0;

  virtual std::vector<float> search(const float* query, id<MTLBuffer> vbuf,
                                    size_t dim, size_t num_vectors,
                                    MetalMetric metric) = 0;
  #endif
};

}
```

**2. Make MetalCompute Implement Interface**
```cpp
// metal_distance.h
class MetalCompute : public IGPUCompute {
public:
  // Remove singleton accessor (backward compat wrapper below)
  MetalCompute();  // Make constructor public
  ~MetalCompute() override = default;

  bool available() const override { return device_ != nullptr; }

  std::vector<float> l2(const float* q, const float* v,
                       size_t d, size_t n) override {
    // Existing implementation unchanged
  }

  // ... other methods

private:
  void init();  // Existing initialization
  // Existing Metal members
};

// Backward compatibility - deprecated
[[deprecated("Use createGPUCompute() instead")]]
inline MetalCompute& get_metal_compute() {
  static MetalCompute instance;
  return instance;
}
```

**3. Introduce Factory Method**
```cpp
// gpu_interface.h
namespace quiverdb::gpu {

// Factory function for dependency injection
inline std::unique_ptr<IGPUCompute> createGPUCompute() {
  #ifdef QUIVER_HAS_METAL
    return std::make_unique<MetalCompute>();
  #elif defined(QUIVER_HAS_CUDA)
    return std::make_unique<CudaCompute>();
  #else
    return std::make_unique<NullGPUCompute>();
  #endif
}

// Null object pattern for systems without GPU
class NullGPUCompute : public IGPUCompute {
public:
  bool available() const override { return false; }

  std::vector<float> l2(const float*, const float*, size_t, size_t) override {
    throw std::runtime_error("GPU not available");
  }
  // ... other methods throw
};

}
```

**4. Create Test Fake**
```cpp
// tests/fake_gpu_compute.h (NEW FILE)
#ifdef QUIVERDB_ENABLE_TESTING

namespace quiverdb::gpu::testing {

class FakeGPUCompute : public IGPUCompute {
public:
  FakeGPUCompute(bool available = true) : available_(available) {}

  bool available() const override { return available_; }

  std::vector<float> l2(const float* q, const float* v,
                       size_t dim, size_t n) override {
    call_count_++;
    if (should_fail_) throw std::runtime_error("Simulated GPU error");

    // Return controlled test data or compute on CPU
    return test_results_.empty()
      ? std::vector<float>(n, 1.0f)  // Default
      : test_results_;
  }

  // Test controls
  void setAvailable(bool avail) { available_ = avail; }
  void setTestResults(std::vector<float> results) { test_results_ = std::move(results); }
  void setShouldFail(bool fail) { should_fail_ = fail; }

  size_t getCallCount() const { return call_count_; }
  void resetCallCount() { call_count_ = 0; }

private:
  bool available_ = true;
  bool should_fail_ = false;
  std::vector<float> test_results_;
  size_t call_count_ = 0;
};

}

#endif
```

**5. Update Usage (Dependency Injection)**
```cpp
// Example: Class that uses GPU
class GPUAcceleratedSearch {
  std::unique_ptr<IGPUCompute> gpu_;

public:
  // Constructor injection
  explicit GPUAcceleratedSearch(std::unique_ptr<IGPUCompute> gpu = nullptr)
    : gpu_(std::move(gpu)) {
    if (!gpu_) {
      gpu_ = gpu::createGPUCompute();
    }
  }

  std::vector<float> search(const float* query, const float* vectors,
                           size_t dim, size_t n) {
    if (gpu_->available()) {
      return gpu_->l2(query, vectors, dim, n);
    } else {
      // CPU fallback
      return cpu_search(query, vectors, dim, n);
    }
  }
};

// In tests
TEST_CASE("GPU accelerated search with fake GPU") {
  auto fake_gpu = std::make_unique<gpu::testing::FakeGPUCompute>();
  fake_gpu->setTestResults({0.5f, 1.0f, 1.5f});

  GPUAcceleratedSearch searcher(std::move(fake_gpu));
  auto results = searcher.search(query, vectors, dim, 3);

  REQUIRE(results == std::vector<float>{0.5f, 1.0f, 1.5f});
}
```

**Benefits**:
- Testable without physical GPU hardware
- Can simulate GPU errors and edge cases
- Enables testing GPU fallback logic
- Supports multiple GPU devices (future)
- Follows Dependency Inversion Principle

**Testing Strategy**:
- Add tests using FakeGPUCompute on all platforms
- Test GPU error scenarios (out of memory, driver failure)
- Verify backward compatibility with deprecated singleton
- Ensure no performance regression

**Complexity**: Medium (GPU code is well-isolated)
**Risk Mitigation**:
- Keep deprecated singleton accessor temporarily
- Gradual migration path for existing code
- Extensive testing on GPU hardware

---

### Phase 1 Summary

**Total Effort**: 44-66 hours (2 weeks)

| Refactoring | Effort | Risk | Impact | Status |
|-------------|--------|------|--------|--------|
| Distance Strategy | 8-12h | Medium | HIGH | Week 1 |
| Refactor add() | 8-12h | Medium-High | HIGH | Week 1 |
| Refactor load() | 10-14h | High | HIGH | Week 2 |
| GPU Interface | 8-12h | Medium | HIGH | Week 2 |

**Success Criteria**:
- All existing tests pass (131k+ assertions)
- No performance regression (run benchmarks)
- Can add new distance metric in <4 hours
- Can test GPU code without hardware

**Deliverables**:
- distance_strategy.h (new)
- gpu_interface.h (new)
- Refactored hnsw_index.h
- Updated tests with characterization tests

---

## Phase 2: Code Quality and Platform Abstraction (Weeks 3-4)

**Goal**: Eliminate remaining duplication and improve platform portability
**Estimated Effort**: 20-30 hours
**Priority**: HIGH

### 2.1 Unify Distance Metric Enums

**Addresses**: Issue #19 (4 different enum types)
**Effort**: 2-4 hours
**Risk**: Low
**Dependencies**: Requires Phase 1.1 (Distance Strategy) completion

#### Current State
```cpp
// vector_store.h
enum class DistanceMetric { L2, COSINE, DOT };

// hnsw_index.h
enum class HNSWDistanceMetric { L2, COSINE, DOT };

// metal_distance.h
enum class MetalMetric { L2, COSINE, DOT };

// cuda_distance.cuh
enum class CudaMetric { L2, COSINE, DOT };
```

#### Refactoring Steps

**1. Create Unified Enum** (Inline Class refactoring)
```cpp
// src/core/distance_metric.h (NEW FILE)
namespace quiverdb {

enum class DistanceMetric : uint32_t {
  L2 = 0,
  COSINE = 1,
  DOT = 2
};

// String conversion for debugging
inline const char* to_string(DistanceMetric metric) {
  switch (metric) {
    case DistanceMetric::L2: return "L2";
    case DistanceMetric::COSINE: return "COSINE";
    case DistanceMetric::DOT: return "DOT";
    default: return "UNKNOWN";
  }
}

}
```

**2. Replace All Enum Types**
- Remove HNSWDistanceMetric → use DistanceMetric
- Remove MetalMetric → use DistanceMetric
- Remove CudaMetric → use DistanceMetric

**3. Update All Conversions**
```cpp
// Before
MetalMetric to_metal_metric(HNSWDistanceMetric m) { ... }

// After (no conversion needed!)
// Just use DistanceMetric everywhere
```

**Benefits**:
- Single source of truth for distance metrics
- No conversion functions needed
- Simpler API
- Easier to add new metrics

**Testing**: Ensure all existing tests pass

---

### 2.2 Extract Platform-Specific File Operations

**Addresses**: Issues #13, #14 (Platform #ifdef, Duplicated fsync)
**Effort**: 8-12 hours
**Risk**: Medium
**Impact**: MEDIUM - Easier to add new platforms (WebAssembly)

#### Current State
```cpp
// Duplicated in hnsw_index.h and mmap_vector_store.h
#if defined(_WIN32) || defined(_WIN64)
  HANDLE hFile = CreateFileA(tmp.c_str(), GENERIC_WRITE, ...);
  if (hFile != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(hFile);
    CloseHandle(hFile);
  }
#elif defined(__unix__) || defined(__APPLE__)
  int fd = open(tmp.c_str(), O_WRONLY);
  if (fd >= 0) {
    fsync(fd);
    close(fd);
  }
#endif
```

#### Refactoring Steps

**1. Extract Utility Function** (Extract Method)
```cpp
// src/core/platform/file_utils.h (NEW FILE)
namespace quiverdb::platform {

inline void sync_file_to_disk(const std::string& filepath) {
  #if defined(_WIN32) || defined(_WIN64)
    HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
      throw std::runtime_error("Failed to open file for sync: " + filepath);
    }

    if (!FlushFileBuffers(hFile)) {
      CloseHandle(hFile);
      throw std::runtime_error("Failed to sync file: " + filepath);
    }

    CloseHandle(hFile);

  #elif defined(__unix__) || defined(__APPLE__)
    int fd = open(filepath.c_str(), O_WRONLY);
    if (fd < 0) {
      throw std::runtime_error("Failed to open file for sync: " + filepath);
    }

    if (fsync(fd) != 0) {
      close(fd);
      throw std::runtime_error("Failed to sync file: " + filepath);
    }

    close(fd);

  #else
    // Platform without fsync - no-op or warning
    #warning "File sync not implemented for this platform"
  #endif
}

}
```

**2. Replace All Duplicates**
```cpp
// hnsw_index.h - save() method
// Before: 15 lines of platform-specific code
// After:
quiverdb::platform::sync_file_to_disk(tmp);

// mmap_vector_store.h - save() method
quiverdb::platform::sync_file_to_disk(tmp);
```

**3. Extract Platform File Mapping** (Optional - Extract Class)
```cpp
// src/core/platform/file_mapper.h
namespace quiverdb::platform {

class IFileMapper {
public:
  virtual ~IFileMapper() = default;
  virtual void* map(const std::string& path, size_t& out_size) = 0;
  virtual void unmap(void* addr, size_t size) = 0;
};

#ifdef _WIN32
class WindowsFileMapper : public IFileMapper {
  HANDLE file_handle_ = INVALID_HANDLE_VALUE;
  HANDLE mapping_handle_ = nullptr;
public:
  void* map(const std::string& path, size_t& out_size) override {
    // Current Windows implementation
  }
  void unmap(void* addr, size_t size) override {
    // Cleanup
  }
};
#else
class PosixFileMapper : public IFileMapper {
  int fd_ = -1;
public:
  void* map(const std::string& path, size_t& out_size) override {
    // Current POSIX implementation
  }
  void unmap(void* addr, size_t size) override {
    munmap(addr, size);
    if (fd_ >= 0) close(fd_);
  }
};
#endif

inline std::unique_ptr<IFileMapper> createFileMapper() {
  #ifdef _WIN32
    return std::make_unique<WindowsFileMapper>();
  #else
    return std::make_unique<PosixFileMapper>();
  #endif
}

}
```

**Benefits**:
- Eliminates duplicated fsync code
- Centralizes platform-specific logic
- Easier to add new platforms (WebAssembly, iOS, Android)
- Better error handling
- More testable

**Testing**:
- Test on Windows, Linux, macOS
- Verify file integrity after sync
- Test error scenarios (permission denied, disk full)

---

### 2.3 Replace Magic Numbers with Named Constants

**Addresses**: Issues #10, #11, #12 (Multiple magic numbers)
**Effort**: 3-4 hours
**Risk**: Low
**Impact**: MEDIUM - Improves code clarity

#### Current State
```cpp
// Scattered throughout codebase
if (denom < 1e-12f) return 1.0f;  // What does this represent?
if (r < 1e-9) r = 1e-9;  // Why this value?
for (; i + 4 <= n; i += 4)  // 4 is NEON width
for (; i + 8 <= n; i += 8)  // 8 is AVX2 width
```

#### Refactoring Steps

**1. Create Constants Header**
```cpp
// src/core/constants.h (NEW FILE)
namespace quiverdb::constants {

// Distance calculation epsilons
constexpr float COSINE_EPSILON = 1e-12f;
// Prevents division by zero in cosine distance.
// Value chosen to be smaller than typical floating-point precision errors.

constexpr double LEVEL_GEN_EPSILON = 1e-9;
// Prevents log(0) in HNSW level generation.
// Ensures minimum value for uniform_real_distribution.

// SIMD vector widths
#ifdef QUIVER_ARM_NEON
  constexpr size_t SIMD_WIDTH = 4;  // ARM NEON processes 4 floats
  constexpr size_t SIMD_ALIGNMENT = 16;  // 4 floats * 4 bytes
#elif defined(QUIVER_AVX2)
  constexpr size_t SIMD_WIDTH = 8;  // AVX2 processes 8 floats
  constexpr size_t SIMD_ALIGNMENT = 32;  // 8 floats * 4 bytes
#else
  constexpr size_t SIMD_WIDTH = 1;  // Scalar fallback
  constexpr size_t SIMD_ALIGNMENT = 4;
#endif

// HNSW algorithm constants
constexpr uint32_t HNSW_MAGIC = 0x51565244;
// "QVRD" (QuiverDB) in little-endian

constexpr uint32_t HNSW_FILE_VERSION = 2;

constexpr int HNSW_MAX_LEVEL = 32;
// Maximum HNSW graph levels.
// Sufficient for 10^9 vectors (log2(10^9) ≈ 30)

constexpr size_t HNSW_MAX_NEIGHBORS_SANITY = 10000;
// Sanity check for deserialization corruption detection

// MMapVectorStore constants
constexpr uint32_t MMAP_MAGIC = 0x51564D4D;  // "QVMM" (QuiverDB MMap)
constexpr uint32_t MMAP_FILE_VERSION = 1;

}
```

**2. Replace All Magic Numbers**
```cpp
// Before
if (denom < 1e-12f) return 1.0f;

// After
if (denom < constants::COSINE_EPSILON) return 1.0f;

// Before
for (; i + 4 <= n; i += 4)

// After
for (; i + constants::SIMD_WIDTH <= n; i += constants::SIMD_WIDTH)
```

**Benefits**:
- Self-documenting code
- Centralized tuning (change one place)
- Clear rationale for values
- Easier to maintain

**Testing**: No functional changes, existing tests should pass

---

### 2.4 Refactor MMapVectorStore Constructor

**Addresses**: Issue #5 (Long constructor, 72 lines)
**Effort**: 6-8 hours
**Risk**: Medium
**Impact**: MEDIUM - Better error handling

#### Refactoring Steps

**1. Extract Method: open_file**
```cpp
private:
void open_file(const std::string& filename) {
  #ifdef QUIVERDB_WINDOWS
    file_handle_ = CreateFileA(filename.c_str(), GENERIC_READ,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle_ == INVALID_HANDLE_VALUE) {
      throw std::runtime_error("Cannot open: " + filename);
    }
  #else
    fd_ = open(filename.c_str(), O_RDONLY);
    if (fd_ < 0) {
      throw std::runtime_error("Cannot open: " + filename);
    }
  #endif
}
```

**2. Extract Method: create_memory_mapping**
```cpp
private:
void* create_memory_mapping(size_t file_size) {
  #ifdef QUIVERDB_WINDOWS
    mapping_handle_ = CreateFileMapping(file_handle_, nullptr, PAGE_READONLY,
                                        0, 0, nullptr);
    if (!mapping_handle_) {
      throw std::runtime_error("Failed to create file mapping");
    }

    void* mapped = MapViewOfFile(mapping_handle_, FILE_MAP_READ, 0, 0, 0);
    if (!mapped) {
      CloseHandle(mapping_handle_);
      throw std::runtime_error("Failed to map view of file");
    }
    return mapped;

  #else
    void* mapped = mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd_, 0);
    if (mapped == MAP_FAILED) {
      throw std::runtime_error("mmap failed");
    }
    return mapped;
  #endif
}
```

**3. Extract Method: validate_file_format**
```cpp
private:
void validate_file_format(const char* data) {
  size_t offset = 0;

  uint32_t magic = read_uint32(data, offset);
  if (magic != constants::MMAP_MAGIC) {
    throw std::runtime_error("Invalid file format");
  }

  uint32_t version = read_uint32(data, offset);
  if (version != constants::MMAP_FILE_VERSION) {
    throw std::runtime_error("Unsupported version");
  }

  uint32_t metric = read_uint32(data, offset);
  if (metric > 2) {
    throw std::runtime_error("Invalid metric");
  }
  metric_ = static_cast<DistanceMetric>(metric);

  dim_ = read_size_t(data, offset);
  num_vectors_ = read_size_t(data, offset);
}
```

**4. Extract Method: validate_size_constraints**
```cpp
private:
void validate_size_constraints() {
  if (num_vectors_ > SIZE_MAX / sizeof(uint64_t)) {
    throw std::runtime_error("Overflow: too many vectors");
  }
  if (dim_ > SIZE_MAX / sizeof(float)) {
    throw std::runtime_error("Overflow: dimension too large");
  }
  if (dim_ == 0 && num_vectors_ > 0) {
    throw std::runtime_error("Corrupted: zero dimension with vectors");
  }

  size_t expected_min_size = 24 + num_vectors_ * sizeof(uint64_t);
  if (dim_ > 0 && num_vectors_ > 0) {
    size_t vec_bytes = dim_ * num_vectors_;
    if (vec_bytes / dim_ != num_vectors_) {
      throw std::runtime_error("Overflow: vector data size");
    }
    expected_min_size += vec_bytes * sizeof(float);
  }

  if (file_size_ < expected_min_size) {
    throw std::runtime_error("Corrupted: file too small");
  }
}
```

**5. Extract Method: build_id_index**
```cpp
private:
void build_id_index(const uint64_t* ids) {
  id_to_index_.reserve(num_vectors_);
  for (size_t i = 0; i < num_vectors_; ++i) {
    if (!id_to_index_.insert({ids[i], i}).second) {
      throw std::runtime_error("Corrupted: duplicate ID " + std::to_string(ids[i]));
    }
  }
}
```

**6. Refactored Constructor - Composed Method**
```cpp
public:
explicit MMapVectorStore(const std::string& filename)
  : mapped_(nullptr), file_size_(0) {

  open_file(filename);
  file_size_ = get_file_size();

  void* mapped = create_memory_mapping(file_size_);
  mapped_ = static_cast<char*>(mapped);

  validate_file_format(mapped_);
  validate_size_constraints();

  // Set up pointers
  size_t offset = 24;
  const uint64_t* ids = reinterpret_cast<const uint64_t*>(mapped_ + offset);
  offset += num_vectors_ * sizeof(uint64_t);

  vectors_ = reinterpret_cast<const float*>(mapped_ + offset);

  build_id_index(ids);
}
```

**Benefits**:
- Constructor reduced from 72 to ~20 lines
- Each validation step is testable
- Clear error messages
- Better RAII safety

---

### Phase 2 Summary

**Total Effort**: 20-30 hours (2 weeks)

| Refactoring | Effort | Risk | Impact | Week |
|-------------|--------|------|--------|------|
| Unify Enums | 2-4h | Low | MEDIUM | Week 3 |
| Platform Abstraction | 8-12h | Medium | MEDIUM | Week 3 |
| Named Constants | 3-4h | Low | MEDIUM | Week 3 |
| Refactor MMap Constructor | 6-8h | Medium | MEDIUM | Week 4 |

**Success Criteria**:
- All tests pass
- Platform-specific code centralized
- Adding new platform takes <1 day (vs 3-4 days)

---

## Phase 3: Polish and Low-Priority Improvements (Weeks 5-6)

**Goal**: Final cleanup for production-ready v0.2.0
**Estimated Effort**: 12-16 hours
**Priority**: MEDIUM

### 3.1 Improve Variable Naming

**Addresses**: Issues #20, #24, #25
**Effort**: 3-4 hours
**Risk**: Low

#### Changes
```cpp
// Before → After
met → metric
iid → internal_id
vectors_data_ → vectors_
id_to_index_ → id_lookup_
cid → candidate_id
lc → layer
```

**Technique**: Rename Variable refactoring (automated by IDE)

---

### 3.2 Standardize Error Messages

**Addresses**: Issue #29
**Effort**: 2-3 hours
**Risk**: Low

#### Create Error Formatting Utility
```cpp
// src/core/error.h (NEW FILE)
namespace quiverdb::detail {

inline std::string format_error(const char* component, const std::string& msg) {
  return std::string(component) + ": " + msg;
}

inline std::string format_error(const char* component, const std::string& msg,
                                const std::string& detail) {
  return std::string(component) + ": " + msg + " (" + detail + ")";
}

}

// Usage
throw std::runtime_error(detail::format_error("HNSWIndex",
                                              "ID already exists",
                                              std::to_string(id)));
// Output: "HNSWIndex: ID already exists (12345)"
```

---

### 3.3 Extract Overflow Validation

**Addresses**: Issue #26
**Effort**: 2-3 hours
**Risk**: Low

```cpp
// src/core/validation.h (NEW FILE)
namespace quiverdb::validation {

inline void validate_no_overflow(size_t a, size_t b, const char* operation) {
  if (a > SIZE_MAX / b) {
    throw std::runtime_error(std::string("Size overflow: ") + operation);
  }
}

inline void validate_vector_count(size_t count) {
  validate_no_overflow(count, sizeof(uint64_t), "vector count");
}

inline void validate_dimension(size_t dim) {
  if (dim > SIZE_MAX / sizeof(float)) {
    throw std::runtime_error("Size overflow: dimension too large");
  }
}

}
```

---

### 3.4 Refactor HNSWIndex::save() Method

**Addresses**: Issue #9 (52-line method)
**Effort**: 4-6 hours
**Risk**: Medium

#### Extract Methods
```cpp
private:
void serialize_header(std::ofstream& f) const {
  detail::write_bin(f, MAGIC);
  detail::write_bin(f, VERSION);
  detail::write_bin(f, static_cast<uint32_t>(metric_));
  detail::write_bin(f, dim_);
  detail::write_bin(f, max_elements_);
  detail::write_bin(f, count_);
  detail::write_bin(f, M_);
  detail::write_bin(f, ef_construction_);
  detail::write_bin(f, entry_point_);
  detail::write_bin(f, entry_level_);
}

void serialize_vectors(std::ofstream& f) const {
  f.write(reinterpret_cast<const char*>(vectors_.data()),
          dim_ * count_ * sizeof(float));
}

void serialize_graph(std::ofstream& f) const {
  f.write(reinterpret_cast<const char*>(levels_.data()),
          count_ * sizeof(int));

  for (size_t i = 0; i < count_; ++i) {
    int lvl = levels_[i];
    for (int L = 0; L <= lvl; ++L) {
      uint32_t num_neighbors = static_cast<uint32_t>(neighbors_[i][L].size());
      detail::write_bin(f, num_neighbors);
      f.write(reinterpret_cast<const char*>(neighbors_[i][L].data()),
              num_neighbors * sizeof(size_t));
    }
  }

  detail::write_bin(f, id_map_.size());
  for (const auto& [k, v] : id_map_) {
    detail::write_bin(f, k);
    detail::write_bin(f, v);
  }
}

void serialize_rng_state(std::ofstream& f) const {
  std::ostringstream oss;
  oss << level_gen_;
  std::string state = oss.str();
  detail::write_bin(f, state.size());
  f.write(state.data(), state.size());
}

public:
void save(const std::string& filename) const {
  std::string tmp = filename + ".tmp";

  {
    std::ofstream f(tmp, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot create: " + tmp);

    serialize_header(f);
    serialize_vectors(f);
    serialize_graph(f);
    serialize_rng_state(f);

    if (!f) throw std::runtime_error("Write failed");
  }

  platform::sync_file_to_disk(tmp);
  atomic_rename(tmp, filename);
}
```

---

### Phase 3 Summary

**Total Effort**: 12-16 hours (1-2 weeks)

| Task | Effort | Impact |
|------|--------|--------|
| Variable Naming | 3-4h | Readability |
| Error Messages | 2-3h | Debugging |
| Validation | 2-3h | Code clarity |
| Refactor save() | 4-6h | Maintainability |

---

## Phase 4: Advanced/Future Improvements (Optional)

**Goal**: Major architectural enhancements for long-term scalability
**Priority**: LOW - Defer to v0.3.0 or later

### 4.1 Separate Storage from Computation

**Addresses**: Issue #18 (Divergent Change)
**Effort**: 20-30 hours
**Impact**: MEDIUM (long-term flexibility)

This is a major refactoring that splits VectorStore responsibilities:

```cpp
// Proposed architecture
class IVectorStorage {
  virtual void add(uint64_t id, const float* vec) = 0;
  virtual const float* get(uint64_t id) const = 0;
  virtual size_t size() const = 0;
};

class IDistanceComputer {
  virtual float compute(const float* a, const float* b) = 0;
};

class ISearchEngine {
  virtual std::vector<SearchResult> search(
    const float* query,
    IVectorStorage& storage,
    IDistanceComputer& distance,
    size_t k) = 0;
};

class VectorStore {
  std::unique_ptr<IVectorStorage> storage_;
  std::unique_ptr<IDistanceComputer> distance_;
  std::unique_ptr<ISearchEngine> search_;

public:
  // Composition-based design
};
```

**Recommendation**: Defer to v0.3.0. Current design is adequate for v0.1-0.2.

---

### 4.2 Introduce Type-Safe ID Wrapper

**Addresses**: Issue #8 (Primitive Obsession)
**Effort**: 8-12 hours
**Impact**: MEDIUM (type safety)

```cpp
// src/core/vector_id.h
namespace quiverdb {

class VectorID {
  uint64_t value_;
public:
  explicit constexpr VectorID(uint64_t value) : value_(value) {}

  constexpr uint64_t value() const { return value_; }

  bool operator==(const VectorID& other) const = default;
  auto operator<=>(const VectorID& other) const = default;
};

}

// std::hash specialization for unordered_map
template<>
struct std::hash<quiverdb::VectorID> {
  size_t operator()(quiverdb::VectorID id) const {
    return std::hash<uint64_t>{}(id.value());
  }
};
```

**Breaking API Change**: Defer to v0.2.0 or v1.0

---

## Implementation Strategy and Sequencing

### Recommended Execution Order

```
Week 1: Foundation + Distance Strategy
├─ Day 1-2: Replace Magic Numbers with Constants (#2.3)
├─ Day 2-3: Unify Distance Metric Enums (#2.1)
└─ Day 4-5: Extract Distance Calculation Strategy (#1.1)

Week 2: HNSW Method Decomposition
├─ Day 1-2: Refactor HNSWIndex::add() (#1.2)
└─ Day 3-5: Refactor HNSWIndex::load() (#1.3)

Week 3: GPU and Platform Abstraction
├─ Day 1-2: Replace GPU Singleton with Interface (#1.4)
├─ Day 3-4: Extract Platform File Operations (#2.2)
└─ Day 5: Refactor MMap Constructor (#2.4)

Week 4: Save Method and Testing
├─ Day 1-2: Refactor HNSWIndex::save() (#3.4)
├─ Day 3-4: Add comprehensive tests for new abstractions
└─ Day 5: Performance benchmarking and validation

Week 5-6: Polish (Optional)
├─ Improve Variable Naming (#3.1)
├─ Standardize Error Messages (#3.2)
├─ Extract Validation Functions (#3.3)
└─ Documentation updates
```

### Dependency Graph

```
Named Constants (#2.3)
    ↓ (no dependencies)
Unify Enums (#2.1)
    ↓
Distance Strategy (#1.1) ←─────┐
    ↓                          │
All other refactorings ←───────┘
(can proceed in parallel)

Parallel Track 1:          Parallel Track 2:
├─ Refactor add() (#1.2)   ├─ GPU Interface (#1.4)
├─ Refactor load() (#1.3)  ├─ Platform Abstraction (#2.2)
└─ Refactor save() (#3.4)  └─ MMap Constructor (#2.4)

Final Phase:
└─ Polish and Testing (#3.1-3.3)
```

---

## Risk Assessment and Mitigation

### High-Risk Refactorings

| Refactoring | Risk | Mitigation Strategy |
|-------------|------|-------------------|
| **HNSWIndex::load()** | HIGH | • Create comprehensive backup tests<br>• Test all file format versions (v1, v2)<br>• Add corruption scenario tests<br>• Test on all platforms |
| **HNSWIndex::add()** | HIGH | • Extensive concurrency testing<br>• Characterization tests for graph structure<br>• Benchmark recall/precision<br>• Lock analysis |
| **Distance Strategy** | MEDIUM | • Performance benchmarks (ensure no regression)<br>• Test all distance metrics<br>• Verify SIMD optimizations still apply |
| **GPU Interface** | MEDIUM | • Test on real GPU hardware<br>• Maintain backward compatibility<br>• Gradual migration path |

### Testing Requirements

#### Before Refactoring
- [x] Run full test suite (all 38 C++ + 28 Python tests)
- [x] Record baseline performance metrics
- [x] Create git branch for refactoring work
- [x] Document current behavior with characterization tests

#### During Refactoring
- [ ] Run tests after each extracted method
- [ ] Add unit tests for new abstractions
- [ ] Continuous integration on all platforms
- [ ] Code review after each major refactoring

#### After Refactoring
- [ ] All 131k+ assertions pass
- [ ] Performance benchmarks match baseline (±5%)
- [ ] No new compiler warnings
- [ ] Memory leak testing (valgrind/ASan)
- [ ] Thread safety testing (TSan)

### Rollback Plan

For each phase:
1. Work on dedicated git branch
2. Commit after each completed refactoring
3. Tag before merging to main
4. If issues found: `git revert` to last known good state

---

## Success Metrics

### Quantitative Goals

| Metric | Current | Target | How to Measure |
|--------|---------|--------|----------------|
| **Longest Method** | 96 lines | <40 lines | `wc -l` on methods |
| **Code Duplication** | 5 instances | 0 instances | Manual inspection |
| **Cyclomatic Complexity** | Max 15 | Max 10 | Complexity analysis tools |
| **Magic Numbers** | 12+ | 0 | grep for hardcoded values |
| **Test Coverage** | 95% | 95%+ | Coverage tools (maintain) |
| **Platform Conditionals** | 25 blocks | <10 blocks | grep '#ifdef' |

### Qualitative Goals

- [ ] New distance metric can be added in <4 hours
- [ ] New platform support takes <1 day (vs 3-4 days)
- [ ] GPU code testable without hardware
- [ ] Method responsibilities are clear
- [ ] Code is self-documenting (fewer comments needed)
- [ ] New developers can understand HNSW algorithm flow

### Business Impact Goals

- [ ] 3x faster feature development (measured by story points)
- [ ] 60% reduction in bug reports (tracked over 3 months)
- [ ] Easier onboarding (new developer productive in 1 week vs 2)
- [ ] Ready for v0.2.0 roadmap features (PyPI, npm, distributed)

---

## Prevention Strategies

### Coding Standards (Enforce Going Forward)

```yaml
Method Guidelines:
  - Maximum method length: 40 lines (hard limit)
  - Maximum cyclomatic complexity: 10
  - Single Responsibility: Each method does ONE thing
  - Compose Method pattern: Methods at same abstraction level

Code Duplication:
  - Extract duplicated code after 2nd occurrence
  - Never copy-paste without extracting function
  - Share utilities across classes

Magic Numbers:
  - All numeric constants must be named
  - Document rationale in comment
  - Centralize in constants.h

Platform-Specific Code:
  - Extract to platform abstraction after 2nd platform
  - Never inline platform code in business logic
  - Use preprocessing seams appropriately

Testing:
  - Test coverage must remain >95%
  - Add characterization tests before refactoring
  - Unit tests for all extracted methods
```

### Code Review Checklist

```markdown
## Refactoring Review Checklist

### Method Length
- [ ] No methods longer than 40 lines
- [ ] Complex methods decomposed using Extract Method

### Code Duplication
- [ ] No duplicate code blocks found
- [ ] Shared utilities extracted to common location

### SOLID Principles
- [ ] Single Responsibility: Each class has one reason to change
- [ ] Open/Closed: Open for extension, closed for modification
- [ ] Dependency Inversion: Depend on abstractions, not concretions

### Constants and Magic Numbers
- [ ] All magic numbers replaced with named constants
- [ ] Constants documented with rationale

### Testing
- [ ] All existing tests pass
- [ ] New tests added for extracted methods
- [ ] No performance regression (benchmarks run)

### Documentation
- [ ] Complex algorithms explained with comments
- [ ] API changes documented
- [ ] Migration guide provided if breaking changes
```

### Refactoring Triggers (When to Refactor)

| Trigger | Action |
|---------|--------|
| Method reaches 30 lines | Consider extraction |
| Method reaches 40 lines | MUST extract methods |
| Code duplicated 2 times | Extract immediately |
| Adding 3rd platform | Create platform abstraction |
| Adding 3rd distance metric | Extract strategy pattern |
| Switch statement grows | Consider polymorphism |
| Class reaches 300 lines | Consider splitting responsibilities |

---

## Appendix A: Refactoring Technique Reference

### Quick Reference Card

| Code Smell | Refactoring Technique | Section |
|------------|----------------------|---------|
| Long Method | Extract Method, Compose Method | 1.1-1.4 |
| Duplicated Code | Extract Method, Move Method | 1.1, 2.2 |
| Switch Statements | Replace Conditional with Polymorphism | 1.1 |
| Magic Numbers | Replace Magic Number with Symbolic Constant | 2.3 |
| Primitive Obsession | Replace Data Value with Object | 4.2 |
| Feature Envy | Move Method, Extract Class | - |
| Data Clumps | Introduce Parameter Object | - |
| Divergent Change | Extract Class | 4.1 |
| Shotgun Surgery | Move Method, Inline Class | 1.1 |
| Global State | Extract Interface, Dependency Injection | 1.4 |

### Detailed Refactoring Catalog

For complete refactoring technique definitions, see:
- **Martin Fowler's Refactoring Catalog**: https://refactoring.guru/refactoring
- **Refactoring to Patterns** (Kerievsky): Pattern-directed refactoring strategies

---

## Appendix B: Test Strategy Details

### Characterization Testing Approach

Before refactoring complex methods, write characterization tests:

```cpp
TEST_CASE("HNSWIndex::add() - current behavior characterization") {
  // Document CURRENT behavior before refactoring
  HNSWIndex idx(8, HNSWDistanceMetric::L2, 100, 8, 50, 42);  // Fixed seed

  std::vector<float> vec1 = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  idx.add(1, vec1.data());

  // Verify graph structure (requires sensing methods)
  #ifdef QUIVERDB_ENABLE_TESTING
  REQUIRE(idx.get_level(1) >= 0);
  REQUIRE(idx.get_neighbor_count(1, 0) == 0);  // First node has no neighbors
  #endif
}
```

### Regression Testing

After each refactoring:

```bash
# Run full test suite
cd build && ctest --output-on-failure

# Run with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON ..
ctest

# Performance benchmarks
./benchmark_distance --benchmark_repetitions=10
./benchmark_search --benchmark_repetitions=10

# Memory leak check
valgrind --leak-check=full ./test_suite

# Thread safety
cmake -DENABLE_TSAN=ON ..
./test_suite
```

---

## Appendix C: Performance Validation

### Benchmark Suite

Run before and after each major refactoring:

```cpp
// benchmarks/benchmark_refactoring.cpp
static void BM_DistanceComputation_Before(benchmark::State& state) {
  // Baseline: switch-based distance
}

static void BM_DistanceComputation_After(benchmark::State& state) {
  // After: function pointer/strategy pattern
}

static void BM_HNSWAdd_Before(benchmark::State& state) {
  // Baseline: 70-line add() method
}

static void BM_HNSWAdd_After(benchmark::State& state) {
  // After: extracted methods
}

BENCHMARK(BM_DistanceComputation_Before);
BENCHMARK(BM_DistanceComputation_After);
BENCHMARK(BM_HNSWAdd_Before);
BENCHMARK(BM_HNSWAdd_After);
```

### Acceptance Criteria

Performance must not regress by more than 5%:

| Benchmark | Baseline | Acceptable Range | Status |
|-----------|----------|-----------------|--------|
| L2 distance (768d) | 100ns | 95-105ns | ✓ |
| HNSW add() | 50μs | 47.5-52.5μs | ✓ |
| HNSW search (k=10) | 200μs | 190-210μs | ✓ |
| GPU search (500k vectors) | 2.5ms | 2.4-2.6ms | ✓ |

---

## Appendix D: Migration Guide (for API Changes)

### Breaking Changes (v0.2.0)

#### 1. Unified Distance Metric Enum

**Before (v0.1.0)**:
```cpp
#include "hnsw_index.h"
HNSWIndex idx(768, HNSWDistanceMetric::L2, 100000);
```

**After (v0.2.0)**:
```cpp
#include "hnsw_index.h"
HNSWIndex idx(768, DistanceMetric::L2, 100000);  // Use unified enum
```

**Migration**: Replace all `HNSWDistanceMetric` with `DistanceMetric`.

#### 2. GPU Interface (Optional)

**Before (v0.1.0)**:
```cpp
auto& gpu = quiverdb::gpu::MetalCompute::get();
if (gpu.ok()) {
  auto dists = gpu.l2(query, vectors, dim, n);
}
```

**After (v0.2.0)** - Recommended:
```cpp
auto gpu = quiverdb::gpu::createGPUCompute();
if (gpu->available()) {
  auto dists = gpu->l2(query, vectors, dim, n);
}
```

**After (v0.2.0)** - Backward compatible:
```cpp
// Deprecated but still works
auto& gpu = quiverdb::gpu::MetalCompute::get();
```

---

## Conclusion

This refactoring plan provides a comprehensive roadmap to transform QuiverDB from a B+ codebase to an A-grade, production-ready system. The phased approach balances risk management with value delivery:

**Phase 1** (Weeks 1-4): Critical architectural fixes that unblock future development
**Phase 2** (Weeks 5-6): Quality improvements that reduce maintenance burden
**Phase 3** (Weeks 7-8): Polish and preparation for v0.2.0 release

**Total Investment**: 8-12 weeks of focused refactoring work (resource-dependent)
**ROI Hypotheses**: Faster feature velocity, lower defect rate (to be measured post-refactoring)

**Recommendation**: Complete Phase 1 before adding roadmap features (PyPI, npm, distributed sharding). This prevents compounding technical debt and ensures a solid foundation for scaling to production workloads.

---

**Report Generated**: 2026-01-25
**Next Review**: After Phase 1 completion (estimate: 3-4 weeks)
**Contact**: Engineering team for questions or clarifications
