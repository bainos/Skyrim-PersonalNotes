# PersonalNotes - Compile Papyrus Scripts
Write-Host "Compiling PersonalNotes Papyrus Scripts" -ForegroundColor Cyan
Write-Host "========================================"

# Set paths
$LORERIM_PATH = "E:\Lorerim"
$COMPILER_PATH = "$LORERIM_PATH\mods\Project New Reign - Nemesis Unlimited Behavior Engine\Nemesis_Engine\Papyrus Compiler\PapyrusCompiler.exe"
$FLAGS_PATH = "$LORERIM_PATH\mods\Project New Reign - Nemesis Unlimited Behavior Engine\Nemesis_Engine\Papyrus Compiler\scripts\TESV_Papyrus_Flags.flg"
$SOURCE_PATH = "E:\Lorerim\mods\powerofthree's Papyrus Extender\Source\scripts;E:\Lorerim\mods\Skyrim Script Extender (SKSE64)\Scripts\Source;E:\Lorerim\mods\ConsoleUtilSSE NG\Scripts\Source;E:\Lorerim\mods\Extended Vanilla Menus\Source\Scripts;E:\Lorerim\mods\PapyrusUtil AE SE - Scripting Utility Functions\Source\Scripts;E:\Lorerim\mods\DbMiscFunctions\Source\Scripts;E:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\Source\Scripts"
$OUTPUT_FOLDER = ".\mod\Scripts"

# Ensure output directory exists
New-Item -ItemType Directory -Force -Path $OUTPUT_FOLDER | Out-Null

# Compile PersonalNotes.psc
Write-Host "Compiling PersonalNotes.psc..." -ForegroundColor Yellow
& "$COMPILER_PATH" "PersonalNotes.psc" -f="$FLAGS_PATH" -i="$SOURCE_PATH" -o="$OUTPUT_FOLDER"
#& "$COMPILER_PATH" "PersonalNotes.psc" -f="$FLAGS_PATH" -o="$OUTPUT_FOLDER"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile PersonalNotes.psc" -ForegroundColor Red
    exit 1
}

# Compile PersonalNotesNative.psc
Write-Host "Compiling PersonalNotesNative.psc..." -ForegroundColor Yellow
& "$COMPILER_PATH" "PersonalNotesNative.psc" -f="$FLAGS_PATH" -i="$SOURCE_PATH" -o="$OUTPUT_FOLDER"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile PersonalNotesNative.psc" -ForegroundColor Red
    Pause
    exit 1
}

Write-Host ""
Write-Host "SUCCESS: Scripts compiled!" -ForegroundColor Green
Write-Host "Compiled files (.pex) are in: $OUTPUT_FOLDER"
Write-Host ""
