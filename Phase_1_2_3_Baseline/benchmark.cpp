#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <execution>
#include <omp.h>
#include <windows.h>
#include <psapi.h>
#include <thread>
#include "JITSlipperyRadixSort.h"
#include "BSPRadixSort.h"

enum class DataDistribution {
    Uniform,
    HighlySkewed,
    FewUnique,
    Sorted,
    ReverseSorted
};

size_t getPeakRSS() {
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.PeakWorkingSetSize;
}

std::vector<int> generate_data(size_t size, DataDistribution dist) {
    std::vector<int> data(size);
    std::mt19937 gen(42);
    // Keep max value within 99,999,999 to use max_digits=8 in JIT Slippery Radix Sort
    std::uniform_int_distribution<int> unif(0, 99999999);
    
    if (dist == DataDistribution::Uniform) {
        for(auto& val : data) val = unif(gen);
    } else if (dist == DataDistribution::HighlySkewed) {
        int dup_val = unif(gen);
        std::uniform_int_distribution<int> unif100(1, 1000);
        for(auto& val : data) {
            // 99.9% duplicate
            if (unif100(gen) <= 999) val = dup_val;
            else val = unif(gen);
        }
    } else if (dist == DataDistribution::FewUnique) {
        std::vector<int> unique_vals;
        for (int i=0; i<10; ++i) unique_vals.push_back(unif(gen));
        std::uniform_int_distribution<int> unif10(0, 9);
        for(auto& val : data) {
            val = unique_vals[unif10(gen)];
        }
    } else if (dist == DataDistribution::Sorted) {
        for(auto& val : data) val = unif(gen);
        std::sort(data.begin(), data.end());
    } else if (dist == DataDistribution::ReverseSorted) {
        for(auto& val : data) val = unif(gen);
        std::sort(data.begin(), data.end(), std::greater<int>());
    }
    return data;
}

static void BM_JITSlipperyRadix(benchmark::State& state) {
    size_t size = state.range(0);
    DataDistribution dist = static_cast<DataDistribution>(state.range(1));
    int num_threads = state.range(2);
    std::vector<int> original_data = generate_data(size, dist);
    
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<int> data = original_data;
        state.ResumeTiming();
        jit_slippery_radix_sort(data, num_threads, 8);
        benchmark::DoNotOptimize(data.data());
        benchmark::ClobberMemory();
        state.counters["PeakRAM_MB"] = getPeakRSS() / (1024.0 * 1024.0);
    }
    state.SetItemsProcessed(state.iterations() * size);
}

static void BM_BSPRadixSort(benchmark::State& state) {
    size_t size = state.range(0);
    DataDistribution dist = static_cast<DataDistribution>(state.range(1));
    int num_threads = state.range(2);
    std::vector<int> original_data = generate_data(size, dist);
    
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<int> data = original_data;
        state.ResumeTiming();
        bsp_parallel_radix_sort(data, num_threads);
        benchmark::DoNotOptimize(data.data());
        benchmark::ClobberMemory();
        state.counters["PeakRAM_MB"] = getPeakRSS() / (1024.0 * 1024.0);
    }
    state.SetItemsProcessed(state.iterations() * size);
}

static void BM_StdSortPar(benchmark::State& state) {
    size_t size = state.range(0);
    DataDistribution dist = static_cast<DataDistribution>(state.range(1));
    int num_threads = state.range(2);
    
    // Control OpenMP threads for std::execution::par under _GLIBCXX_PARALLEL
    omp_set_num_threads(num_threads);

    std::vector<int> original_data = generate_data(size, dist);
    
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<int> data = original_data;
        state.ResumeTiming();
        std::sort(std::execution::par, data.begin(), data.end());
        benchmark::DoNotOptimize(data.data());
        benchmark::ClobberMemory();
        state.counters["PeakRAM_MB"] = getPeakRSS() / (1024.0 * 1024.0);
    }
    state.SetItemsProcessed(state.iterations() * size);
}

static void BM_StdSort(benchmark::State& state) {
    size_t size = state.range(0);
    DataDistribution dist = static_cast<DataDistribution>(state.range(1));
    std::vector<int> original_data = generate_data(size, dist);
    
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<int> data = original_data;
        state.ResumeTiming();
        std::sort(data.begin(), data.end());
        benchmark::DoNotOptimize(data.data());
        benchmark::ClobberMemory();
        state.counters["PeakRAM_MB"] = getPeakRSS() / (1024.0 * 1024.0);
    }
    state.SetItemsProcessed(state.iterations() * size);
}

static void CustomArgumentsParallel(benchmark::internal::Benchmark* b) {
    std::vector<int> sizes = {1000000, 10000000, 50000000, 100000000};
    std::vector<int> dists = {0, 1, 2, 3, 4};
    std::vector<int> threads = {1, 2, 4, 8, 16};
    for (int size : sizes) {
        for (int dist : dists) {
            for (int thread : threads) {
                b->Args({size, dist, thread});
            }
        }
    }
}

// Strong Scaling explicitly
static void CustomArgumentsStrongScalingJIT(benchmark::internal::Benchmark* b) {
    // Standard suite
    std::vector<int> sizes = {1000000, 10000000, 50000000, 100000000};
    std::vector<int> dists = {0, 1, 2, 3, 4};
    std::vector<int> threads = {1, 2, 4, 8, 16};
    int max_cores = std::thread::hardware_concurrency();
    if (max_cores > 16 && std::find(threads.begin(), threads.end(), max_cores) == threads.end()) {
        threads.push_back(max_cores);
    }

    for (int size : sizes) {
        for (int dist : dists) {
            for (int thread : threads) {
                b->Args({size, dist, thread});
            }
        }
    }
}

static void CustomArgumentsSeq(benchmark::internal::Benchmark* b) {
    std::vector<int> sizes = {1000000, 10000000, 50000000, 100000000};
    std::vector<int> dists = {0, 1, 2, 3, 4}; // Uniform, HighlySkewed, FewUnique, Sorted, ReverseSorted
    for (int size : sizes) {
        for (int dist : dists) {
            b->Args({size, dist});
        }
    }
}

// Fast algorithms registered first
BENCHMARK(BM_JITSlipperyRadix)->Apply(CustomArgumentsStrongScalingJIT)->Iterations(10);
BENCHMARK(BM_BSPRadixSort)->Apply(CustomArgumentsParallel)->Iterations(10);
// Slower baselines registered after
BENCHMARK(BM_StdSortPar)->Apply(CustomArgumentsParallel)->Iterations(10);
BENCHMARK(BM_StdSort)->Apply(CustomArgumentsSeq)->Iterations(10);

BENCHMARK_MAIN();
