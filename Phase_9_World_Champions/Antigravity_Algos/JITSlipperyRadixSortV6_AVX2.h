#pragma once
#include <vector>
#include <omp.h>
#include <iostream>
#include <windows.h>
#include <immintrin.h>

// --- The Locked Memory Pool (V5 Baseline) ---
static std::vector<int*> avx_locked_pool;
static long long avx_locked_size_per_thread = 0;

void cleanup_avx2_locked_memory() {
    for (int* ptr : avx_locked_pool) {
        if (ptr) VirtualFree(ptr, 0, MEM_RELEASE);
    }
    avx_locked_pool.clear();
    avx_locked_size_per_thread = 0;
}

void initialize_avx2_locked_memory(int num_threads, long long size) {
    if (avx_locked_size_per_thread >= size && avx_locked_pool.size() >= num_threads) {
        #pragma omp parallel for
        for (int t = 0; t < num_threads; ++t) {
            memset(avx_locked_pool[t], 0, size * sizeof(int));
        }
        return;
    }

    cleanup_avx2_locked_memory();
    
    avx_locked_pool.resize(num_threads);
    avx_locked_size_per_thread = size;

    SIZE_T total_ram_bytes = (SIZE_T)num_threads * size * sizeof(int) + (SIZE_T)(9.5 * 1024 * 1024 * 1024ULL);
    SetProcessWorkingSetSize(GetCurrentProcess(), total_ram_bytes, total_ram_bytes);

    #pragma omp parallel for
    for (int t = 0; t < num_threads; ++t) {
        avx_locked_pool[t] = (int*)VirtualAlloc(NULL, size * sizeof(int), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        VirtualLock(avx_locked_pool[t], size * sizeof(int));
        memset(avx_locked_pool[t], 0, size * sizeof(int));
    }
}

void jit_slippery_radix_sort_v6_avx2(std::vector<int>& data, int num_threads) {
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
    
    // Ensure hist_size is a multiple of 8 for SIMD addition safety
    long long aligned_hist_size = (hist_size + 7) & ~7ULL; 

    initialize_avx2_locked_memory(num_threads, aligned_hist_size);

    // 2. The Standard Scatter Phase (AVX2 doesn't have scatter)
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int* local_hist = avx_locked_pool[thread_id];
        
        #pragma omp for schedule(static)
        for (size_t i = 0; i < data.size(); ++i) {
            local_hist[data[i]]++;
        }
    }

    // 3. AVX2 Vectorized Combination Phase (Addition)
    std::vector<int> global_histogram(aligned_hist_size, 0);

    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < aligned_hist_size; v += 8) {
        __m256i sum_vec = _mm256_setzero_si256();
        
        for (int t = 0; t < num_threads; ++t) {
            __m256i thread_vec = _mm256_loadu_si256((const __m256i*)&avx_locked_pool[t][v]);
            sum_vec = _mm256_add_epi32(sum_vec, thread_vec);
        }
        _mm256_storeu_si256((__m256i*)&global_histogram[v], sum_vec);
    }

    // 4. Compute Offsets
    std::vector<size_t> write_offsets(hist_size, 0);
    size_t current_offset = 0;
    for (long long v = 0; v < hist_size; ++v) {
        write_offsets[v] = current_offset;
        current_offset += global_histogram[v];
    }

    // 5. AVX2 Vectorized Write-Back Phase (Store)
    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        size_t start_idx = write_offsets[v];
        int weight = global_histogram[v];
        
        if (weight > 0) {
            __m256i val_vec = _mm256_set1_epi32((int)v);
            int i = 0;
            
            // SIMD Store 8 integers per clock
            for (; i + 8 <= weight; i += 8) {
                _mm256_storeu_si256((__m256i*)&data[start_idx + i], val_vec);
            }
            
            // Remainder
            for (; i < weight; ++i) {
                data[start_idx + i] = v;
            }
        }
    }
}
