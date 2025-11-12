# ENBNighteyeFix - Package mod for distribution

$ErrorActionPreference = "Stop"

# Paths
$dllSource = "out\BainosNighteyeFix.dll"
$iniSource = "mod\SKSE\plugins\ENBNighteyeFix.ini"
$enbEffectFx = "mod\Root\enbseries\enbeffect.fx"
$enbEffectIni = "mod\Root\enbseries\enbeffect.fx.ini"
$outputDir = "dist"
$tempDir = "temp_package"

# Variants
$variants = @(
    @{
        Name = "ENBNighteyeFix-RudyENB-patch"
        IncludeENBFiles = $true
    },
    @{
        Name = "ENBNighteyeFix-NoPatch"
        IncludeENBFiles = $false
    }
)

# Validate source files exist
Write-Host "Validating source files..." -ForegroundColor Cyan
$requiredFiles = @($dllSource, $iniSource)
foreach ($file in $requiredFiles) {
    if (-not (Test-Path $file)) {
        Write-Host "ERROR: Required file not found: $file" -ForegroundColor Red
        exit 1
    }
}

if (-not (Test-Path $enbEffectFx) -or -not (Test-Path $enbEffectIni)) {
    Write-Host "WARNING: ENB files not found. RudyENB patch variant may be incomplete." -ForegroundColor Yellow
}

# Create output directory
Write-Host "Creating output directory..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

# Clean temp directory if exists
if (Test-Path $tempDir) {
    Remove-Item -Path $tempDir -Recurse -Force
}

# Package each variant
foreach ($variant in $variants) {
    $variantName = $variant.Name
    $includeENB = $variant.IncludeENBFiles

    Write-Host ""
    Write-Host "Packaging: $variantName" -ForegroundColor Green

    # Create staging directory structure
    $stagingRoot = Join-Path $tempDir $variantName
    $sksePluginsDir = Join-Path $stagingRoot "SKSE\plugins"
    $enbSeriesDir = Join-Path $stagingRoot "Root\enbseries"

    New-Item -ItemType Directory -Force -Path $sksePluginsDir | Out-Null
    New-Item -ItemType Directory -Force -Path $enbSeriesDir | Out-Null

    # Copy SKSE plugin files
    Copy-Item -Path $dllSource -Destination (Join-Path $sksePluginsDir "BainosNighteyeFix.dll")
    Copy-Item -Path $iniSource -Destination (Join-Path $sksePluginsDir "ENBNighteyeFix.ini")
    Write-Host "  [OK] Copied SKSE plugin files" -ForegroundColor Gray

    # Copy ENB files if needed
    if ($includeENB) {
        if ((Test-Path $enbEffectFx) -and (Test-Path $enbEffectIni)) {
            Copy-Item -Path $enbEffectFx -Destination (Join-Path $enbSeriesDir "enbeffect.fx")
            Copy-Item -Path $enbEffectIni -Destination (Join-Path $enbSeriesDir "enbeffect.fx.ini")
            Write-Host "  [OK] Copied ENB effect files" -ForegroundColor Gray
        }
    }
    else {
        Write-Host "  [OK] Created empty enbseries folder" -ForegroundColor Gray
    }

    # Create zip file
    $zipPath = Join-Path $outputDir "$variantName.zip"
    if (Test-Path $zipPath) {
        Remove-Item $zipPath -Force
    }

    Compress-Archive -Path $stagingRoot -DestinationPath $zipPath -CompressionLevel Optimal

    $zipInfo = Get-Item $zipPath
    $sizeKB = [math]::Round($zipInfo.Length / 1KB, 2)
    Write-Host "  [OK] Created: $($zipInfo.Name) ($sizeKB KB)" -ForegroundColor Green
}

# Cleanup
Write-Host ""
Write-Host "Cleaning up..." -ForegroundColor Cyan
Remove-Item -Path $tempDir -Recurse -Force

Write-Host ""
Write-Host "Packaging complete! Files created in '$outputDir':" -ForegroundColor Green
Get-ChildItem $outputDir -Filter "*.zip" | ForEach-Object {
    Write-Host "  - $($_.Name)" -ForegroundColor White
}
