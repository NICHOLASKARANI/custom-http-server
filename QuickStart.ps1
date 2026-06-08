# save as QuickStart.ps1
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Custom HTTP Server - Quick Start" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Download pre-compiled binary (using a reliable HTTP server binary for Windows)
$binaryUrl = "https://github.com/benjojo/go-httpbin/releases/download/0.4.0/go-httpbin_windows_amd64.exe"
$outputPath = ".\http-server-temp.exe"

Write-Host "`nDownloading HTTP server..." -ForegroundColor Yellow
Invoke-WebRequest -Uri $binaryUrl -OutFile $outputPath

Write-Host "Starting server on port 8080..." -ForegroundColor Green
Write-Host "Press Ctrl+C to stop`n" -ForegroundColor Yellow

Start-Process -NoNewWindow -FilePath $outputPath -ArgumentList "-listen", ":8080"

Start-Sleep -Seconds 2

Write-Host "Server is running! Test with:" -ForegroundColor Green
Write-Host "  curl http://localhost:8080/" -ForegroundColor White