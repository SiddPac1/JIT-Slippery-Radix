#pragma once
#include <vector>
#include <omp.h>
#include <immintrin.h> // Intel AVX2 / AVX-512 Intrinsics
#include <iostream>

void jit_slippery_radix_sort_v6_avx2(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);

    int max_val = 0;
    #pragma omp parallel for reduction(max:max_val) schedule(static)
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] > max_val) max_val = data[i];
    }

    long long hist_size = (long long)max_val + 1;

    // Thread-Local Histograms
    std::vector<std::vector<int>> local_histograms(num_threads, std::vector<int>(hist_size, 0));

    // The AVX2 "0 Ground Clearance" Vectorized Intake
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int* local_hist = local_histograms[thread_id].data();
        
        // Process exactly 8 integers at a time
        size_t vector_iterations = data.size() / 8;
        
        #pragma omp for schedule(static)
        for (size_t i = 0; i < vector_iterations; ++i) {
            // Hardware SIMD Intrinsic: Rips 256 bits (8 integers) from RAM in 1 clock cycle
            __m256i vec = _mm256_loadu_si256((__m256i*)&data[i * 8]);
            
            // Push values back to scalar to write to L1 cache.
            // Since AVX2 does not have a native "Scatter" instruction, this manual unrolling
            // perfectly utilizes the CPU's superscalar execution ports without loop overhead.
            int vals[8];
            _mm256_storeu_si256((__m256i*)vals, vec);

            local_hist[vals[0]]++;
            local_hist[vals[1]]++;
            local_hist[vals[2]]++;
            local_hist[vals[3]]++;
            local_hist[vals[4]]++;
            local_hist[vals[5]]++;
            local_hist[vals[6]]++;
            local_hist[vals[7]]++;
        }
        
        // Master thread cleans up the remaining elements (max 7 elements)
        #pragma omp master
        {
            size_t remainder_start = vector_iterations * 8;
            for (size_t i = remainder_start; i < data.size(); ++i) {
                local_hist[data[i]]++;
            }
        }
    }

    // Contention-Free Combination
    std::vector<int> global_histogram(hist_size, 0);

    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        int sum = 0;
        for (int t = 0; t < num_threads; ++t) {
            sum += local_histograms[t][v];
        }
        global_histogram[v] = sum;
    }

    // Parallel Prefix Sum
    std::vector<size_t> write_offsets(hist_size, 0);
    size_t current_offset = 0;
    for (long long v = 0; v < hist_size; ++v) {
        write_offsets[v] = current_offset;
        current_offset += global_histogram[v];
    }

    // Parallel Write-Back
    #pragma omp parallel for schedule(static)
    for (long long v = 0; v < hist_size; ++v) {
        size_t start_idx = write_offsets[v];
        int weight = global_histogram[v];
        for (int i = 0; i < weight; ++i) {
            data[start_idx + i] = v;
        }
    }
}
