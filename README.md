# Just-In-Time (JIT) Slippery Radix Sort
**The World's Fastest Integer Sorting Algorithm for Localized Domains (1.51 Billion items/sec)**

This repository contains the evolutionary C++ architecture and raw hardware telemetry for the **JIT Slippery Radix Sort**. Over the course of 9 architectural phases, this project successfully mapped the physical limits of modern Silicon routing, ultimately utilizing `VirtualLock` OS-bypassing and AVX2 Branchless vectorization to achieve world-record throughput on consumer hardware.

## ?? The Evolutionary Architecture (Phases 1-9)
The repository is strictly organized sequentially to preserve the chronological research and development of the algorithm.

1. `Phase_1_2_3_Baseline/`: The original iterative tests and the catastrophic O(N^2) Mutex failure.
2. `Phase_4_Mutex_Locks/`: The transition to a lock-free Global Atomic pipeline using OpenMP.
3. `Phase_5_ThreadLocal/`: Eradicating L1 Cache-Line False Sharing via Thread-Local memory pools.
4. `Phase_6_AVX2_Tiered/`: The `VirtualLock` memory pin and L3 Silicon Cache 1-byte compression.
5. `Phase_7_Waterfall/`: Experimental Cache-Oblivious trials.
6. `Phase_8_AVX2_MultiPass/`: The 1-Kilobyte sliding window for massive, uniform data domains.
7. `Phase_9_World_Champions/`: The final C++ testing arena, pitting the V6 AVX2 architectures against `std::sort` and BSP Radix at Exascale boundaries.

## Raw Telemetry and Data Availability
All empirical benchmarking was executed using the Google Benchmark framework. To prove algorithmic independence from standard OS paging limits, custom Windows Kernel Hardware Loggers were developed. 

You can find the exact hardware telemetry for the 100-Million and 1-Billion element Exascale runs in the `Phase_9_World_Champions/` directory:
- `FINAL_FULL_SUITE_RUN.txt`: The Google Benchmark CLI output proving 1.51 Billion items/sec.
- `ADVANCED_HARDWARE_LOG.csv`: The kernel telemetry proving exactly 0 OS Page Faults during execution.
- `EXASCALE_HARDWARE_LOG.csv`: The kernel log documenting the 34 GB swap-file explosion and OOM limit of `VirtualLock`.

## How to Run
The final architecture is located in `Phase_9_World_Champions/`.
1. Ensure you have CMake and an AVX2-compatible C++ compiler (MSVC or GCC/Clang).
2. Configure the project: `cmake -B build -S .`
3. Build the executable in Release mode: `cmake --build build --config Release`
4. Run the generated executable. (Note: Ensure your system has sufficient physical RAM to support the `VirtualLock` allocations, otherwise the kernel will trigger an OOM panic).

## Publications
This code repository serves as the official Data and Code Availability archive for the corresponding JIT Slippery Radix IEEE Research Papers published on ResearchGate. The Research and Code is subject to copyright by the Author. It can be used for academic study but reproducing and using it for further work requires the permission from the author. 
