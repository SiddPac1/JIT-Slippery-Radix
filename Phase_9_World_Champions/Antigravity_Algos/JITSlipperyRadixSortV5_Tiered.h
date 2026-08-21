#pragma once
#include <vector>
#include <omp.h>
#include <iostream>
#include <windows.h>
#include <algorithm>

// --- The Locked Hot Layer (2-Byte Array) ---
static std::vector<uint8_t*> locked_hot_pool;
static long long locked_hot_size_per_thread = 0;

void cleanup_tiered_locked_memory() {
    for (uint8_t* ptr : locked_hot_pool) {
        if (ptr) VirtualFree(ptr, 0, MEM_RELEASE);
    }
    locked_hot_pool.clear();
    locked_hot_size_per_thread = 0;
}

void initialize_tiered_locked_memory(int num_threads, long long size) {
    if (locked_hot_size_per_thread >= size && locked_hot_pool.size() >= num_threads) {
        #pragma omp parallel for
        for (int t = 0; t < num_threads; ++t) {
            memset(locked_hot_pool[t], 0, size * sizeof(uint8_t));
        }
        return;
    }

    cleanup_tiered_locked_memory();
    
    locked_hot_pool.resize(num_threads);
    locked_hot_size_per_thread = size;
    SIZE_T total_ram_bytes = (SIZE_T)num_threads * size * sizeof(uint8_t) + (SIZE_T)(9.5 * 1024 * 1024 * 1024ULL);
    SetProcessWorkingSetSize(GetCurrentProcess(), total_ram_bytes, total_ram_bytes);

    #pragma omp parallel for
    for (int t = 0; t < num_threads; ++t) {
        locked_hot_pool[t] = (uint8_t*)VirtualAlloc(NULL, size * sizeof(uint8_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        VirtualLock(locked_hot_pool[t], size * sizeof(uint8_t));
        memset(locked_hot_pool[t], 0, size * sizeof(uint8_t));
    }
}

void jit_slippery_radix_sort_v5_tiered(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);

    int max_val = 0;
    #pragma omp parallel for reduction(max:max_val) schedule(static)
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] > max_val) max_val = data[i];
    }

    long long hist_size = (long long)max_val + 1;

    // 1. Grab our exclusively locked 2-Byte Hot RAM
    initialize_tiered_locked_memory(num_threads, hist_size);

    // 2. The Warm Layer (Append-Only Log)
    // We use a vector for each thread. It will use almost zero memory.
    std::vector<std::vector<int>> warm_logs(num_threads);

    // 3. The Tiered JIT Pipeline
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        uint8_t* local_hot = locked_hot_pool[thread_id];
        
        // Reserve a small amount of memory to prevent reallocation during the loop
        warm_logs[thread_id].reserve(data.size() / 256);

        #pragma omp for schedule(static)
        for (size_t i = 0; i < data.size(); ++i) {
            int element = data[i];
            
            local_hot[element]++;
            
            // The Slipping Mechanism! (Branch prediction will guess False 99.999% of the time)
            if (local_hot[element] == 255) {
                warm_logs[thread_id].push_back(element);
                local_hot[element] = 0;
            }
        }
    }

    // 4. The Combination Phase
    std::vector<int> global_histogram(hist_size, 0);

    // 4A. Combine Hot Arrays
    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        int sum = 0;
        for (int t = 0; t < num_threads; ++t) {
            sum += locked_hot_pool[t][v];
        }
        global_histogram[v] = sum;
    }

    // 4B. Apply Warm Overflows
    #pragma omp parallel for
    for (int t = 0; t < num_threads; ++t) {
        for (int element : warm_logs[t]) {
            #pragma omp atomic
            global_histogram[element] += 255;
        }
    }

    // 5. Compute Exact Memory Offsets
    std::vector<size_t> write_offsets(hist_size, 0);
    size_t current_offset = 0;
    for (long long v = 0; v < hist_size; ++v) {
        write_offsets[v] = current_offset;
        current_offset += global_histogram[v];
    }

    // 6. Contention-Free Parallel Write-Back
    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        size_t start_idx = write_offsets[v];
        int weight = global_histogram[v];
        for (int i = 0; i < weight; ++i) {
            data[start_idx + i] = v;
        }
    }
}
