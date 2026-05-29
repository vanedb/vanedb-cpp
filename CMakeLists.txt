cmake_minimum_required(VERSION 3.20)
project(vanedb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# General warning flags for all targets and configurations
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Configuration-specific flags
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "-O3")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
  set(CMAKE_C_FLAGS_RELEASE "-O3")
  set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
endif()

# Detect x86_64 for AVX2 support
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
  set(VANEDB_X86_64 TRUE)
endif()

# Header-only library
add_library(vanedb INTERFACE)
target_include_directories(vanedb INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Options
option(VANEDB_BUILD_TESTS "Build tests" ON)
option(VANEDB_BUILD_BENCHMARKS "Build benchmarks" ON)
option(VANEDB_BUILD_EXAMPLES "Build examples" ON)
option(VANEDB_BUILD_PYTHON "Build Python bindings" ON)
option(VANEDB_COVERAGE "Build with code coverage instrumentation" OFF)

# Fetch dependencies
include(FetchContent)

# Benchmarks
if(VANEDB_BUILD_BENCHMARKS)
# Google Benchmark (only fetch if building benchmarks)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.4
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)
add_executable(bench_distance benchmarks/bench_distance.cpp)
target_link_libraries(bench_distance PRIVATE vanedb benchmark::benchmark)
target_compile_options(bench_distance PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /arch:AVX2>
  $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<BOOL:${VANEDB_X86_64}>>:-mavx2 -mfma>
)

add_executable(bench_vector_store benchmarks/bench_vector_store.cpp)
target_link_libraries(bench_vector_store PRIVATE vanedb benchmark::benchmark)
target_compile_options(bench_vector_store PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /arch:AVX2>
  $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<BOOL:${VANEDB_X86_64}>>:-mavx2 -mfma>
)

add_executable(bench_hnsw_index benchmarks/bench_hnsw_index.cpp)
target_link_libraries(bench_hnsw_index PRIVATE vanedb benchmark::benchmark)
target_compile_options(bench_hnsw_index PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /arch:AVX2>
  $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<BOOL:${VANEDB_X86_64}>>:-mavx2 -mfma>
)
endif() # VANEDB_BUILD_BENCHMARKS

# Tests - Catch2
if(VANEDB_BUILD_TESTS)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.11.0
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

# Coverage flags for test targets only (not dependencies)
if(VANEDB_COVERAGE AND NOT MSVC)
  set(COVERAGE_COMPILE_FLAGS --coverage -fprofile-arcs -ftest-coverage -fprofile-update=atomic)
  set(COVERAGE_LINK_FLAGS --coverage -fprofile-update=atomic)
endif()

add_executable(test_distance tests/test_distance.cpp)
target_link_libraries(test_distance PRIVATE vanedb Catch2::Catch2WithMain)
target_compile_options(test_distance PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  ${COVERAGE_COMPILE_FLAGS}
)
target_link_options(test_distance PRIVATE ${COVERAGE_LINK_FLAGS})

add_executable(test_vector_store tests/test_vector_store.cpp)
target_link_libraries(test_vector_store PRIVATE vanedb Catch2::Catch2WithMain)
target_compile_options(test_vector_store PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  ${COVERAGE_COMPILE_FLAGS}
)
target_link_options(test_vector_store PRIVATE ${COVERAGE_LINK_FLAGS})

add_executable(test_hnsw_index tests/test_hnsw_index.cpp)
target_link_libraries(test_hnsw_index PRIVATE vanedb Catch2::Catch2WithMain)
target_compile_options(test_hnsw_index PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /arch:AVX2>
  $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<BOOL:${VANEDB_X86_64}>>:-mavx2 -mfma>
  ${COVERAGE_COMPILE_FLAGS}
)
target_link_options(test_hnsw_index PRIVATE ${COVERAGE_LINK_FLAGS})

add_executable(test_distance_strategy tests/test_distance_strategy.cpp)
target_link_libraries(test_distance_strategy PRIVATE vanedb Catch2::Catch2WithMain)
target_compile_options(test_distance_strategy PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  ${COVERAGE_COMPILE_FLAGS}
)
target_link_options(test_distance_strategy PRIVATE ${COVERAGE_LINK_FLAGS})

