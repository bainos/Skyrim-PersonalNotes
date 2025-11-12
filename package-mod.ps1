# PersonalNotes - Package mod for distribution

$ErrorActionPreference = "Stop"

# Paths
$modDir = "mod"
$outputDir = "dist"
$modName = "PersonalNotes"
$version = "1.0.0"

# Validate mod directory exists
Write-Host "Validating mod directory..." -ForegroundColor Cyan
if (-not (Test-Path $modDir)) {
    Write-Host "ERROR: mod/ directory not found" -ForegroundColor Red
    exit 1
}

# Check for required files
$dllPath = Join-Path $modDir "SKSE\Plugins\PersonalNotes.dll"
if (-not (Test-Path $dllPath)) {
    Write-Host "ERROR: PersonalNotes.dll not found in mod/SKSE/Plugins/" -ForegroundColor Red
    exit 1
}

# Create output directory
Write-Host "Creating output directory..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

# Create zip file
$zipName = "$modName-v$version.zip"
$zipPath = Join-Path $outputDir $zipName

if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Write-Host "Packaging mod..." -ForegroundColor Cyan
Compress-Archive -Path "$modDir\*" -DestinationPath $zipPath -CompressionLevel Optimal

# Show result
$zipInfo = Get-Item $zipPath
$sizeKB = [math]::Round($zipInfo.Length / 1KB, 2)

Write-Host ""
Write-Host "Packaging complete!" -ForegroundColor Green
Write-Host "  File: $($zipInfo.Name)" -ForegroundColor White
Write-Host "  Size: $sizeKB KB" -ForegroundColor White
Write-Host "  Path: $($zipInfo.FullName)" -ForegroundColor Gray
