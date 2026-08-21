#pragma once
#include <vector>
#include <omp.h>
#include <iostream>
#include <cstring>

void jit_slippery_radix_sort_v5_multipass(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;

    omp_set_num_threads(num_threads);
    size_t n = data.size();
    
    // We need an auxiliary array for the out-of-place stable writes
    std::vector<int> aux(n);
    int* current_data = data.data();
    int* next_data = aux.data();

    // 4 passes for a 32-bit integer (8 bits per pass)
    for (int shift = 0; shift < 32; shift += 8) {
        
        // 1. Thread-Local Histograms (pure L1 Cache)
        // 256 buckets per thread. Size = 1 KB per thread!
        std::vector<std::vector<int>> local_histograms(num_threads, std::vector<int>(256, 0));

        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int* local_hist = local_histograms[thread_id].data();

            // OpenMP chunking ensures each thread reads a specific portion of the array
            #pragma omp for schedule(static)
            for (size_t i = 0; i < n; ++i) {
                int bucket = (current_data[i] >> shift) & 0xFF;
                local_hist[bucket]++;
            }
        }

        // 2. Compute 2D Prefix Sums
        // We need exact target offsets for every Thread for every Bucket.
        // This is done sequentially. Since there are only 256 buckets, it's virtually instantaneous.
        std::vector<std::vector<size_t>> thread_offsets(num_threads, std::vector<size_t>(256, 0));
        size_t current_offset = 0;
        
        for (int v = 0; v < 256; ++v) {
            for (int t = 0; t < num_threads; ++t) {
                thread_offsets[t][v] = current_offset;
                current_offset += local_histograms[t][v];
            }
        }

        // 3. Parallel Write-Back (Stable Routing)
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            size_t* my_offsets = thread_offsets[thread_id].data();

            // Must use exactly the same chunking as the counting phase to maintain stability!
            #pragma omp for schedule(static)
            for (size_t i = 0; i < n; ++i) {
                int element = current_data[i];
                int bucket = (element >> shift) & 0xFF;
                
                // Write directly to the EXACT non-overlapping target destination
                size_t dest = my_offsets[bucket]++;
                next_data[dest] = element;
            }
        }

        // 4. Ping-Pong Buffers
        std::swap(current_data, next_data);
    }

    // If the final sorted data ended up in the aux buffer, copy it back to the original vector
    // (With 4 passes, it actually ends up back in original data buffer anyway, but this is safe)
    if (current_data == aux.data()) {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            data[i] = aux[i];
        }
    }
}