add_executable(test_mmap_vector_store tests/test_mmap_vector_store.cpp)
target_link_libraries(test_mmap_vector_store PRIVATE vanedb Catch2::Catch2WithMain)
target_compile_options(test_mmap_vector_store PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  ${COVERAGE_COMPILE_FLAGS}
)
target_link_options(test_mmap_vector_store PRIVATE ${COVERAGE_LINK_FLAGS})

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(test_distance)
catch_discover_tests(test_vector_store)
catch_discover_tests(test_hnsw_index)
catch_discover_tests(test_distance_strategy)
catch_discover_tests(test_mmap_vector_store)
endif() # VANEDB_BUILD_TESTS

# Examples
if(VANEDB_BUILD_EXAMPLES)
add_executable(semantic_search_example examples/semantic_search_example.cpp)
target_link_libraries(semantic_search_example PRIVATE vanedb)
target_compile_options(semantic_search_example PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
endif() # VANEDB_BUILD_EXAMPLES

# GPU support (optional)
option(VANEDB_BUILD_METAL "Build Metal GPU tests (macOS only)" OFF)
option(VANEDB_BUILD_CUDA "Build CUDA GPU support" OFF)

# Metal GPU test (macOS only)
if(VANEDB_BUILD_METAL AND APPLE)
  enable_language(OBJCXX)
  add_executable(test_metal_distance tests/test_metal_distance.mm)
  target_link_libraries(test_metal_distance PRIVATE vanedb "-framework Metal" "-framework Foundation")
  target_compile_options(test_metal_distance PRIVATE -fobjc-arc)
  set_target_properties(test_metal_distance PROPERTIES
    MACOSX_BUNDLE FALSE
    CXX_STANDARD 20
  )
endif()

# CUDA GPU support
if(VANEDB_BUILD_CUDA)
  enable_language(CUDA)
  # EXPERIMENTAL: cuda_distance.cuh contains kernel source but is intentionally
  # not yet wired into a build target. It is unverified — there is no NVIDIA
  # hardware in CI. Metal is the supported GPU path.
endif()

# C API (shippable extern "C" surface; consumed by vanedb-bench)
option(VANEDB_BUILD_CAPI "Build the C API static library" OFF)
if(VANEDB_BUILD_CAPI)
  add_library(vanedb_cpp_capi STATIC capi/vanedb_capi.cpp)
  target_include_directories(vanedb_cpp_capi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(vanedb_cpp_capi PRIVATE cxx_std_20)

  if(VANEDB_BUILD_TESTS)
    # Enable C so CMake can compile test_capi.c (project declares LANGUAGES CXX only).
    enable_language(C)
    add_executable(test_capi tests/capi/test_capi.c)
    target_include_directories(test_capi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_capi PRIVATE vanedb_cpp_capi)
    # The static lib is C++; link the C test with the C++ runtime.
    set_target_properties(test_capi PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME capi COMMAND test_capi)
  endif()

  install(TARGETS vanedb_cpp_capi ARCHIVE DESTINATION lib)
  install(FILES capi/vanedb_capi.h DESTINATION include/vanedb)
endif()

# Install
install(TARGETS vanedb EXPORT vanedb-targets)
install(DIRECTORY src/core/ DESTINATION include/vanedb FILES_MATCHING PATTERN "*.h" PATTERN "*.cuh")
install(EXPORT vanedb-targets FILE vanedb-config.cmake DESTINATION lib/cmake/vanedb)

# Python bindings (optional)
if(VANEDB_BUILD_PYTHON)
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.12.0
  )
  FetchContent_MakeAvailable(pybind11)

  pybind11_add_module(vanedb_py MODULE python/vanedb.cpp)
  target_link_libraries(vanedb_py PRIVATE vanedb pybind11::module)
  target_compile_options(vanedb_py PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /arch:AVX2>
    $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<BOOL:${VANEDB_X86_64}>>:-mavx2 -mfma>
  )
endif() # VANEDB_BUILD_PYTHON

