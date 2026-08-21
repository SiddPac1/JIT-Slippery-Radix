#pragma once
#include <vector>
#include <omp.h>
#include <iostream>
#include <immintrin.h>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <intrin.h>
#endif

inline int get_shift_amount_v7(int max_val) {
    if (max_val == 0) return 0;
    unsigned long index;
#ifdef _WIN32
    _BitScanReverse(&index, max_val);
#else
    index = 31 - __builtin_clz(max_val);
#endif
    int bits = index + 1;
    int shift = bits - 8;
    return (shift > 0) ? shift : 0;
}

void jit_slippery_radix_sort_v7_waterfall(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);

    // 1. Find Max Val
    int global_max = 0;
    #pragma omp parallel
    {
        int local_max = 0;
        #pragma omp for schedule(static)
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] > local_max) local_max = data[i];
        }
        #pragma omp critical
        {
            if (local_max > global_max) global_max = local_max;
        }
    }

    int shift = get_shift_amount_v7(global_max);
    int num_bins = (global_max >> shift) + 1;
    if (num_bins > 256) num_bins = 256;

    // Tertiary Layer: Thread-Local RAM Bins
    std::vector<std::vector<std::vector<int>>> tertiary_bins(num_threads, std::vector<std::vector<int>>(num_bins));
    
    // Pre-allocate RAM conservatively to avoid vector resizing during JIT
    size_t expected_per_bin = (data.size() / num_bins) / num_threads * 2;
    for (int t = 0; t < num_threads; ++t) {
        for (int b = 0; b < num_bins; ++b) {
            tertiary_bins[t][b].reserve(expected_per_bin);
        }
    }

    // 2. The Waterfall JIT Pipeline
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        
        // Primary Layer: L1 Cache Buffers (256 bins * 32 elements = 8 KB total)
        alignas(32) int l1_buffers[256][32];
        alignas(32) int l1_counts[256] = {0};

        #pragma omp for schedule(static)
        for (size_t i = 0; i < data.size(); ++i) {
            int element = data[i];
            int bin = element >> shift;

            // Route to Primary L1 Buffer
            int count = l1_counts[bin];
            l1_buffers[bin][count] = element;
            count++;

            if (count == 32) {
                // POP! AVX2 Flush to Tertiary RAM
                std::vector<int>& ram_bin = tertiary_bins[thread_id][bin];
                size_t old_size = ram_bin.size();
                ram_bin.resize(old_size + 32);
                int* dest = &ram_bin[old_size];
                
                // Blast 32 elements using four 256-bit AVX2 vectors
                __m256i v0 = _mm256_load_si256((const __m256i*)&l1_buffers[bin][0]);
                __m256i v1 = _mm256_load_si256((const __m256i*)&l1_buffers[bin][8]);
                __m256i v2 = _mm256_load_si256((const __m256i*)&l1_buffers[bin][16]);
                __m256i v3 = _mm256_load_si256((const __m256i*)&l1_buffers[bin][24]);
                
                _mm256_storeu_si256((__m256i*)&dest[0], v0);
                _mm256_storeu_si256((__m256i*)&dest[8], v1);
                _mm256_storeu_si256((__m256i*)&dest[16], v2);
                _mm256_storeu_si256((__m256i*)&dest[24], v3);

                count = 0;
            }
            l1_counts[bin] = count;
        }

        // Flush remaining buffers at the end of the JIT stream
        for (int b = 0; b < num_bins; ++b) {
            if (l1_counts[b] > 0) {
                std::vector<int>& ram_bin = tertiary_bins[thread_id][b];
                for (int c = 0; c < l1_counts[b]; ++c) {
                    ram_bin.push_back(l1_buffers[b][c]);
                }
            }
        }
    }

    // 3. Final Reassembly and Sort
    // We now have the data perfectly partitioned into L3 Cache sized chunks!
    size_t write_idx = 0;
    for (int b = 0; b < num_bins; ++b) {
        size_t bin_start = write_idx;
        for (int t = 0; t < num_threads; ++t) {
            std::vector<int>& ram_bin = tertiary_bins[t][b];
            if (!ram_bin.empty()) {
                memcpy(&data[write_idx], ram_bin.data(), ram_bin.size() * sizeof(int));
                write_idx += ram_bin.size();
            }
        }
        
        // Since the chunk fits in cache, std::sort will be incredibly fast (Cache-Oblivious)
        std::sort(data.begin() + bin_start, data.begin() + write_idx);
    }
}
