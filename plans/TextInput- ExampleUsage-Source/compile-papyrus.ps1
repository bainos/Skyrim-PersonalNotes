Write-Host "Compiling BainosNighteyeFix Papyrus Scripts"
Write-Host "=========================================="

# Set paths
$LORERIM_PATH = "E:\Lorerim"
$COMPILER_PATH = "$LORERIM_PATH\mods\Project New Reign - Nemesis Unlimited Behavior Engine\Nemesis_Engine\Papyrus Compiler\PapyrusCompiler.exe"
$FLAGS_PATH = "$LORERIM_PATH\mods\Project New Reign - Nemesis Unlimited Behavior Engine\Nemesis_Engine\Papyrus Compiler\scripts\TESV_Papyrus_Flags.flg"
$SOURCE_PATH = "E:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\Source\Scripts;E:\Lorerim\mods\Papyrus MessageBox - SKSE NG\Source\Scripts"
$SCRIPTS_FOLDER = ".\Scripts"
$SCRIPTS_COMPILED_FOLDER = "."

# Save current location
$originalLocation = Get-Location

# Move into Scripts folder
Set-Location $SCRIPTS_FOLDER

# Compile
Write-Host "Compiling BainosNighteyeFix.psc..."
& "$COMPILER_PATH" "BainosNighteyeFix.psc" -f="$FLAGS_PATH" -i="$SOURCE_PATH" -o="$SCRIPTS_COMPILED_FOLDER"

# Check result
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile BainosNighteyeFix.psc"
    Write-Host "Check the error messages above"
    Pause
    exit 1
}

# Return to original location
Set-Location $originalLocation

Write-Host ""
Write-Host "SUCCESS: Scripts compiled!"
Write-Host "Compiled files (.pex) are in: $SCRIPTS_FOLDER"
Write-Host "Source files (.psc) are in: $SOURCE_PATH"
Write-Host ""
Write-Host "Your scripts are now ready to use in Skyrim!"
Pause