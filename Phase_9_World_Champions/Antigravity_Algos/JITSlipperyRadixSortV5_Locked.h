#pragma once
#include <vector>
#include <omp.h>
#include <iostream>
#include <windows.h>
#include <algorithm>

// We pre-allocate the memory pool globally so it is not destroyed between iterations.
static std::vector<int*> locked_memory_pool;
static long long locked_pool_size_per_thread = 0;

void cleanup_locked_memory() {
    for (int* ptr : locked_memory_pool) {
        if (ptr) VirtualFree(ptr, 0, MEM_RELEASE);
    }
    locked_memory_pool.clear();
    locked_pool_size_per_thread = 0;
}

void initialize_locked_memory(int num_threads, long long size) {
    if (locked_pool_size_per_thread >= size && locked_memory_pool.size() >= num_threads) {
        // Memory is already large enough. Just zero it out to reuse.
        #pragma omp parallel for
        for (int t = 0; t < num_threads; ++t) {
            memset(locked_memory_pool[t], 0, size * sizeof(int));
        }
        return;
    }

    cleanup_locked_memory();
    
    locked_memory_pool.resize(num_threads);
    locked_pool_size_per_thread = size;

    // Increase process working set size so Windows allows us to lock massive amounts of RAM
    SIZE_T total_ram_bytes = (SIZE_T)num_threads * size * sizeof(int) + (SIZE_T)(9.5 * 1024 * 1024 * 1024ULL); // +1GB buffer
    SetProcessWorkingSetSize(GetCurrentProcess(), total_ram_bytes, total_ram_bytes);

    #pragma omp parallel for
    for (int t = 0; t < num_threads; ++t) {
        // MEM_COMMIT physically allocates the pages.
        locked_memory_pool[t] = (int*)VirtualAlloc(NULL, size * sizeof(int), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        // VirtualLock physically locks the pages into RAM (No Page Faults allowed!)
        VirtualLock(locked_memory_pool[t], size * sizeof(int));
        
        // Zero initialize
        memset(locked_memory_pool[t], 0, size * sizeof(int));
    }
}

void jit_slippery_radix_sort_v5_locked(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);

    int max_val = 0;
    #pragma omp parallel for reduction(max:max_val) schedule(static)
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] > max_val) max_val = data[i];
    }

    long long hist_size = (long long)max_val + 1;

    // 1. Grab our exclusively locked physical RAM!
    initialize_locked_memory(num_threads, hist_size);

    // 2. The Contention-Free JIT Pipeline
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int* local_hist = locked_memory_pool[thread_id];
        
        #pragma omp for schedule(static)
        for (size_t i = 0; i < data.size(); ++i) {
            int element = data[i];
            local_hist[element]++;
        }
    }

    // 3. The Contention-Free Combination Phase
    std::vector<int> global_histogram(hist_size, 0);

    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        int sum = 0;
        for (int t = 0; t < num_threads; ++t) {
            sum += locked_memory_pool[t][v];
        }
        global_histogram[v] = sum;
    }

    // 4. Compute Exact Memory Offsets (Prefix Sum)
    std::vector<size_t> write_offsets(hist_size, 0);
    size_t current_offset = 0;
    for (long long v = 0; v < hist_size; ++v) {
        write_offsets[v] = current_offset;
        current_offset += global_histogram[v];
    }

    // 5. The Contention-Free Parallel Write-Back
    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        size_t start_idx = write_offsets[v];
        int weight = global_histogram[v];
        for (int i = 0; i < weight; ++i) {
            data[start_idx + i] = v;
        }
    }
}
