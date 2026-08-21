# Run this script to download the world champion sorting algorithms for the benchmark suite!

Write-Host "Downloading IPS^4o (In-Place Parallel Super Scalar Samplesort)..."
git clone https://github.com/ips4o/ips4o.git World_Champions_Pipeline/ips4o

Write-Host "Downloading IPS^2Ra (In-Place Parallel Super Scalar Radix Sort)..."
git clone https://github.com/ips4o/ips2ra.git World_Champions_Pipeline/ips2ra

Write-Host "Downloading Google Highway (Regions Sort)..."
git clone https://github.com/google/highway.git World_Champions_Pipeline/highway

Write-Host "Done! You can now compile the World Champions benchmark on GCP!"
