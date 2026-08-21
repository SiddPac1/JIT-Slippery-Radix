#pragma once
#include <vector>
#include <omp.h>
#include <algorithm>

void bsp_parallel_radix_sort(std::vector<int>& data, int num_threads) {
    if (data.empty()) return;
    int n = data.size();
    std::vector<int> aux(n);
    std::vector<int>* src = &data;
    std::vector<int>* dst = &aux;

    omp_set_num_threads(num_threads);

    for (int shift = 0; shift < 32; shift += 8) {
        std::vector<std::vector<int>> thread_counts(num_threads, std::vector<int>(256, 0));
        
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            #pragma omp for schedule(static)
            for (int i = 0; i < n; ++i) {
                int digit = ((*src)[i] >> shift) & 0xFF;
                thread_counts[tid][digit]++;
            }
        }

        std::vector<std::vector<int>> thread_offsets(num_threads, std::vector<int>(256, 0));
        int current_offset = 0;
        for (int digit = 0; digit < 256; ++digit) {
            for (int tid = 0; tid < num_threads; ++tid) {
                thread_offsets[tid][digit] = current_offset;
                current_offset += thread_counts[tid][digit];
            }
        }

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            std::vector<int> local_offsets = thread_offsets[tid];
            #pragma omp for schedule(static)
            for (int i = 0; i < n; ++i) {
                int digit = ((*src)[i] >> shift) & 0xFF;
                int pos = local_offsets[digit]++;
                (*dst)[pos] = (*src)[i];
            }
        }
        
        std::swap(src, dst);
    }
    
    if (src != &data) {
        data = *src;
    }
}
