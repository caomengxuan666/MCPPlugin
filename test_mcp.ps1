Write-Host "=== Testing MCP Plugin Server Interfaces ===" -ForegroundColor Green

$BaseURL = "http://localhost:6680"
$tempFile = [System.IO.Path]::GetTempFileName() + ".zip"  # Generate temporary file path

# 1. Get latest version information
Write-Host "`n[GET /self/latest/info] Retrieve latest version information" -ForegroundColor Cyan
try {
    $response = Invoke-RestMethod -Uri "$BaseURL/self/latest/info" -Method Get -TimeoutSec 10
    Write-Host "✅ Successfully returned data:" -ForegroundColor Green
    Write-Host "  Tag: $($response.tag_name)"
    Write-Host "  Name: $($response.name)"
    Write-Host "  Published At: $($response.published_at)"
    foreach ($asset in $response.assets) {
        Write-Host "  Asset: $($asset.name) [$($asset.platform)]"
    }
}
catch {
    Write-Host "❌ Request failed: $($_.Exception.Message)" -ForegroundColor Red
}

# 2. Download Windows version plugin package
Write-Host "`n[GET /self/latest/download/windows] Test downloading Windows package" -ForegroundColor Cyan
try {
    $downloadURL = "$BaseURL/self/latest/download/windows"
    # Use -OutFile parameter to specify output file, resolving PassThru parameter dependency
    $response = Invoke-WebRequest -Uri $downloadURL -Method Get -TimeoutSec 15 `
                                 -OutFile $tempFile -PassThru
    
    if ($response.StatusCode -eq 200) {
        # Get file size
        $fileInfo = Get-Item $tempFile
        $fileSize = $fileInfo.Length
        
        Write-Host "✅ Received 200 response" -ForegroundColor Green
        Write-Host "  File size: $fileSize bytes"
        Write-Host "  Temporary save path: $tempFile"
        
        # Optional: Delete temporary file after download
        # Remove-Item $tempFile -Force
    } else {
        Write-Host "❌ Unexpected status code: $($response.StatusCode)" -ForegroundColor Red
    }
}
catch {
    Write-Host "❌ Download failed: $($_.Exception.Message)" -ForegroundColor Red
}
    