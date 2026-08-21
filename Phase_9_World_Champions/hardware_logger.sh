#!/bin/bash
PROCESS_NAME="benchmark_world_champions"
LOG_FILE="HARDWARE_TELEMETRY.csv"

echo "Timestamp,CPU_Percent,RAM_MB,Virtual_Memory_MB" > $LOG_FILE
echo "=========================================="
echo " Linux Hardware Logger Initialized"
echo " Tracking Process: $PROCESS_NAME"
echo " Output File: $LOG_FILE"
echo "=========================================="
echo "Waiting for process to start..."

while true; do
    # Find the Process ID
    PID=$(pgrep -f $PROCESS_NAME | head -n 1)
    
    if [ ! -z "$PID" ]; then
        TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
        
        # Get CPU% and RSS (Physical RAM) in KB
        STATS=$(ps -p $PID -o %cpu,rss,vsz | tail -n 1)
        CPU=$(echo $STATS | awk '{print $1}')
        RAM_KB=$(echo $STATS | awk '{print $2}')
        VM_KB=$(echo $STATS | awk '{print $3}')
        
        # Convert KB to MB
        RAM_MB=$(echo "scale=2; $RAM_KB / 1024" | bc 2>/dev/null)
        VM_MB=$(echo "scale=2; $VM_KB / 1024" | bc 2>/dev/null)
        
        if [ ! -z "$RAM_MB" ]; then
            echo "$TIMESTAMP,$CPU,$RAM_MB,$VM_MB" >> $LOG_FILE
            echo "[$TIMESTAMP] CPU: $CPU% | RAM: $RAM_MB MB | VM: $VM_MB MB"
        fi
    fi
    sleep 1
done
