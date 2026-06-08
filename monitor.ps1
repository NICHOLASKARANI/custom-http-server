# monitor.ps1 - Real-time server monitoring
while ($true) {
    Clear-Host
    Write-Host "=== Custom HTTP Server Monitor ===" -ForegroundColor Cyan
    Write-Host "Time: $(Get-Date)" -ForegroundColor Yellow
    Write-Host ""
    
    # Test server health
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:8080/api/time" -UseBasicParsing -TimeoutSec 2
        $data = $response.Content | ConvertFrom-Json
        Write-Host "✓ Server Status: ONLINE" -ForegroundColor Green
        Write-Host "  Server Time: $($data.server_time)" -ForegroundColor White
    } catch {
        Write-Host "✗ Server Status: OFFLINE" -ForegroundColor Red
    }
    
    # Show recent logs
    Write-Host "`n=== Recent Requests ===" -ForegroundColor Cyan
    Get-Content -Path "python.log" -Tail 5 -ErrorAction SilentlyContinue
    
    Start-Sleep -Seconds 2
}