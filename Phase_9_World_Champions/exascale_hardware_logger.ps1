$logFile = "EXASCALE_HARDWARE_LOG.csv"
"Timestamp,CPU_Usage(%),Private_Memory(MB),Page_Faults_Per_Sec" | Out-File -FilePath $logFile

Write-Host "Starting Exascale Hardware Logger..."
Write-Host "Logging to $logFile"

# Get the initial page fault count to calculate per-second rate
$prevPageFaults = (Get-Process benchmark_world_champions -ErrorAction SilentlyContinue).PageFaults
$prevTime = Get-Date

while ($true) {
    Start-Sleep -Seconds 1
    $proc = Get-Process benchmark_world_champions -ErrorAction SilentlyContinue
    
    if ($proc) {
        $currentTime = Get-Date
        $cpu = [math]::Round($proc.CPU, 2)
        $memMB = [math]::Round($proc.PrivateMemorySize64 / 1MB, 2)
        
        $currentPageFaults = $proc.PageFaults
        $timeDiff = ($currentTime - $prevTime).TotalSeconds
        $pfPerSec = 0
        if ($prevPageFaults -ne $null -and $timeDiff -gt 0) {
            $pfPerSec = [math]::Round(($currentPageFaults - $prevPageFaults) / $timeDiff, 2)
        }
        
        $prevPageFaults = $currentPageFaults
        $prevTime = $currentTime
        
        $logLine = "$($currentTime.ToString('HH:mm:ss')),$cpu,$memMB,$pfPerSec"
        $logLine | Out-File -FilePath $logFile -Append
        Write-Host "Logged: CPU: $cpu`% | RAM: $memMB MB | Page Faults/sec: $pfPerSec"
    } else {
        # Process not running yet or finished
    }
}
