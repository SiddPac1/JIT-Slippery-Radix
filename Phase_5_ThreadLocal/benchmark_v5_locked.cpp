#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <omp.h>
#include "JITSlipperyRadixSortV5_Locked.h"
#include "JITSlipperyRadixSortV5_MultiPass.h"

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
        state.PauseTiming();
        std::vector<int> data = original_data;
        state.ResumeTiming();

        jit_slippery_radix_sort_v5_locked(data, num_threads);

        benchmark::DoNotOptimize(data);
        benchmark::ClobberMemory();
    }
    
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

// --- Benchmark: JIT Slippery Radix V5 Multi-Pass ---
static void BM_JITSlipperyRadix_V5_MultiPass(benchmark::State& state) {
    size_t size = state.range(0);
    int distribution_type = state.range(1);
    int num_threads = state.range(2);

    std::vector<int> original_data;
    generate_data(original_data, size, distribution_type);

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<int> data = original_data;
        state.ResumeTiming();

        jit_slippery_radix_sort_v5_multipass(data, num_threads);

        benchmark::DoNotOptimize(data);
        benchmark::ClobberMemory();
    }
    
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

// Register full suite for V5 Locked
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

BENCHMARK(BM_JITSlipperyRadix_V5_Locked)->Apply(CustomArguments)->Unit(benchmark::kMillisecond)->Iterations(10);
BENCHMARK(BM_JITSlipperyRadix_V5_MultiPass)->Apply(CustomArguments)->Unit(benchmark::kMillisecond)->Iterations(10);

BENCHMARK_MAIN();
