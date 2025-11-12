# BainosNighteyeFix - Copy files to mod directory

$modDestPath = "E:\Lorerim\mods\BainosNighteyeMod"
$localDestPath = "mod"

# Define source and destination pairs
$fileCopies = @(
    @{
        Source = "out\BainosNighteyeFix.dll"
        Dest = "$modDestPath\SKSE\plugins\BainosNighteyeFix.dll"
    },
    @{
        Source = "mod\Root\enbseries\enbeffect.fx"
        Dest = "$modDestPath\Root\enbseries\enbeffect.fx"
    },
    @{
        Source = "mod\SKSE\plugins\ENBNighteyeFix.ini"
        Dest = "$modDestPath\SKSE\plugins\ENBNighteyeFix.ini"
    },
    @{
        Source = "out\BainosNighteyeFix.dll"
        Dest = "$localDestPath\SKSE\plugins\BainosNighteyeFix.dll"
    }
)

# Ensure destination directories exist
Write-Host "Ensuring destination directories exist..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path "$modPath\Scripts" | Out-Null
New-Item -ItemType Directory -Force -Path "$modPath\SKSE\plugins" | Out-Null

# Copy files
foreach ($copy in $fileCopies) {
    if (Test-Path $copy.Source) {
        Copy-Item -Path $copy.Source -Destination $copy.Dest -Force
        Write-Host "✓ Copied: $($copy.Source) -> $($copy.Dest)" -ForegroundColor Green
        ls $copy.Dest | Select-Object Name, LastWriteTime
    } else {
        Write-Host "✗ Source not found: $($copy.Source)" -ForegroundColor Red
    }
}

Write-Host "`nDone!" -ForegroundColor Cyan
