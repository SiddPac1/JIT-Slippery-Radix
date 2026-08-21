#pragma once
#include <vector>
#include <omp.h>
#include <iostream>

void jit_slippery_radix_sort_v5(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);

    // 1. Determine the exact required size of the reference groups (JIT bounded slopes)
    int max_val = 0;
    #pragma omp parallel for reduction(max:max_val) schedule(static)
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] > max_val) max_val = data[i];
    }

    long long hist_size = (long long)max_val + 1;

    // 2. The Thread-Local Histograms (V5 Extreme Steroids)
    // Instead of one global atomic array, we allocate a private array for EVERY thread.
    // This requires (num_threads * hist_size * 4) bytes of RAM.
    // For 100M max_val and 16 threads, this is ~6.4 GB of RAM, ensuring zero contention.
    std::vector<std::vector<int>> local_histograms(num_threads, std::vector<int>(hist_size, 0));

    // 3. The Contention-Free JIT Pipeline
    // Each thread slides elements into its OWN private slope. 
    // ZERO atomic locks, ZERO cache-line contention, pure L1 cache writes.
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        
        #pragma omp for schedule(static)
        for (size_t i = 0; i < data.size(); ++i) {
            int element = data[i];
            local_histograms[thread_id][element]++;
        }
    }

    // 4. The Contention-Free Combination Phase
    // Threads combine the local histograms into one global array.
    // By dividing the INDICES among the threads, no two threads ever write to the same memory!
    std::vector<int> global_histogram(hist_size, 0);

    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        int sum = 0;
        for (int t = 0; t < num_threads; ++t) {
            sum += local_histograms[t][v];
        }
        global_histogram[v] = sum;
    }

    // 5. Compute Exact Memory Offsets (Prefix Sum)
    // To make the final array write-back perfectly parallel, we pre-compute
    // exactly where each reference group should start writing in the main array.
    std::vector<size_t> write_offsets(hist_size, 0);
    size_t current_offset = 0;
    for (long long v = 0; v < hist_size; ++v) {
        write_offsets[v] = current_offset;
        current_offset += global_histogram[v];
    }

    // 6. The Contention-Free Parallel Write-Back
    // Every thread grabs a range of values and independently writes them to the main array.
    // Because the write_offsets are pre-computed, threads never step on each other's memory.
    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        size_t start_idx = write_offsets[v];
        int weight = global_histogram[v];
        for (int i = 0; i < weight; ++i) {
            data[start_idx + i] = v;
        }
    }
}
