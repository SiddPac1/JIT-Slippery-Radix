#pragma once
#include <vector>
#include <omp.h>
#include <iostream>
#include <cstring>
#include <immintrin.h> // AVX2 intrinsics

#ifdef _WIN32
#include <windows.h>
#endif

// ==========================================
// V6 AVX2 MULTIPASS LOCKED MEMORY ARENA
// ==========================================
int* g_aux_array = nullptr;
int** g_multipass_histograms = nullptr;
size_t** g_multipass_offsets = nullptr;
size_t g_aux_size = 0;
int g_num_threads_multipass = 0;

void initialize_avx2_multipass_locked_memory(int num_threads, size_t data_size) {
    g_num_threads_multipass = num_threads;
    g_aux_size = data_size;

#ifdef _WIN32
    // 1. Lock the Massive O(N) Aux Array (e.g. 400 MB for 100M items)
    size_t aux_bytes = data_size * sizeof(int);
    g_aux_array = (int*)VirtualAlloc(NULL, aux_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (g_aux_array) {
        VirtualLock(g_aux_array, aux_bytes);
    }

    // 2. Lock the Thread-Local Histograms & Offsets
    // For MultiPass, the histogram is always exactly 256 buckets (1 KB) per thread.
    g_multipass_histograms = new int*[num_threads];
    g_multipass_offsets = new size_t*[num_threads];
    
    for (int i = 0; i < num_threads; ++i) {
        g_multipass_histograms[i] = (int*)VirtualAlloc(NULL, 256 * sizeof(int), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (g_multipass_histograms[i]) VirtualLock(g_multipass_histograms[i], 256 * sizeof(int));

        g_multipass_offsets[i] = (size_t*)VirtualAlloc(NULL, 256 * sizeof(size_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (g_multipass_offsets[i]) VirtualLock(g_multipass_offsets[i], 256 * sizeof(size_t));
    }
#endif
}

void cleanup_avx2_multipass_locked_memory() {
#ifdef _WIN32
    if (g_aux_array) {
        VirtualUnlock(g_aux_array, g_aux_size * sizeof(int));
        VirtualFree(g_aux_array, 0, MEM_RELEASE);
        g_aux_array = nullptr;
    }
    
    if (g_multipass_histograms && g_multipass_offsets) {
        for (int i = 0; i < g_num_threads_multipass; ++i) {
            if (g_multipass_histograms[i]) {
                VirtualUnlock(g_multipass_histograms[i], 256 * sizeof(int));
                VirtualFree(g_multipass_histograms[i], 0, MEM_RELEASE);
            }
            if (g_multipass_offsets[i]) {
                VirtualUnlock(g_multipass_offsets[i], 256 * sizeof(size_t));
                VirtualFree(g_multipass_offsets[i], 0, MEM_RELEASE);
            }
        }
        delete[] g_multipass_histograms; g_multipass_histograms = nullptr;
        delete[] g_multipass_offsets; g_multipass_offsets = nullptr;
    }
#endif
}


// ==========================================
// V6 AVX2 MULTIPASS ALGORITHM
// ==========================================
void jit_slippery_radix_sort_v6_avx2_multipass(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);
    size_t n = data.size();
    
    int* current_data = data.data();
    int* next_data = g_aux_array;

    // Zero-Vector for AVX2 fast clearing
    __m256i zero_vec = _mm256_setzero_si256();

    // 4 passes for a 32-bit integer (8 bits per pass)
    for (int shift = 0; shift < 32; shift += 8) {
        
        // 1. AVX2 Fast Zero & Count Phase
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int* local_hist = g_multipass_histograms[thread_id];

            // AVX2 Fast Zeroing of the 256-bucket array (8 integers at a time)
            for (int i = 0; i < 256; i += 8) {
                _mm256_storeu_si256((__m256i*)&local_hist[i], zero_vec);
            }

            // OpenMP chunking ensures each thread reads a specific portion of the array
            #pragma omp for schedule(static)
            for (size_t i = 0; i < n; ++i) {
                int bucket = (current_data[i] >> shift) & 0xFF;
                local_hist[bucket]++;
            }
        }

        // 2. Sequential 2D Prefix Sums
        size_t current_offset = 0;
        for (int v = 0; v < 256; ++v) {
            for (int t = 0; t < num_threads; ++t) {
                g_multipass_offsets[t][v] = current_offset;
                current_offset += g_multipass_histograms[t][v];
            }
        }

        // 3. Parallel Write-Back (Stable Routing)
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            size_t* my_offsets = g_multipass_offsets[thread_id];

            #pragma omp for schedule(static)
            for (size_t i = 0; i < n; ++i) {
                int element = current_data[i];
                int bucket = (element >> shift) & 0xFF;
                
                size_t dest = my_offsets[bucket]++;
                next_data[dest] = element;
            }
        }

        // 4. Ping-Pong Buffers
        std::swap(current_data, next_data);
    }

    // If the final sorted data ended up in the aux buffer, copy it back to the original vector
    if (current_data == g_aux_array) {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            data[i] = g_aux_array[i];
        }
    }
}
