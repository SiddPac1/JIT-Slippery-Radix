param (
    [string]$ProcessName = "benchmark_runner_v6_avx2",
    [string]$LogFile = "HARDWARE_TELEMETRY.csv",
    [int]$IntervalSeconds = 1
)

$header = "Timestamp,CPU_Percent,Physical_RAM_MB,Virtual_Memory_MB"
$header | Out-File -FilePath $LogFile -Encoding utf8

Write-Host "=========================================="
Write-Host " Hardware Logger Initialized"
Write-Host " Tracking Process: $ProcessName"
Write-Host " Output File: $LogFile"
Write-Host " Interval: Every $IntervalSeconds second(s)"
Write-Host "=========================================="
Write-Host "Waiting for process to start..."

while ($true) {
    $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
    
    if ($proc) {
        $timestamp = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
        
        # Get live CPU % using CIM Instance (Hardware Performance Counters)
        $cpuPercent = 0.00
        $cim = Get-CimInstance Win32_PerfFormattedData_PerfProc_Process -Filter "IDProcess = $($proc.Id)" -ErrorAction SilentlyContinue
        if ($cim) {
            # Windows scales CPU time by core count, so we divide by total logical processors to get 0-100%
            $cpuPercent = [math]::Round($cim.PercentProcessorTime / $env:NUMBER_OF_PROCESSORS, 2)
        }
        
        # WorkingSet64 = Physical RAM locked/used by the process
        $ramMB = [math]::Round($proc.WorkingSet64 / 1MB, 2)
        
        # VirtualMemorySize64 = Total Memory allocated (including paged)
        $vmMB = [math]::Round($proc.VirtualMemorySize64 / 1MB, 2)
        
        $logLine = "$timestamp,$cpuPercent,$ramMB,$vmMB"
        $logLine | Out-File -FilePath $LogFile -Append -Encoding utf8
        
        Write-Host "[$timestamp] CPU: $cpuPercent% | RAM: $ramMB MB | VM: $vmMB MB"
    }
    
    Start-Sleep -Seconds $IntervalSeconds
}
