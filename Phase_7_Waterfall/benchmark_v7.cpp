#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include "JITSlipperyRadixSortV7_Waterfall.h"

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
    }
}

static void BM_JITSlipperyRadix_V7_Waterfall(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    int threads = state.range(2);

    std::vector<int> data;
    generate_data(data, size, dist_type);

    for (auto _ : state) {
        std::vector<int> copy = data;
        
        auto start = std::chrono::high_resolution_clock::now();
        jit_slippery_radix_sort_v7_waterfall(copy, threads);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        
        benchmark::DoNotOptimize(copy);
        benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

static void CustomArguments(benchmark::internal::Benchmark* b) {
    std::vector<size_t> sizes = {10000000, 50000000};
    std::vector<int> dists = {0, 1}; // Uniform, Skewed
    std::vector<int> threads = {1, 16};
    
    for (size_t size : sizes) {
        for (int dist : dists) {
            for (int t : threads) {
                b->Args({static_cast<long long>(size), dist, t});
            }
        }
    }
}

BENCHMARK(BM_JITSlipperyRadix_V7_Waterfall)->Apply(CustomArguments)->UseManualTime();

BENCHMARK_MAIN();
