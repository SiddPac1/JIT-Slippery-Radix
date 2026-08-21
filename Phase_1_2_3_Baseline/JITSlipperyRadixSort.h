#pragma once
#include <vector>
#include <atomic>
#include <omp.h>
#include <iostream>

void jit_slippery_radix_sort(std::vector<int>& data, int num_threads, int /*max_digits - dynamically calculated*/) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);

    // 1. Determine the exact required size of the reference groups (JIT bounded slopes)
    int max_val = 0;
    #pragma omp parallel for reduction(max:max_val) schedule(static)
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] > max_val) max_val = data[i];
    }

    long long hist_size = (long long)max_val + 1;

    // 2. The Flat Atomic Histogram (Leaf Nodes of the Pipeline)
    // We allocate this dynamically. Using std::atomic<int> guarantees lock-free 
    // thread safety as elements collide in the final reference group.
    std::atomic<int>* global_histogram = new std::atomic<int>[hist_size];
    
    #pragma omp parallel for schedule(static)
    for(long long i = 0; i < hist_size; ++i) {
        global_histogram[i].store(0, std::memory_order_relaxed);
    }

    // 3. The Pipelined Dataflow
    // Each thread takes a chunk of the array. The elements slide directly into their 
    // exact reference group (histogram bin) via a single lock-free fetch_add.
    // This perfectly fulfills the O(N) asynchronous JIT flow without intermediate vector thrashing.
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < data.size(); ++i) {
        int element = data[i];
        global_histogram[element].fetch_add(1, std::memory_order_relaxed);
    }

    // 4. Reconstruct the sorted array based on the final reference group weights.
    // Because the histogram indices represent the value, the data is inherently sorted.
    // We just write the weighted elements back sequentially.
    size_t write_idx = 0;
    for (long long v = 0; v < hist_size; ++v) {
        int weight = global_histogram[v].load(std::memory_order_relaxed);
        while (weight > 0) {
            data[write_idx++] = v;
            weight--;
        }
    }

    delete[] global_histogram;
}
