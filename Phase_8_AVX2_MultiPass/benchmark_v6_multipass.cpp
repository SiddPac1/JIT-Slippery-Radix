#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include "JITSlipperyRadixSortV6_AVX2_MultiPass.h"

void generate_data(std::vector<int>& data, size_t size, int distribution_type) {
    data.resize(size);
    std::mt19937 gen(42);
    if (distribution_type == 0) {
        std::uniform_int_distribution<int> dist(0, size - 1);
        for (size_t i = 0; i < size; ++i) data[i] = dist(gen);
    } else if (distribution_type == 1) {
        std::uniform_int_distribution<int> dist(0, size / 100);
        for (size_t i = 0; i < size; ++i) data[i] = dist(gen);
    } else if (distribution_type == 2) {
        std::uniform_int_distribution<int> dist(0, 10);
        for (size_t i = 0; i < size; ++i) data[i] = dist(gen);
    }
}

static void BM_StdSort(benchmark::State& state) {
    size_t size = state.range(0);
    int dist_type = state.range(1);
    std::vector<int> data; generate_data(data, size, dist_type);
    for (auto _ : state) {
        std::vector<int> copy = data;
        auto start = std::chrono::high_resolution_clock::now();
        std::sort(copy.begin(), copy.end());
        auto end = std::chrono::high_resolution_clock::now();
        state.SetIterationTime(std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

static void BM_V6_AVX2_MultiPass(benchmark::State& state) {
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
        state.SetIterationTime(std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count());
        benchmark::DoNotOptimize(copy); benchmark::ClobberMemory();
    }
    
    cleanup_avx2_multipass_locked_memory();
    state.counters["items_per_second"] = benchmark::Counter(size, benchmark::Counter::kIsIterationInvariantRate);
}

static void CustomArguments(benchmark::internal::Benchmark* b) {
    std::vector<size_t> sizes = {1000000, 10000000}; // Small test first as requested
    std::vector<int> dists = {0, 1, 2}; // Uniform, Skewed, Few Unique
    std::vector<int> threads = {1, 16};
    
    for (size_t size : sizes) {
        for (int dist : dists) {
            for (int t : threads) {
                b->Args({static_cast<long long>(size), dist, t});
            }
        }
    }
}

BENCHMARK(BM_StdSort)->Apply(CustomArguments)->UseManualTime();
BENCHMARK(BM_V6_AVX2_MultiPass)->Apply(CustomArguments)->UseManualTime();

BENCHMARK_MAIN();
