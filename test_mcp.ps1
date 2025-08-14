Write-Host "=== Testing MCP Plugin Server Interfaces ===" -ForegroundColor Green

$BaseURL = "http://localhost:6680"
$RepoURL = "http://localhost:6381"
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

# 3. Get plugin repository tags
Write-Host "`n[GET /tags] Retrieve plugin repository tags" -ForegroundColor Cyan
try {
    $response = Invoke-RestMethod -Uri "$RepoURL/tags" -Method Get -TimeoutSec 10
    Write-Host "✅ Successfully returned data:" -ForegroundColor Green
    if ($response.Count -gt 0) {
        foreach ($tag in $response) {
            Write-Host "  Tag: $tag"
        }
    } else {
        Write-Host "  No tags found"
    }
}
catch {
    Write-Host "❌ Request failed: $($_.Exception.Message)" -ForegroundColor Red
}

# 4. Get specific tag information
Write-Host "`n[GET /tags/{tag}] Retrieve specific tag information" -ForegroundColor Cyan
try {
    # First get a list of tags to test with
    $tags = Invoke-RestMethod -Uri "$RepoURL/tags" -Method Get -TimeoutSec 10
    if ($tags.Count -gt 0) {
        $testTag = $tags[0]
        $response = Invoke-RestMethod -Uri "$RepoURL/tags/$testTag" -Method Get -TimeoutSec 10
        Write-Host "✅ Successfully returned data for tag: $testTag" -ForegroundColor Green
        Write-Host "  Tag Name: $($response.tag_name)"
        Write-Host "  Release Name: $($response.name)"
        Write-Host "  Published At: $($response.published_at)"
        Write-Host "  Assets Count: $($response.assets.Count)"
        Write-Host "  Plugin Packages Count: $($response.plugin_packages.Count)"
    } else {
        Write-Host "  No tags available to test" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "❌ Request failed: $($_.Exception.Message)" -ForegroundColor Red
}

# 5. Process a tag (POST request)
Write-Host "`n[POST /tags/{tag}/process] Process a tag" -ForegroundColor Cyan
try {
    # First get a list of tags to test with
    $tags = Invoke-RestMethod -Uri "$RepoURL/tags" -Method Get -TimeoutSec 10
    if ($tags.Count -gt 0) {
        $testTag = $tags[0]
        $response = Invoke-RestMethod -Uri "$RepoURL/tags/$testTag/process" -Method Post -TimeoutSec 30
        Write-Host "✅ Successfully processed tag: $testTag" -ForegroundColor Green
        Write-Host "  Message: $($response.message)"
    } else {
        Write-Host "  No tags available to test" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "❌ Request failed: $($_.Exception.Message)" -ForegroundColor Red
}

# 6. Start periodic scan (POST request)
Write-Host "`n[POST /scan/start] Start periodic scan" -ForegroundColor Cyan
try {
    $response = Invoke-RestMethod -Uri "$RepoURL/scan/start" -Method Post -TimeoutSec 10
    Write-Host "✅ Successfully started periodic scan" -ForegroundColor Green
    Write-Host "  Message: $($response.message)"
}
catch {
    Write-Host "❌ Request failed: $($_.Exception.Message)" -ForegroundColor Red
}

# 7. Stop periodic scan (POST request)
Write-Host "`n[POST /scan/stop] Stop periodic scan" -ForegroundColor Cyan
try {
    $response = Invoke-RestMethod -Uri "$RepoURL/scan/stop" -Method Post -TimeoutSec 10
    Write-Host "✅ Successfully stopped periodic scan" -ForegroundColor Green
    Write-Host "  Message: $($response.message)"
}
catch {
    Write-Host "❌ Request failed: $($_.Exception.Message)" -ForegroundColor Red
}