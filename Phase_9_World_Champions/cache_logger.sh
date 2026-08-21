#!/bin/bash
PROCESS_NAME="benchmark_world_champions"

echo "=========================================="
echo " Linux Cache & Branch Logger Initialized"
echo "=========================================="
echo "This script uses the Linux 'perf' tool to track silicon-level metrics."
echo "You MUST run this script with 'sudo' on GCP!"
echo "Waiting for process to start..."

while true; do
    # Find the Process ID
    PID=$(pgrep -f $PROCESS_NAME | head -n 1)
    
    if [ ! -z "$PID" ]; then
        echo "Found Process $PID! Hooking into hardware performance counters..."
        
        # Log L1 Misses, L3 (LLC) Misses, and Branch Mispredictions every 1 second (1000ms)
        perf stat -p $PID -I 1000 -e L1-dcache-load-misses,LLC-load-misses,LLC-store-misses,branch-misses,page-faults -o CACHE_TELEMETRY.txt
        break
    fi
    sleep 1
done
