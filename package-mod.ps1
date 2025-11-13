# Get the directory this script is running from
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

# Path to the other script
$DeployScript = Join-Path $ScriptDir 'deploy.ps1'

# Run deploy.ps1 if it exists
if (Test-Path $DeployScript) {
    Write-Host "Running pre-deploy script: $DeployScript" -ForegroundColor Cyan
    & $DeployScript
    Write-Host "Pre-deploy script completed." -ForegroundColor Green
} else {
    Write-Host "Warning: deploy.ps1 not found in $ScriptDir" -ForegroundColor Yellow
}

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
