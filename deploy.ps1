# PersonalNotes - Deploy to mod directory

$localDestPath = "mod"

# Define source and destination pairs
$fileCopies = @(
    @{
        Source = "out\PersonalNotes.dll"
        Dest = "$localDestPath\SKSE\Plugins\PersonalNotes.dll"
    },
    @{
        Source = "PersonalNotes.psc"
        Dest = "$localDestPath\Source\Scripts\PersonalNotes.psc"
    },
    @{
        Source = "PersonalNotesNative.psc"
        Dest = "$localDestPath\Source\Scripts\PersonalNotesNative.psc"
    }
)

# Ensure destination directories exist
Write-Host "Ensuring destination directories exist..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path "$localDestPath\SKSE\Plugins" | Out-Null
New-Item -ItemType Directory -Force -Path "$localDestPath\Scripts" | Out-Null

# Copy files
foreach ($copy in $fileCopies) {
    if (Test-Path $copy.Source) {
        Copy-Item -Path $copy.Source -Destination $copy.Dest -Force
        Write-Host "Copied: $($copy.Source) -> $($copy.Dest)" -ForegroundColor Green
        Get-ChildItem $copy.Dest | Select-Object Name, LastWriteTime
    } else {
        Write-Host "Source not found: $($copy.Source)" -ForegroundColor Red
    }
}

Write-Host "Done!" -ForegroundColor Cyan
