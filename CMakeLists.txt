set(CMAKE_POLICY_VERSION_MINIMUM "3.5.0" CACHE STRING "" FORCE)
cmake_minimum_required(VERSION 3.20)
project(JITSlipperyRadixBenchmark CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use -O3 for Release build
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

include(FetchContent)

# Google Benchmark
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.3
)
# Disable benchmark tests to speed up build
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# TBB removed due to GCC ICE, using OpenMP for parallel STL via _GLIBCXX_PARALLEL

# OpenMP
find_package(OpenMP REQUIRED)

add_executable(benchmark_runner benchmark.cpp)

# Link everything together
target_link_libraries(benchmark_runner PRIVATE 
    benchmark::benchmark
    OpenMP::OpenMP_CXX
)

# For parallel STL
target_compile_definitions(benchmark_runner PRIVATE _GLIBCXX_PARALLEL)

# --- V5 Runner ---
add_executable(benchmark_runner_v5 V5_ThreadLocal_Pipeline/benchmark_v5.cpp)
target_link_libraries(benchmark_runner_v5 PRIVATE 
    benchmark::benchmark
    OpenMP::OpenMP_CXX
)
target_compile_definitions(benchmark_runner_v5 PRIVATE _GLIBCXX_PARALLEL)
# --- V5 Locked Runner ---
add_executable(benchmark_runner_v5_locked V5_ThreadLocal_Pipeline/benchmark_v5_locked.cpp)
target_link_libraries(benchmark_runner_v5_locked PRIVATE 
    benchmark::benchmark
    OpenMP::OpenMP_CXX
)
target_compile_definitions(benchmark_runner_v5_locked PRIVATE _GLIBCXX_PARALLEL)

# --- V5 Tiered Runner ---
add_executable(benchmark_runner_v5_tiered V5_ThreadLocal_Pipeline/benchmark_v5_tiered.cpp)
target_link_libraries(benchmark_runner_v5_tiered benchmark::benchmark benchmark::benchmark_main OpenMP::OpenMP_CXX)

add_executable(benchmark_runner_v6_avx2 V6_AVX2_Pipeline/benchmark_v6_avx2.cpp)
target_compile_options(benchmark_runner_v6_avx2 PRIVATE -mavx2)
target_link_libraries(benchmark_runner_v6_avx2 benchmark::benchmark benchmark::benchmark_main OpenMP::OpenMP_CXX)

add_executable(benchmark_runner_v7_waterfall V7_Waterfall_Pipeline/benchmark_v7.cpp)
target_compile_options(benchmark_runner_v7_waterfall PRIVATE -mavx2)
target_link_libraries(benchmark_runner_v7_waterfall benchmark::benchmark benchmark::benchmark_main OpenMP::OpenMP_CXX)

add_executable(benchmark_world_champions World_Champions_Pipeline/benchmark_world_champions.cpp)
target_compile_options(benchmark_world_champions PRIVATE -march=native -O3)
target_link_libraries(benchmark_world_champions benchmark::benchmark benchmark::benchmark_main OpenMP::OpenMP_CXX)

add_executable(benchmark_runner_v6_multipass V6_AVX2_MultiPass_Pipeline/benchmark_v6_multipass.cpp)
target_compile_options(benchmark_runner_v6_multipass PRIVATE -mavx2)
target_link_libraries(benchmark_runner_v6_multipass benchmark::benchmark benchmark::benchmark_main OpenMP::OpenMP_CXX)

target_compile_definitions(benchmark_runner_v5_tiered PRIVATE _GLIBCXX_PARALLEL)