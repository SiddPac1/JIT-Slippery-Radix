#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>

// Our Antigravity Algorithms (Cloud-Isolated Copies)
#include "Antigravity_Algos/JITSlipperyRadixSortV5_Locked.h"
#include "Antigravity_Algos/JITSlipperyRadixSortV5_Tiered.h"
#include "Antigravity_Algos/JITSlipperyRadixSortV5_MultiPass.h"
#include "Antigravity_Algos/JITSlipperyRadixSortV6_AVX2.h"
#include "Antigravity_Algos/JITSlipperyRadixSortV6_AVX2_Tiered.h"
#include "Antigravity_Algos/JITSlipperyRadixSortV6_AVX2_MultiPass.h"
#include "Antigravity_Algos/JITSlipperyRadixSort_V4.h"
#include "Antigravity_Algos/BSPRadixSort.h"

// The World Champions (Requires running download_champions.ps1 / .sh first)
#if __has_include("ips4o/include/ips4o.hpp")
#include "ips4o/include/ips4o.hpp"
#define HAS_IPS4O 1
#endif

#if __has_include("ips2ra/include/ips2ra.hpp")
#include "ips2ra/include/ips2ra.hpp"
#define HAS_IPS2RA 1
#endif

// --- Data Generation Utility ---
void generate_data(std::vector<int>& data, size_t size, int distribution_type) {
    data.resize(size);
    std::mt19937 gen(42); 
    if (distribution_type == 0) { // Uniform Distribution
        std::uniform_int_distribution<int> dist(0, size - 1);
        for (size_t i = 0; i < size; ++i) data[i] = dist(gen);
    } else if (distribution_type == 1) { // Highly Skewed
        std::uniform_int_distribution<int> dist(0, size / 100);
        for (size_t i = 0; i < size; ++i) data[i] = dist(gen);
    } else if (distribution_type == 2) { // Few Unique
        std::uniform_int_distribution<int> dist(0, 10);
        for (size_t i = 0; i < size; ++i) data[i] = dist(gen);
    }
}

// ==============================================
// 1. THE WORLD CHAMPIONS
// ==============================================
static void BM_WorldChamp_StdSort(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    std::vector<int> data; generate_data(data, size, dist_type);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        std::sort(copy.begin(), copy.end());
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

#ifdef HAS_IPS4O
static void BM_WorldChamp_IPS4o(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        ips4o::parallel::sort(copy.begin(), copy.end(), threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}
#endif

#ifdef HAS_IPS2RA
static void BM_WorldChamp_IPS2Ra(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        ips2ra::parallel::sort(copy.begin(), copy.end(), threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}
#endif

// ==============================================
// 2. THE ANTIGRAVITY ALGORITHMS
// ==============================================

// 2.1 V6 AVX2 (Tiny Domain Champion)
static void BM_Antigravity_V6_AVX2(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    int max_val = *std::max_element(data.begin(), data.end());
    initialize_avx2_locked_memory(threads, max_val + 1);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v6_avx2(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
    cleanup_avx2_locked_memory();
}

// 2.2 V6 AVX2 Tiered (Master Algorithm)
static void BM_Antigravity_V6_AVX2_Tiered(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    int max_val = *std::max_element(data.begin(), data.end());
    initialize_avx2_tiered_locked_memory(threads, max_val + 1);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v6_avx2_tiered(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
    cleanup_avx2_tiered_locked_memory();
}

// 2.3 V5 Tiered (Skewed Data Champion)
static void BM_Antigravity_V5_Tiered(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    int max_val = *std::max_element(data.begin(), data.end());
    initialize_tiered_locked_memory(threads, max_val + 1);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v5_tiered(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
    cleanup_tiered_locked_memory();
}

// 2.4 V5 Locked (Original Baseline)
static void BM_Antigravity_V5_Locked(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    int max_val = *std::max_element(data.begin(), data.end());
    initialize_locked_memory(threads, max_val + 1);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v5_locked(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
    cleanup_locked_memory();
}

// 2.5 V5 MultiPass (Uncompressible Data Baseline)
static void BM_Antigravity_V5_MultiPass(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v5_multipass(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

// 2.6 V6 AVX2 MultiPass (Cloud-Scale AVX2 Sweeper)
static void BM_Antigravity_V6_AVX2_MultiPass(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    
    initialize_avx2_multipass_locked_memory(threads, size);

    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v6_avx2_multipass(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    
    cleanup_avx2_multipass_locked_memory();
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

// 2.7 V4 Global Atomic Baseline
static void BM_Antigravity_V4_GlobalAtomic(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);

    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort(copy, threads, 0); // V4 uses this name
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

// 2.8 BSP Radix Baseline
static void BM_BSP_RadixSort(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);
    std::vector<int> data; generate_data(data, size, dist_type);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        bsp_parallel_radix_sort(copy, threads); // BSP uses this name
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

// ==============================================
// 3. EXECUTION ARGS
// ==============================================
static void CustomArguments(benchmark::internal::Benchmark* b) {
    // Exascale arena: 100M, 500M, 1 Billion!
    std::vector<size_t> sizes = {100000000, 500000000, 1000000000}; 
    // 0 = Uniform, 1 = Highly Skewed, 2 = Few Unique
    std::vector<int> dists = {0, 1, 2}; 
    // 16 Threads
    std::vector<int> threads = {16};
    
    for (size_t size : sizes) {
        for (int dist : dists) {
            for (int t : threads) {
                b->Args({static_cast<long long>(size), dist, t});
            }
        }
    }
}

BENCHMARK(BM_WorldChamp_StdSort)->Apply(CustomArguments)->UseManualTime();
#ifdef HAS_IPS4O
BENCHMARK(BM_WorldChamp_IPS4o)->Apply(CustomArguments)->UseManualTime();
#endif
#ifdef HAS_IPS2RA
BENCHMARK(BM_WorldChamp_IPS2Ra)->Apply(CustomArguments)->UseManualTime();
#endif

BENCHMARK(BM_Antigravity_V6_AVX2)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_Antigravity_V6_AVX2_Tiered)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_Antigravity_V5_Tiered)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_Antigravity_V5_Locked)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_Antigravity_V5_MultiPass)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_Antigravity_V6_AVX2_MultiPass)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_Antigravity_V4_GlobalAtomic)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_BSP_RadixSort)->Apply(CustomArguments)->UseManualTime();

BENCHMARK_MAIN();
