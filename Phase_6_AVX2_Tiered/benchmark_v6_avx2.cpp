#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include "../V5_ThreadLocal_Pipeline/JITSlipperyRadixSortV5_Locked.h"
#include "../V5_ThreadLocal_Pipeline/JITSlipperyRadixSortV5_Tiered.h"
#include "../V5_ThreadLocal_Pipeline/JITSlipperyRadixSortV5_MultiPass.h"
#include "JITSlipperyRadixSortV6_AVX2.h"
#include "JITSlipperyRadixSortV6_AVX2_Tiered.h"

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
    } else if (distribution_type == 3) { // Sorted
        for (size_t i = 0; i < size; ++i) data[i] = i;
    } else if (distribution_type == 4) { // Reverse-sorted
        for (size_t i = 0; i < size; ++i) data[i] = size - i;
    }
}

// --- Benchmark: JIT Slippery Radix V5 Locked (Single-Pass) ---
static void BM_JITSlipperyRadix_V5_Locked(benchmark::State& state) {
    size_t size = state.range(0);
    int distribution_type = state.range(1);
    int num_threads = state.range(2);

    std::vector<int> original_data;
    generate_data(original_data, size, distribution_type);

    for (auto _ : state) {
        std::vector<int> copy = original_data;
        
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v5_locked(copy, num_threads);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        
        benchmark::DoNotOptimize(copy);
        benchmark::ClobberMemory();
    }
    
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
    cleanup_locked_memory();
}

// 2. AVX2 Vectorized Implementation
static void BM_JITSlipperyRadix_V6_AVX2(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);

    std::vector<int> data;
    generate_data(data, size, dist_type);
    
    // Warmup memory pool
    int max_val = *std::max_element(data.begin(), data.end());
    initialize_avx2_locked_memory(threads, max_val + 1);

    for (auto _ : state) {
        std::vector<int> copy = data;
        
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v6_avx2(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
    cleanup_avx2_locked_memory();
}

// 3. AVX2 Tiered Master Algorithm
static void BM_JITSlipperyRadix_V6_AVX2_Tiered(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);

    std::vector<int> data;
    generate_data(data, size, dist_type);
    
    // Warmup memory pool
    int max_val = *std::max_element(data.begin(), data.end());
    initialize_avx2_tiered_locked_memory(threads, ((long long)max_val + 1 + 7) & ~7ULL);

    for (auto _ : state) {
        std::vector<int> copy = data;
        
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v6_avx2_tiered(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        
        benchmark::DoNotOptimize(copy);
        benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
    cleanup_avx2_tiered_locked_memory();
}

// 4. V5 Tiered (Original uint8_t if-statement version)
static void BM_JITSlipperyRadix_V5_Tiered(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);

    std::vector<int> data;
    generate_data(data, size, dist_type);
    
    int max_val = *std::max_element(data.begin(), data.end());
    initialize_tiered_locked_memory(threads, max_val + 1);

    for (auto _ : state) {
        std::vector<int> copy = data;
        
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v5_tiered(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        
        benchmark::DoNotOptimize(copy);
        benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
    cleanup_tiered_locked_memory();
}

// 5. V5 MultiPass (8-bit Sliding Window)
static void BM_JITSlipperyRadix_V5_MultiPass(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);

    std::vector<int> data;
    generate_data(data, size, dist_type);

    for (auto _ : state) {
        std::vector<int> copy = data;
        
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v5_multipass(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        
        benchmark::DoNotOptimize(copy);
        benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

// Register FULL test suite for all 5 architectures
static void CustomArguments(benchmark::internal::Benchmark* b) {
    std::vector<size_t> sizes = {1000000, 10000000, 50000000, 100000000};
    std::vector<int> dists = {0, 1, 2, 3, 4};
    std::vector<int> threads = {1, 2, 4, 8, 16};
    
    for (size_t size : sizes) {
        for (int dist : dists) {
            for (int t : threads) {
                b->Args({static_cast<long long>(size), dist, t});
            }
        }
    }
}

BENCHMARK(BM_JITSlipperyRadix_V6_AVX2_Tiered)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_JITSlipperyRadix_V6_AVX2)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_JITSlipperyRadix_V5_Tiered)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_JITSlipperyRadix_V5_Locked)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_JITSlipperyRadix_V5_MultiPass)->Apply(CustomArguments)->UseManualTime();

BENCHMARK_MAIN();
