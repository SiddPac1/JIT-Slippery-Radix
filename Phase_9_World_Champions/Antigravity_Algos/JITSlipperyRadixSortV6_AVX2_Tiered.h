#pragma once
#include <vector>
#include <omp.h>
#include <iostream>
#include <windows.h>
#include <immintrin.h>

// --- The Locked Memory Pool (Tiered 1-Byte) ---
static std::vector<uint8_t*> avx_tiered_locked_pool;
static long long avx_tiered_locked_size = 0;

void cleanup_avx2_tiered_locked_memory() {
    for (uint8_t* ptr : avx_tiered_locked_pool) {
        if (ptr) VirtualFree(ptr, 0, MEM_RELEASE);
    }
    avx_tiered_locked_pool.clear();
    avx_tiered_locked_size = 0;
}

void initialize_avx2_tiered_locked_memory(int num_threads, long long size) {
    if (avx_tiered_locked_size >= size && avx_tiered_locked_pool.size() >= num_threads) {
        #pragma omp parallel for
        for (int t = 0; t < num_threads; ++t) {
            memset(avx_tiered_locked_pool[t], 0, size * sizeof(uint8_t));
        }
        return;
    }

    cleanup_avx2_tiered_locked_memory();
    
    avx_tiered_locked_pool.resize(num_threads);
    avx_tiered_locked_size = size;

    SIZE_T total_ram_bytes = (SIZE_T)num_threads * size * sizeof(uint8_t) + (SIZE_T)(9.5 * 1024 * 1024 * 1024ULL);
    SetProcessWorkingSetSize(GetCurrentProcess(), total_ram_bytes, total_ram_bytes);

    #pragma omp parallel for
    for (int t = 0; t < num_threads; ++t) {
        avx_tiered_locked_pool[t] = (uint8_t*)VirtualAlloc(NULL, size * sizeof(uint8_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        VirtualLock(avx_tiered_locked_pool[t], size * sizeof(uint8_t));
        memset(avx_tiered_locked_pool[t], 0, size * sizeof(uint8_t));
    }
}

void jit_slippery_radix_sort_v6_avx2_tiered(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);

    // 1. AVX2 Vectorized Max-Val (Reduction)
    int global_max = 0;
    #pragma omp parallel
    {
        __m256i local_max_vec = _mm256_setzero_si256();
        int local_max_scalar = 0;
        
        #pragma omp for schedule(static) nowait
        for (size_t i = 0; i < (data.size() & ~7ULL); i += 8) {
            __m256i v = _mm256_loadu_si256((const __m256i*)&data[i]);
            local_max_vec = _mm256_max_epi32(local_max_vec, v);
        }
        
        alignas(32) int max_arr[8];
        _mm256_store_si256((__m256i*)max_arr, local_max_vec);
        for (int j = 0; j < 8; ++j) {
            if (max_arr[j] > local_max_scalar) local_max_scalar = max_arr[j];
        }
        
        #pragma omp critical
        {
            if (local_max_scalar > global_max) global_max = local_max_scalar;
        }

        #pragma omp single
        {
            for (size_t j = (data.size() & ~7ULL); j < data.size(); ++j) {
                if (data[j] > global_max) global_max = data[j];
            }
        }
    }

    long long hist_size = (long long)global_max + 1;
    long long aligned_hist_size = (hist_size + 7) & ~7ULL; 

    initialize_avx2_tiered_locked_memory(num_threads, aligned_hist_size);

    std::vector<std::vector<int>> warm_logs(num_threads);

    // 2. The Branchless Tiered JIT Pipeline
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        uint8_t* local_hot = avx_tiered_locked_pool[thread_id];
        
        // Pre-allocate to mathematically maximum possible size to prevent OS allocation stalls
        std::vector<int>& local_log = warm_logs[thread_id];
        local_log.resize(data.size() / 256 + 1000);
        int* log_data = local_log.data();
        int log_index = 0;
        
        #pragma omp for schedule(static)
        for (size_t i = 0; i < data.size(); ++i) {
            int element = data[i];
            
            // Branchless Wrap-Around Slip Logic (Zero if statements!)
            local_hot[element]++;
            int overflow = (local_hot[element] == 0); 
            log_data[log_index] = element;
            log_index += overflow;
        }
        
        // Trim unused capacity
        local_log.resize(log_index);
    }

    // 3. AVX2 Upcast Combination Phase
    std::vector<int> global_histogram(aligned_hist_size, 0);

    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < aligned_hist_size; v += 8) {
        __m256i sum_vec = _mm256_setzero_si256();
        
        for (int t = 0; t < num_threads; ++t) {
            // Load 8 bytes (uint8_t) into lower 64 bits
            __m128i bytes = _mm_loadl_epi64((const __m128i*)&avx_tiered_locked_pool[t][v]);
            
            // Instantly Upcast 8 bytes into 8 32-bit integers
            __m256i thread_vec = _mm256_cvtepu8_epi32(bytes);
            
            // Add to sum
            sum_vec = _mm256_add_epi32(sum_vec, thread_vec);
        }
        _mm256_storeu_si256((__m256i*)&global_histogram[v], sum_vec);
    }

    // Apply Warm Overflows (Each overflow represents exactly 256 counts)
    #pragma omp parallel for
    for (int t = 0; t < num_threads; ++t) {
        for (int element : warm_logs[t]) {
            #pragma omp atomic
            global_histogram[element] += 256;
        }
    }

    // 4. Compute Offsets
    std::vector<size_t> write_offsets(hist_size, 0);
    size_t current_offset = 0;
    for (long long v = 0; v < hist_size; ++v) {
        write_offsets[v] = current_offset;
        current_offset += global_histogram[v];
    }

    // 5. AVX2 Vectorized Write-Back Phase
    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        size_t start_idx = write_offsets[v];
        int weight = global_histogram[v];
        
        if (weight > 0) {
            __m256i val_vec = _mm256_set1_epi32((int)v);
            int i = 0;
            
            for (; i + 8 <= weight; i += 8) {
                _mm256_storeu_si256((__m256i*)&data[start_idx + i], val_vec);
            }
            
            for (; i < weight; ++i) {
                data[start_idx + i] = v;
            }
        }
    }
}
